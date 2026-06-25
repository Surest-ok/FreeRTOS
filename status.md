# SMP Kernel Scheduler / ARCH Status

Date: 2026-06-14

## Current conclusion

FreeRTOS SMP scheduler logic is treated as the trusted baseline. The recent
failures were caused by local port/integration issues and by an overly coupled
acceptance topology, not by a proven defect in the upstream scheduler model.

## Fixed / changed

- Added a shared ARCH layer under `arch/` for AArch64
  primitives: CPU id / MPIDR mapping, IRQ save/restore, atomics, barriers,
  raw/recursive spinlocks, vector/exception/syscall/context placeholders, and
  a common thread handle shape.
- Migrated the FreeRTOS SMP port spinlock and core-id paths onto the shared
  ARCH layer while keeping the upstream scheduler model unchanged.
- Added `arch_thread_handle_t` as the shared thread handle view. FreeRTOS TCBs
  now carry `xArchThread`; LK `thread_t` now carries `arch_handle`. The native
  scheduler handles remain unchanged (`TaskHandle_t` and `thread_t *`).
- Extended the shared thread handle from create-time metadata to runtime state:
  FreeRTOS now updates the handle on scheduler start, affinity changes, and
  `vTaskSwitchContext()`; LK updates it through thread CPU setters, pinned CPU
  changes, idle-thread setup, and `thread_resched()`.
- Added `arch_thread_context_switch()` as the common context-switch observation
  hook. It is wired into both FreeRTOS and LK but still delegates actual
  register save/restore to each kernel's native assembly path.
- Added common ARCH IRQ and exception state:
  `arch_irq_register()`, `arch_irq_dispatch()`, IRQ enter/exit counters, and
  `arch_exception_record()`.
- Routed the FreeRTOS BSP IRQ registration/dispatch through the common ARCH IRQ
  table. Existing device handlers still use the BSP `app_irq_install()` API,
  but the backing table is now kernel-neutral.
- Wired LK GICv3 IRQ entry/exit into the common ARCH IRQ accounting and added a
  fallback `arch_irq_dispatch()` path when LK has no native handler for a
  vector. Native LK interrupt registration remains intact.
- Wired LK synchronous exception fatal path into `arch_exception_record()` so
  the shared layer records ESR/ELR/SPSR/FAR before panic.
- Moved FreeRTOS fatal synchronous exception handling out of the assembly
  infinite loop into `vPortFatalException()`. The C path records
  ESR/ELR/SPSR/FAR through `arch_exception_record()` and emits a Linux-style
  `[panic]` marker before halting.
- Unified vector installation through `arch_vector_install()` for both
  FreeRTOS and LK. FreeRTOS now calls it from `vPortRestoreTaskContext()`, and
  LK calls it from `arm64_early_init_percpu()`.
- Added an LK mainline-based workspace wrapper under `lk/`, built from the
  upstream `origin/master` snapshot instead of the local dirty LK worktree.
- Added an LK scheduler matrix app that emits the same `[sched-verify]` and
  `[xsync-verify]` markers as the FreeRTOS BSP, covering 32-core CPU coverage,
  priority scheduling, RR progress, semaphore, message queue/port, mutex, and
  spinlock behavior.
- Fixed LK/QEMU 32-core adaptation issues in the port layer:
  PSCI CPU_ON now uses the ARCH linear-CPU-to-MPIDR mapping, and GICv3 SGI
  sends are split by Aff1 target-list groups instead of addressing only the
  low 16 CPUs.
- Replaced LK GICv3 ICC system-register aliases with explicit S3 encodings so
  the current AArch64 cross toolchain can assemble the mainline wrapper.
- Added top-level LK validation targets:
  `lk-kernel`, `lk-sched-full`, `lk-sched-cross`, `lk-sched-smp`, and
  `arch-sched-matrix`.
- Generalized the QEMU acceptance script wording and fatal marker checks so it
  can validate either FreeRTOS or LK images through the same parser.
- Stabilized the LK priority probe. LK priority ordering is still exercised by
  using different priority values, but the marker no longer depends on a
  single first-writer race under 32-core SMP pressure.
