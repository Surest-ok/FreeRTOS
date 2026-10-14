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

    return [
        "qemu-system-aarch64",
        "-machine",
        "virt,gic-version=3",
        "-cpu",
        "cortex-a53",
        "-smp",
        str(args.cores),
        "-m",
        "512",
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
        return all(fields.get(key) == "PASS" for key in required)

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

    return all(fields.get(key) == "PASS" for key in required)


def process_line(
    line: str,
    args: argparse.Namespace,
    markers: list[str],
) -> bool | None:
    if any(marker in line for marker in FAIL_MARKERS):
        sys.stdout.write(line)
        sys.stdout.flush()
        print(f"[SCHED-TEST] SUMMARY result=FAIL reason=fatal-marker cores={args.cores}")
        return False

    if "[vfs-verify]" in line:
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
                f"[SCHED-TEST] SUMMARY result=PASS mode={args.mode} "
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
        f"[SCHED-TEST] SUMMARY result=FAIL mode={args.mode} "
        f"cores={args.cores} markers={len(markers)} reason={reason}"
    )
    if markers:
        print(f"[SCHED-TEST] LAST {markers[-1]}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
