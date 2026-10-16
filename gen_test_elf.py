import subprocess
from pathlib import Path

# ==============================================================================
# 1. Native Standalone Non-PIE (ET_EXEC) with exit(42) and write(1, msg, 39)
# ==============================================================================
src_nopie = """
.global _start
.text
_start:
    // sys_write(1, msg, 39)
    mov x0, #1
    adr x1, msg
    mov x2, #39
    mov x8, #64
    svc #0

    // sys_exit(42)
    mov x0, #42
    mov x8, #93
    svc #0

.section .rodata
msg:
    .ascii "[LINUX-NATIVE-NOPIE] Non-PIE verified!\\n"
"""

Path("/tmp/nopie.S").write_text(src_nopie)
subprocess.run(["aarch64-linux-gnu-gcc", "-nostdlib", "-no-pie", "/tmp/nopie.S", "-o", "/tmp/native_nopie_app.elf"], check=True)

data_nopie = Path("/tmp/native_nopie_app.elf").read_bytes()
lines = ["/* Native Linux AArch64 Non-PIE (ET_EXEC) ELF binary */",
         "unsigned char _tmp_native_nopie_elf[] = {"]
for i in range(0, len(data_nopie), 12):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in data_nopie[i:i+12]) + ",")
lines.append("};")
lines.append(f"unsigned int _tmp_native_nopie_elf_len = {len(data_nopie)};\n")

# ==============================================================================
# 2. Dynamic Interpreter (ld.so) - Standalone PIE (ET_DYN)
# Reads auxv from stack to find AT_ENTRY and AT_BASE, writes banner, jumps to AT_ENTRY
# ==============================================================================
src_interp = """
.global _start
.text
_start:
    // SP points to argc, argv[0..argc], NULL, envp[0..envc], NULL, auxv[]
    // Find AT_ENTRY (type=9) in auxv
    mov x10, sp
    ldr x11, [x10], #8          // skip argc
1:  ldr x12, [x10], #8          // skip argv
    cbnz x12, 1b
2:  ldr x12, [x10], #8          // skip envp
    cbnz x12, 2b

    // x10 now points to auxv: array of (uint64_t a_type, uint64_t a_val)
    mov x15, #0                 // target AT_ENTRY
3:  ldr x13, [x10], #8          // a_type
    ldr x14, [x10], #8          // a_val
    cbz x13, 4f                 // AT_NULL
    cmp x13, #9                 // AT_ENTRY == 9
    b.ne 3b
    mov x15, x14                // found entry point
    b 3b
4:
    // sys_write(1, msg, 45)
    mov x0, #1
    adr x1, msg
    mov x2, #45
    mov x8, #64
    svc #0

    // Jump to main executable AT_ENTRY
    cbnz x15, 5f
    // If AT_ENTRY not found, exit with error
    mov x0, #99
    mov x8, #93
    svc #0
5:
    br x15

.section .rodata
msg:
    .ascii "[DYNAMIC-LINKER] ld.so relocated & jumped!\\n"
"""

Path("/tmp/interp.S").write_text(src_interp)
subprocess.run(["aarch64-linux-gnu-gcc", "-nostdlib", "-fPIE", "-pie", "-Wl,-shared", "/tmp/interp.S", "-o", "/tmp/ld_so.elf"], check=True)

data_interp = Path("/tmp/ld_so.elf").read_bytes()
lines.append("/* Native Linux AArch64 Dynamic Interpreter (ld.so / PT_INTERP) */")
lines.append("unsigned char _tmp_ld_so_elf[] = {")
for i in range(0, len(data_interp), 12):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in data_interp[i:i+12]) + ",")
lines.append("};")
lines.append(f"unsigned int _tmp_ld_so_elf_len = {len(data_interp)};\n")

# ==============================================================================
# 3. Dynamic Linked Executable (with PT_INTERP = "/lib/ld-linux-aarch64.so.1")
# ==============================================================================
src_dyn_app = """
.global _start
.text
_start:
    // Main executable reached via dynamic linker!
    // sys_write(1, msg, 40)
    mov x0, #1
    adr x1, msg
    mov x2, #40
    mov x8, #64
    svc #0

    // sys_exit(42)
    mov x0, #42
    mov x8, #93
    svc #0

.section .rodata
msg:
    .ascii "[DYNAMIC-APP] Application main executed!\\n"
"""

Path("/tmp/dyn_app.S").write_text(src_dyn_app)
subprocess.run([
    "aarch64-linux-gnu-gcc", "-nostdlib", "-fPIE", "-pie",
    "-Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1",
    "/tmp/dyn_app.S", "-o", "/tmp/dyn_app.elf"
], check=True)

subprocess.run(["readelf", "-l", "/tmp/dyn_app.elf"])

data_dyn = Path("/tmp/dyn_app.elf").read_bytes()
lines.append("/* Native Linux AArch64 Dynamically Linked Application (PT_INTERP) */")
lines.append("unsigned char _tmp_dynamic_app_elf[] = {")
for i in range(0, len(data_dyn), 12):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in data_dyn[i:i+12]) + ",")
lines.append("};")
lines.append(f"unsigned int _tmp_dynamic_app_elf_len = {len(data_dyn)};\n")

out_h = Path("/home/surest/freertos-workspace/components/cherry_process/tests/native_nopie_bin.h")
out_h.write_text("\n".join(lines))
print(f"Successfully generated all test binaries into {out_h}")