- Removed the experimental `vTaskDelete()` path that directly called
  `vTaskSwitchContext()` and `vPortRestoreTaskContext()`.
- Kept current-task self-delete on the standard SMP pending-yield path.
- Kept the local `prvCheckForRunStateChange()` yield assist needed by this
  AArch64/GIC port.
- Split basic acceptance from cross-core synchronization stress:
  priority, RR, semaphore, queue, mutex, and spinlock probes now run on one
  stable data core; low-priority workers still cover all secondary cores.
- Added `SCHED_MODE=cross` / `make sched-cross` as an isolated cross-core
  synchronization stress mode. It compiles a separate BSP object directory for
  the selected scheduler mode, avoiding stale `app.o` when switching between
  full and cross.
- BSP ELF outputs are now also mode-specific
  (`kernel-<cores>-<mode>-log<level>-tb<tick-broadcast>.elf`), and acceptance
  passes the matching `--elf`. This prevents full/cross or tick-broadcast
  policy runs from accidentally reusing the previous image.
- Added product-style `LOG_LEVEL`.
  Default builds use `LOG_LEVEL=6`, which keeps the extended scheduler/queue
  stage writes and verbose UART diagnostics out of the product path.
  `LOG_LEVEL=7` enables debug-level SMP diagnostics and routes them through the
  same Linux-style log format.
- Compiled low-level SVC/abort/IRQ-switch diagnostic counters out of the
  default product image. They are now present only when
  `configPORT_SMP_DIAGNOSTICS=1` (`LOG_LEVEL=7`).
- Unified UART logs to a Linux-like format:
  `<level>[    0.000000] freertos: ...`. Acceptance markers remain embedded in
  the message body, for example `[sched-verify]` and `[xsync-verify]`.
- Added `make sched-smp` to run full and cross validation from one command.
  `make sched-matrix` remains as a compatibility alias.
- Added `TICK_BROADCAST` as a product build option. The default remains
  `TICK_BROADCAST=1`; `TICK_BROADCAST=0` builds into separate `tb0` kernel/BSP
  outputs for A/B validation.
- Fixed Makefile dependencies so public/kernel/BSP headers force object
  rebuilds, and BSP relinks when `libfreertos.a` changes.
- Fixed BSP relink dependency on `linker.ld`.
- Fixed the per-core EL1 exception stack setup: `stack_top` is the low address
  of the reserved stack area, so SP_EL1 must be set to the high address of each
  core slot.
- Fixed the 32-core primary tick check in `gtimer.c`: it now uses the linear
  core id instead of Aff0-only MPIDR matching, so core 16 is no longer treated
  as a primary core.
- Published secondary-core mailbox entries with release semantics before `SEV`.
- Kept scheduler probe task stack at 512 words; the earlier 1024-word increase
  was not required after the port-layer fixes and could increase heap pressure.
- Changed cross-core synchronization stress to use separate core pairs for
  semaphore, queue, mutex, and spinlock probes instead of coupling all probes on
  cores 2/3.
- Cross-core stress no longer creates the background per-core worker tasks.
  Full mode still validates 32-core worker coverage; cross mode now isolates the
  semaphore/queue/mutex/spin primitives from unrelated worker scheduling noise.
- Standardized `portEND_SWITCHING_ISR()` / `portYIELD_FROM_ISR()` for SMP so ISR
  yield requests update `ulPortYieldRequireds[portGET_CORE_ID()]` instead of the
  legacy single-core flag.
- Made `taskCHECK_FOR_STACK_OVERFLOW()` SMP-safe for this port by reading
  `pxCurrentTCBs[portGET_CORE_ID()]` directly instead of routing through
  `xTaskGetCurrentTaskHandle()` while scheduler locks are held.
- Made `tasks.c` internal `pxCurrentTCB` resolve directly to the per-core TCB
  array in SMP builds. The public `xTaskGetCurrentTaskHandle()` API remains
  unchanged.
- Masked IRQs at SVC scheduler entry before saving context and calling
  `vTaskSwitchContext()`, avoiding IRQ/SGI nesting inside the SVC switch path.
