#!/usr/bin/env python3
"""Run an SMP scheduler image in QEMU and validate scheduler markers."""

from __future__ import annotations

import argparse
import os
import re
import select
import subprocess
import sys
import time
from pathlib import Path


SCHED_MARKER = "[sched-verify]"
XSYNC_MARKER = "[xsync-verify]"
FAIL_MARKERS = (
    "[panic]",
    "[PANIC]",
    "[assert]",
    "ASSERT FAILED",
    "panic:",
    "PANIC:",
    "Data Abort",
    "Synchronous Abort",
    "Undefined Instruction",
    "malloc failed",
    "stack overflow",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cores", type=int, default=2)
    parser.add_argument("--timeout", type=int, default=30)
    parser.add_argument("--el", type=int, default=1, choices=(1, 2))
    parser.add_argument("--elf", type=Path)
    parser.add_argument(
        "--mode",
        choices=("quick", "full", "cross", "cross-sem", "cross-queue", "cross-mutex", "cross-spin"),
        default="quick",
        help="quick/full validate scheduler acceptance; cross validates cross-core synchronization probes",
    )
    return parser.parse_args()


def qemu_command(args: argparse.Namespace) -> list[str]:
    elf = args.elf or Path(f"bsp/kernel-{args.cores}.elf")
    if not elf.exists():
        raise FileNotFoundError(elf)

    sd_img = Path("build/test_sd.img")
    sd_img.parent.mkdir(parents=True, exist_ok=True)
    if not sd_img.exists() or sd_img.stat().st_size != 64 * 1024 * 1024:
        with open(sd_img, "wb") as f:
            f.truncate(64 * 1024 * 1024)

    nvme_img = Path("build/test_nvme.img")
    if not nvme_img.exists() or nvme_img.stat().st_size != 64 * 1024 * 1024:
        with open(nvme_img, "wb") as f:
            f.truncate(64 * 1024 * 1024)

    sata_img = Path("build/test_sata.img")
    if not sata_img.exists() or sata_img.stat().st_size != 64 * 1024 * 1024:
        with open(sata_img, "wb") as f:
            f.truncate(64 * 1024 * 1024)

    machine_opt = "virt,virtualization=on,gic-version=3,its=on" if args.el == 2 else "virt,gic-version=3,its=on"
    return [
        "qemu-system-aarch64",
        "-machine",
        machine_opt,
        "-cpu",
        "cortex-a53",
        "-smp",
        str(args.cores),
        "-m",
        "512",
        "-device",
        "sdhci-pci,id=sdhci0",
        "-drive",
        f"if=none,id=sd0,file={sd_img},format=raw",
        "-device",
        "sd-card,drive=sd0",
        "-netdev",
        "user,id=net0",
        "-device",
        "virtio-net-pci,netdev=net0,mac=52:54:00:12:34:56",
        "-drive",
        f"if=none,id=nvme0,file={nvme_img},format=raw",
        "-device",
        "nvme,drive=nvme0,serial=nvme_disk_0",
        "-device",
        "ich9-ahci,id=ahci0",
        "-drive",
        f"if=none,id=sata0,file={sata_img},format=raw",
        "-device",
        "ide-hd,drive=sata0,bus=ahci0.0",
        "-device",
        "virtio-gpu-pci,id=gpu0",
        "-kernel",
        str(elf),
        "-nographic",
        "-monitor",
        "none",
        "-serial",
        "stdio",
    ]


def parse_marker(line: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for key, value in re.findall(r"([A-Za-z0-9_]+)=([^ \r\n]+)", line):
        result[key] = value
    return result


def marker_passes(fields: dict[str, str], mode: str) -> bool:
    if mode.startswith("cross"):
        required_by_mode = {
            "cross": ("sem", "queue", "mutex", "spin"),
            "cross-sem": ("sem",),
            "cross-queue": ("queue",),
            "cross-mutex": ("mutex",),
            "cross-spin": ("spin",),
        }
        required = required_by_mode[mode]
        return all(fields.get(key) in ("PASS", "pass") for key in required)

    required = ("active", "idle", "affinity")
    if mode == "full":
        required = required + (
            "coverage",
            "progress",
            "prio",
            "rr",
            "sem",
            "queue",
            "mutex",
            "spin",
        )

    return all(fields.get(key) in ("PASS", "pass") for key in required)


def process_line(
    line: str,
    args: argparse.Namespace,
    markers: list[str],
) -> bool | None:
    if any(marker in line for marker in FAIL_MARKERS):
        sys.stdout.write(line)
        sys.stdout.flush()
        print(f"[SCHED-TEST] SUMMARY result=fail reason=fatal-marker cores={args.cores}")
        return False

    if "[vfs-verify]" in line or "[vfs-stress]" in line or "[pci-verify]" in line or "[sdio-verify]" in line or "[ext4-verify]" in line or "[fstools-verify]" in line or "[net-verify]" in line or "[nvme-verify]" in line or "[ahci-verify]" in line or "[usbd-verify]" in line or "[process-verify]" in line or "[shell-verify]" in line or "[kmod-verify]" in line or "[ebpf-verify]" in line or "[drm-verify]" in line or "[mesa-verify]" in line or "[printk-verify]" in line or "[timer-verify]" in line or "cherry_" in line:
        sys.stdout.write(line)
        sys.stdout.flush()

    expected_marker = XSYNC_MARKER if args.mode.startswith("cross") else SCHED_MARKER
    if expected_marker in line:

        markers.append(line.rstrip())
        fields = parse_marker(line)
        if marker_passes(fields, args.mode):
            sys.stdout.write(line)
            sys.stdout.flush()
            print(
                f"[SCHED-TEST] SUMMARY result=pass mode={args.mode} "
                f"cores={args.cores} markers={len(markers)}"
            )
            return True
        if len(markers) == 1:
            sys.stdout.write(line)
            sys.stdout.flush()

    return None


def main() -> int:
    args = parse_args()
    cmd = qemu_command(args)
    deadline = time.time() + args.timeout
    markers: list[str] = []
    pending = ""

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="ignore",
        bufsize=1,
    )

    try:
        assert proc.stdout is not None
        os.set_blocking(proc.stdout.fileno(), False)
        while time.time() < deadline:
            ready, _, _ = select.select([proc.stdout], [], [], 0.1)
            if not ready:
                if proc.poll() is not None:
                    break
                continue

            chunk = proc.stdout.read()
            if not chunk:
                continue

            pending += chunk
            lines = pending.splitlines(True)
            pending = ""
            if lines and not lines[-1].endswith(("\n", "\r")):
                pending = lines.pop()

            for line in lines:
                result = process_line(line, args, markers)
                if result is True:
                    return 0
                if result is False:
                    return 1
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()

    if pending:
        result = process_line(pending + "\n", args, markers)
        if result is True:
            return 0
        if result is False:
            return 1

    reason = "no-marker" if not markers else "condition-not-met"
    print(
        f"[SCHED-TEST] SUMMARY result=fail mode={args.mode} "
        f"cores={args.cores} markers={len(markers)} reason={reason}"
    )
    if markers:
        print(f"[SCHED-TEST] LAST {markers[-1]}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
