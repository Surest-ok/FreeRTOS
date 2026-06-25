# FreeRTOS SMP Unikernel Architecture

## Goal

The image is a single-purpose SMP unikernel for QEMU virt AArch64:

1. one linked ELF contains startup, BSP, FreeRTOS SMP kernel, and the application;
2. `CORES=N` selects the scheduler shape at compile time;
3. boot, panic handling, and scheduler observability are part of the runtime, not ad-hoc demo code.

## Runtime Layers

```
kernel-$(CORES).elf
├── startup.S
│   ├── EL2/EL1 setup
│   ├── BSS clear
│   ├── per-core exception stack
│   └── mailbox release for secondary cores
├── BSP
│   ├── GICv3 distributor/redistributor
│   ├── generic timer
│   └── PL011 console
├── FreeRTOS SMP
│   ├── pxCurrentTCBs[CORES]
│   ├── CAS raw spinlock + task/ISR recursive spinlocks
│   ├── SGI 0 cross-core yield
│   ├── active/idle CPU masks
│   └── per-core interrupt/yield/FPU state
├── ukernel runtime
│   ├── ukernel_start()
│   ├── panic/assert/malloc/stack hooks
│   ├── secondary-core release
│   ├── UART serialization through RawSpinlock_t
│   └── per-core scheduler heartbeat
└── application
    ├── ukernel_app_init()
    ├── pinned scheduler workers
    └── core0 monitor
```

## Build Contract

`CORES` is part of the ABI between the FreeRTOS configuration, BSP, and linker script.

```bash
make run             # builds/runs bsp/kernel-2.elf with -smp 2
make CORES=32 all    # builds bsp/kernel-32.elf with 32-core scheduler state
make CORES=32 run    # runs -smp 32; QEMU TCG may need a longer timeout
```

Build output is isolated by core count:

| Component | Output |
|-----------|--------|
| Kernel archive | `kernel/build-$(CORES)/libfreertos.a` |
| BSP objects | `bsp/build-$(CORES)/` |
| Bootable ELF | `bsp/kernel-$(CORES).elf` |

This prevents a 32-core FreeRTOS object set from being linked into a 2-core QEMU run.

## Scheduler Productization

The old demo used raw UART writes from worker tasks. That proved the port could run, but it was not useful as a product signal: output interleaved and task placement was implicit.

The runtime now creates one worker per configured core using `xTaskCreateAffinitySet()`. Each worker is pinned to exactly one core and updates a per-core heartbeat without printing. A core0 monitor owns scheduler reporting:

```text
[sched] active=0x00000003 idle=0x00000000 seen=0x00000003 cores=2 hb0=3993 hb1=8396
```

The `active` mask records cores that have completed local port/GIC/SGI initialization. Cross-core yield IPIs are sent only to active non-local targets. The `idle` mask is updated on context switch and catches the corner case where a core is online but stuck in an idle task. The `seen` mask is the application scheduler liveness contract. For 2 cores, `0x3` means core 0 and core 1 both executed application tasks. For 32 cores, the low 32 bits represent all configured cores.

UART output is serialized with `RawSpinlock_t`, a CAS lock exported by the SMP port. Task and ISR kernel locks remain recursive `Spinlock_t`. This keeps console serialization independent from FreeRTOS internal lock ownership while using the same acquire/release memory ordering style.

## FreeRTOS SMP Scheduler Corner Cases

The runtime exposes these FreeRTOS SMP corner cases directly in the monitor output:

1. Do not send yield IPI to a CPU that has not completed per-core initialization.
2. Distinguish "CPU is online" from "CPU is running application work".
3. Filter remote reschedule targets: local CPU handles yield locally; inactive CPUs do not receive SGI.
4. Keep FreeRTOS run-state semantics intact: if FreeRTOS marks a task/core as scheduled-to-yield during startup, that state must not be skipped just because the IPI is filtered while the target CPU is inactive.
5. Wake pinned work narrowly and report aggregate state from one monitor task instead of letting all workers print to UART.

## 32-Core Constraints

The 32-core path depends on these invariants:

1. Linear FreeRTOS core id is `Aff1 * 16 + Aff0`.
2. GICv3 SGI uses `ICC_SGI1R_EL1` with `Aff1` and a 16-bit `TargetList`.
3. Scheduler masks must use `UBaseType_t`, never `(1U << configNUMBER_OF_CORES)` at `CORES=32`.
4. Linker script reserves 32 EL1 stacks and 32 mailbox slots.
5. QEMU TCG is slow with 32 vCPUs; short runs prove boot and partial scheduling, not full throughput.

## Extension Points

Applications should replace `bsp/src/app.c` and keep `ukernel_start()` unchanged. Required application entry:

```c
int ukernel_app_init(const UkernelBootInfo *boot);
```

Runtime services exported to applications:

```c
void ukernel_panic(const char *reason);
void ukernel_note_core_alive(uint32_t core_id);
uint64_t ukernel_core_seen_mask(void);
uint64_t ukernel_core_heartbeat(uint32_t core_id);
```

This keeps boot, scheduler startup, and failure handling consistent across unikernel workloads.