- Restored ICC_PMR_EL1 from the current task's TCB critical nesting state in
  `portRESTORE_CONTEXT`. Tasks restored with zero critical nesting run
  unmasked; tasks restored while nested keep the FreeRTOS API interrupt mask.
- Hardened the recursive task/ISR spinlock owner/count updates with atomic
  acquire/release operations.
- Enabled SMP tick broadcast by default so the primary tick can nudge active
  secondary cores through the SGI yield path. It is now configurable with
  `TICK_BROADCAST=0/1`.
- Reduced acceptance-log forwarding so failing stress tests respect their
  timeout and print a concise final marker.
- Removed default diagnostic-only UART lines from the product validation path.
  Full mode now emits `[sched-verify]`; cross mode emits `[xsync-verify]`.
- Verified that the default `kernel-32-full-log6-tb1.elf` does not contain
  `ullPortSvc*`, `ullPortSyncAbort*`, or `ullPortIrqSwitches` symbols, while
  the `LOG_LEVEL=7` debug image still contains them.
- Re-enabled `configASSERT()` through the existing `vAssertCalled()` panic/log
  path. Assertions now emit `[assert]` followed by `[panic]` instead of being
  compiled out.
- Added a Luna-style root `build.yaml` describing `arch_api`, `freertos_smp`,
  `lk_smp`, and `threadx_smp` as components with a shared scheduler test
  matrix. The Makefile remains the executable frontend for now.
- Added a ThreadX workspace wrapper under `threadx/` using the Luna ecosystem
  source at `/mnt/d/workspace/luna-workspace/other-os/threadx-master` without
  copying the whole external tree into this repo.
- Added ThreadX ARCH adaptation:
  `TX_THREAD_USER_EXTENSION` now carries a generic ARCH-thread pointer,
  `threadx/port/threadx_arch.c` owns the backing `arch_thread_handle_t` pool,
  and `TX_THREAD_CREATE_INTERNAL_EXTENSION()` initializes the common handle at
  ThreadX thread creation time.
- Started replacing ThreadX AArch64 low-level scheduler support with C code
  backed by the standard `arch_api` layer. `threadx/port/threadx_lowlevel.c`
  now provides C implementations for stack-frame construction, current core
  lookup, interrupt disable/restore/control, SMP protect/unprotect, trace time,
  current-state/current-thread getters, low-level SMP initialization, and the
  secondary-core scheduler wait path.
- Reworked ThreadX C `_k_thread_smp_protect()` from a plain CAS/TAS
  competition lock into a FIFO-style wait-list lock using the external
  ThreadX port's existing `_k_thread_smp_protect_wait_list`,
  `_k_thread_smp_protect_wait_counts`, head/tail, and wait-list guard globals.
  Recursive ownership and the original `preempt_disable` delayed-release
  behavior are preserved, while contending CPUs can no longer repeatedly
  bypass the current queue head under SMP pressure.
- Added dedicated ARCH APIs for special architecture operations needed by this
  work:
  `arch_api/stack.h` exposes `arch_current_stack_pointer()` and
  `arch_stack_align_down()`, and `arch_api/ipi.h` exposes `arch_ipi_send()`
  plus `arch_ipi_send_reschedule()` for future SGI/IPI routing
  standardization. `arch_api/arch.h` now also exposes CPU hint/event helpers:
  `arch_cpu_relax()`, `arch_cpu_wait_for_interrupt()`, and
  `arch_cpu_send_event()`.
- Removed the matching ThreadX AArch64 assembly files from the workspace
  wrapper build:
  `k_thread_stack_build.S`, `k_thread_smp_core_get.S`,
  `k_thread_smp_time_get.S`, `k_thread_smp_current_state_get.S`,
  `k_thread_smp_current_thread_get.S`, `k_thread_smp_low_level_initialize.S`,
  `k_thread_smp_initialize_wait.S`, `k_thread_interrupt_disable.S`,
  `k_thread_interrupt_restore.S`, `k_thread_interrupt_control.S`,
  `k_thread_smp_protect.S`, and `k_thread_smp_unprotect.S`.
