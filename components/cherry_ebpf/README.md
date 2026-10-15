# CherryEBPF - Full-Featured In-Kernel eBPF & Activity Tracing Subsystem

## 1. Overview
CherryEBPF is an enterprise-grade in-kernel eBPF (extended Berkeley Packet Filter) subsystem designed for FreeRTOS and bare-metal AArch64/RV64 kernels. It provides a standard Linux-compatible 64-bit virtual machine interpreter, static safety verifier, high-performance map data structures, and deep kernel activity tracepoints covering every layer of the operating system.

## 2. Key Architecture & Features
- **Standard 64-bit eBPF ISA**:
  - 10 general-purpose 64-bit registers (R0-R9) + read-only Frame Pointer (R10) pointing to a 512-byte stack.
  - Full ALU64 & ALU32 arithmetic and logic instructions (ADD, SUB, MUL, DIV, MOD, OR, AND, XOR, LSH, RSH, ARSH, NEG, MOV).
  - Endianness conversion (`BPF_END`) supporting 16-bit, 32-bit, and 64-bit byte swapping.
  - Complete conditional branching and jumps (JEQ, JNE, JGT, JGE, JLT, JLE, JSET, JSGT, JSGE, JSLT, JSLE, JA).
  - 64-bit wide immediate loads (`BPF_LD_IMM64`).
- **Static Safety Verifier**:
  - Bounds checks instruction counts and execution step limits.
  - Enforces program termination (`BPF_EXIT`).
  - Validates register indices, prevents modification of R10 frame pointer.
  - Enforces safe stack bounds and jump target boundaries.
  - Pre-execution rejection of divisions by constant zero.
- **eBPF Maps**:
  - `BPF_MAP_TYPE_HASH`: Dynamic FNV-1a hash map with bucket collision chains and multi-core spinlock protection.
  - `BPF_MAP_TYPE_ARRAY`: O(1) indexed flat memory array.
  - `BPF_MAP_TYPE_RINGBUF`: Lockless circular ring buffer for real-time event streaming.
- **Standard BPF Helpers**:
  - `bpf_map_lookup_elem`, `bpf_map_update_elem`, `bpf_map_delete_elem`
  - `bpf_probe_read`: Safe kernel memory access
  - `bpf_ktime_get_ns`: High-resolution nanoseconds clock from ARM Generic Timer (`cntpct_el0`)
  - `bpf_trace_printk`: Direct kernel trace formatting
  - `bpf_get_smp_processor_id`: Core ID detection
  - `bpf_get_current_pid_tgid`: Process/task identifier extraction
  - `bpf_get_current_comm`: Process/task name extraction
  - `bpf_ringbuf_output`: High-throughput event publishing
- **Comprehensive Activity Tracing (18 Tracepoints)**:
  - System Calls: `sys_enter`, `sys_exit`
  - Scheduler: `sched_switch`, `sched_wakeup`
  - Interrupts: `irq_enter`, `irq_exit`
  - VFS: `vfs_open`, `vfs_close`, `vfs_read`, `vfs_write`
  - Storage Block I/O: `block_rq_issue`, `block_rq_complete`
  - Networking: `net_tx_pkt`, `net_rx_pkt`
  - Kernel Modules: `kmod_load`, `kmod_unload`
  - Process Management: `proc_spawn`, `proc_exit`
- **Shell & CLI Tooling (`ebpf` / `bpftool`)**:
  - `ebpf list`: List all loaded programs and execution stats
  - `ebpf maps`: List all maps, capacities, and memory usage
  - `ebpf tp`: List all 18 kernel tracepoints and hit counts
  - `ebpf top`: Real-time kernel activity monitor
  - `ebpf dump <map_id>`: Inspect key-value pairs inside maps

## 3. Directory Layout
```
components/cherry_ebpf/
├── include/
│   ├── cherry_ebpf.h      # Master header
│   ├── ebpf_insn.h        # eBPF instruction encoding & macros
│   ├── ebpf_map.h         # Map subsystem & structures
│   ├── ebpf_trace.h       # Tracepoint events & subsystem hooks
│   └── ebpf_vm.h          # VM interpreter & verifier declarations
├── src/
│   ├── ebpf_vm.c          # 64-bit interpreter engine & verifier
│   ├── ebpf_map.c         # Hash, Array, RingBuf implementations
│   ├── ebpf_helpers.c     # Standard helper dispatch
│   ├── ebpf_trace.c       # Tracepoint registry & hook handlers
│   └── ebpf_shell.c       # Interactive CLI & bpftool commands
├── tests/
│   └── ebpf_test.c        # Acceptance test suite
├── SConscript             # SCons build definition
└── README.md              # Documentation
```