- Preserved ThreadX hard context mechanics in assembly for now:
  boot/vector entry, context save/restore, scheduler `eret` handoff,
  system-return handoff, timer interrupt entry, secondary-core wait, and
  SGI preempt send. These are currently treated as ARCH mechanisms, not
  portable kernel scheduling policy.
- Matched the C ThreadX low-level code to the external port's existing assembly
  ABI where required. `_k_thread_smp_protection` is accessed through 32-bit raw
  slots because the remaining assembly uses 32-bit offsets, and C stack build
  writes the ready bit at the scheduler assembly's ABI offset.
- Added `ARCH_KERNEL_THREADX` to the common thread handle and made
  `arch_api/thread.h` usable with kernels that provide their own fixed-width
  typedefs before including the shared ARCH headers.
- Added a ThreadX scheduler matrix app that emits the same `[sched-verify]`
  and `[xsync-verify]` markers as FreeRTOS and LK. It validates priority/RR
  progress plus semaphore, queue, mutex, and spinlock marker coverage through
  the common acceptance parser.
- Fixed ThreadX AArch64 QEMU execution by compiling the low-level scheduler
  path with `-DEL1`. Without this, the external port built stack frames and
  restore paths for EL3 while the boot path had already dropped to EL1.
- Added top-level ThreadX validation targets:
  `threadx-kernel`, `threadx-sched-full`, `threadx-sched-cross`, and
  `threadx-sched-smp`.

## Verified

- `make sched-test TIMEOUT=60`
  - Result: PASS
  - Cores: 2
  - Coverage: active/idle/affinity/progress/priority/RR/semaphore/queue/mutex/spin all PASS.
  - Latest default product-style run used `LOG_LEVEL=6` and mode-specific ELF
    naming: `kernel-2-full-log6-tb1.elf`.

- `make CORES=32 sched-test SCHED_MODE=full TIMEOUT=180`
  - Result: PASS
  - Cores: 32
  - Coverage: active/idle/affinity/progress/priority/RR/semaphore/queue/mutex/spin all PASS.
  - Latest run after the port-layer and mode-specific ELF fixes passed on the
    second marker: `seen=0xFFFFFFFF`.
  - Latest default product-style run used `LOG_LEVEL=6`.

- `make CORES=32 sched-cross TIMEOUT=60`
  - Result: PASS
  - Cores: 32
  - Coverage: cross-core semaphore/queue/mutex/spin all PASS.
  - Latest acceptance summary:
    `sem=PASS queue=PASS mutex=PASS spin=PASS sg=16 st=16 qs=16 qr=16 qe=0 me=32 mc=32 merr=0 se=32 sc=32 serr=0`.
  - Latest default product-style run used `LOG_LEVEL=6`.

- `make CORES=32 TIMEOUT=180 sched-matrix`
  - Result: PASS
  - Purpose: unified full + cross validation in one command.

- `make CORES=32 TIMEOUT=180 sched-smp`
  - Result: PASS
  - Purpose: product-facing SMP scheduler validation across the full scheduling
    and cross-core synchronization angles.
  - Latest stress result: PASS for 10 consecutive loops after the PMR restore
    fix.
  - Latest clean rebuild after compiling out default low-level diagnostics:
    PASS for both full and cross.
  - Latest clean rebuild after re-enabling `configASSERT()`: PASS for both full
    and cross, with no assert/panic marker.
  - Latest default tick-broadcast product build used `TICK_BROADCAST=1`
    (`tb1`) and passed both full and cross.

- `make CORES=32 TIMEOUT=120 lk-sched-smp`
  - Result: PASS
  - Kernel source: LK mainline snapshot from `origin/master`.
  - Full coverage: 32-core active/affinity/coverage/progress/priority/RR,
    semaphore, queue, mutex, and spinlock all PASS.
  - Cross coverage: semaphore/queue/mutex/spin all PASS.
  - Latest full marker reached `seen=0xffffffff missing=0x0`.

- `make CORES=32 TIMEOUT=180 arch-sched-matrix`
  - Result: PASS
  - Purpose: one command runs FreeRTOS full/cross, LK full/cross, and ThreadX
    full/cross through the same QEMU acceptance parser.
  - FreeRTOS final markers: full PASS and cross PASS.
  - LK final markers: full PASS and cross PASS.
  - ThreadX final markers: full PASS and cross PASS.
  - Current core counts: FreeRTOS and LK run at 32 cores; ThreadX runs at
    4 cores because the external ThreadX boot path still explicitly wakes only
    CPUs 1, 2, and 3.
  - Latest run after adding unified `arch_thread_handle_t`: PASS for all four
    stages.
  - Latest run after runtime handle/context-switch synchronization: PASS for
    all four stages.
  - Latest run after common ARCH IRQ/exception state and FreeRTOS IRQ dispatch
    migration: PASS for all four stages.
  - Latest run after FreeRTOS fatal exception C path and shared vector install:
    PASS for all four stages.
  - Latest run after ThreadX wrapper/EL1 adaptation: PASS for all six stages.
  - Latest run after ThreadX low-level C replacement: PASS for all six stages.
  - Latest run after ARCH CPU hint/stack/IPI API cleanup and LK priority probe
    stabilization: PASS for all six stages.
  - Latest run after replacing the ThreadX SMP protection path with the C
    wait-list fairness lock: PASS for all six stages.

- `make CORES=4 TIMEOUT=60 threadx-sched-smp`
  - Result: PASS
  - Kernel source: Luna ecosystem ThreadX tree at
    `/mnt/d/workspace/luna-workspace/other-os/threadx-master`.
  - Full coverage: active/idle/affinity/coverage/progress/priority/RR,
    semaphore, queue, mutex, and spinlock all PASS.
  - Cross coverage: semaphore/queue/mutex/spin all PASS.
  - Latest markers: `seen=0xf`, full PASS and cross PASS.
  - Latest run after replacing the safe low-level assembly subset with C:
    PASS for both full and cross.
  - Latest run after moving the secondary-core scheduler wait path to C:
    PASS for both full and cross.
  - Latest run after the fair wait-list SMP protection lock: PASS for both
    full and cross.

- `make CORES=8 TIMEOUT=90 threadx-sched-smp`
  - Result: PASS
  - Purpose: confirmed the ThreadX wrapper still boots and passes when built
    and launched with an 8-core configuration.
  - Effective coverage remains 4 active cores in the current external ThreadX
    boot/test path, shown by `seen=0xf`; this is not yet a real 8-core
    scheduler coverage result.
  - Full coverage: active/idle/affinity/coverage/progress/priority/RR,
    semaphore, queue, mutex, and spinlock all PASS.
  - Cross coverage: semaphore/queue/mutex/spin all PASS.
  - Latest markers: full PASS with `seen=0xf`; cross PASS.

- `make CORES=32 TIMEOUT=180 sched-smp`
  - Result: PASS after adding LK targets.
  - Purpose: confirmed FreeRTOS was not regressed by the ARCH layer and
    top-level Makefile changes.

- `make CORES=4 TIMEOUT=30 lk-sched-smp`
  - Result: PASS
  - Purpose: quick LK bring-up path for build/link/runtime regressions.

- `make CORES=32 TICK_BROADCAST=0 SCHED_MODE=full TIMEOUT=180 sched-test`
  - Result: PASS
  - Purpose: confirmed the optional no-broadcast tick policy still reaches
    32-core full scheduling convergence.

- `make CORES=32 TICK_BROADCAST=0 SCHED_MODE=cross TIMEOUT=60 sched-test`
  - Result: PASS
  - Purpose: confirmed cross-core semaphore/queue/mutex/spin stress does not
    depend on tick broadcast.

- `make CORES=32 LOG_LEVEL=7 sched-cross TIMEOUT=60`
  - Result: PASS
  - Purpose: confirmed the optional debug log build still compiles and passes
    the 32-core cross-core synchronization matrix.
  - Latest clean rebuild after conditional low-level diagnostics and
    `configASSERT()` re-enable: PASS.

- `make CORES=32 sched-cross-sem TIMEOUT=45`
  - Result: PASS
  - Coverage: `sg=16 st=16`.

- `make CORES=32 sched-cross-queue TIMEOUT=45`
  - Result: PASS
  - Coverage: `qs=16 qr=16 qe=0`.

- `make CORES=32 sched-cross-mutex TIMEOUT=45`
  - Result: PASS
  - Coverage: `me=32 mc=32 merr=0`.

- `make CORES=32 sched-cross-spin TIMEOUT=45`
  - Result: PASS
  - Coverage: `se=32 sc=32 serr=0`.

## Important negative result

- Forcing `FreeRTOS_IRQ_Handler` to switch to SP_EL1 at IRQ entry caused
  32-core full acceptance to regress with a stable missing core. It was not
  kept.
- Increasing per-core EL1 exception stacks from 8KB to 32KB also regressed
  32-core full acceptance with a stable missing core. It was reverted.
- Replacing ThreadX `_k_thread_smp_core_preempt()` with a first-pass C
  ICC_SGI1R_EL1 writer allowed threads to run but regressed semaphore/queue/
  mutex completion in full mode. It was not kept. The current hypothesis is
  that the external ThreadX GIC setup depends on its existing SGI routing/ID
  details; this should be revisited together with 32-core Aff1 SGI support.
- Replacing ThreadX `_k_timer_interrupt()` with a first-pass C implementation
  also allowed threads to run but regressed semaphore/queue/mutex completion.
  It was not kept. The likely issue is the ABI contract between the vector
  path, context-save path, preserved registers, and timer/time-slice processing.

## Remaining work

- Finish the non-inline ARCH context-switch implementation only if ThreadX or
  another kernel needs the shared layer to own register save/restore directly.
  FreeRTOS and LK currently use the shared hook as a synchronized handle/context
  view while retaining their native assembly switch paths.
- Productize LK wrapper logging. The current LK mainline wrapper still prints
  verbose boot diagnostics; scheduler acceptance uses Linux-style markers but
  LK's early boot log path is not yet normalized like the FreeRTOS BSP.
- Productize ThreadX wrapper logging. The current external ThreadX GIC/kernel
  files still print verbose boot/debug lines; the workspace app only emits the
  acceptance markers, but the external early log path still needs the same
  Linux-style log gate used by FreeRTOS.
- Lift ThreadX from 4-core validation to 32-core validation:
  replace the external `boot.S` hardcoded CPU1/2/3 PSCI wakeup sequence with
  the shared ARCH linear-CPU-to-MPIDR mapping, make the ThreadX GIC SGI path
  handle Aff1 target-list groups like LK, and audit hardcoded core loops in
  the ThreadX AArch64 port.
- Continue peeling ThreadX scheduler policy out of assembly:
  move ready-thread selection, ready-bit locking, current/execute pointer
  assignment, run-count/time-slice updates, and handle synchronization into C.
  Keep only the final SP switch and `eret` frame restore in an ARCH context
  trampoline until a fully portable context backend exists.
- Move ThreadX affinity/runtime updates deeper into the kernel paths. Creation
  and test-side affinity sync are wired now; the next step is to synchronize
  the common handle in `k_thread_smp_core_exclude()` and the low-level schedule
  handoff path so `arch_thread_handle_t.current_cpu` is live for all switches.
- Split timer sleep/timeout validation from scheduler/synchronization
  validation for LK. The LK matrix intentionally avoids timer-dependent waits
  because the current wrapper still needs a dedicated timer wakeup validation
  pass.
- Add longer soak coverage and per-primitive loop counts beyond the current
  10-loop 32-core `sched-smp` stress run.
- Add real hardware validation. Current evidence is QEMU/GICv3 only.
- Productize interrupt routing/affinity policy instead of relying on the
  current minimal BSP setup.
- Add an explicit negative-path QEMU test that deliberately triggers a
  synchronous exception and verifies the unified `[panic]` marker plus recorded
  ESR/ELR/FAR fields.
- Add regression coverage for suspend/resume, timeout ordering, timer service
  task behavior, event groups, stream/message buffers, and task notifications.
