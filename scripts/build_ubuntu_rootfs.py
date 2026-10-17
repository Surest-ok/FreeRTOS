#!/usr/bin/env python3
import os
import shutil
import subprocess
import urllib.request
from pathlib import Path

def ensure_ubuntu_bash_root(root: Path):
    if (root / "bin/bash").exists():
        return True

    print(f"[*] Preparing Ubuntu aarch64 bash rootfs at {root}...")
    root.mkdir(parents=True, exist_ok=True)

    packages = [
        ("bash", "http://ports.ubuntu.com/pool/main/b/bash/bash_5.1-6ubuntu1.1_arm64.deb"),
        ("libtinfo6", "http://ports.ubuntu.com/pool/main/n/ncurses/libtinfo6_6.3-2ubuntu0.3_arm64.deb"),
    ]

    cache_dir = Path("/tmp")
    for pkg_name, url in packages:
        deb_file = cache_dir / url.split("/")[-1]
        if not deb_file.exists():
            print(f"[*] Downloading {pkg_name} from {url}...")
            urllib.request.urlretrieve(url, deb_file)
        print(f"[*] Extracting {deb_file.name}...")
        subprocess.run(["dpkg-deb", "-x", str(deb_file), str(root)], check=True)

    glibc_candidates = [
        Path("/usr/aarch64-linux-gnu/lib"),
        Path("/usr/lib/aarch64-linux-gnu"),
    ]
    for lib_dir in glibc_candidates:
        libc = lib_dir / "libc.so.6"
        ld = lib_dir / "ld-linux-aarch64.so.1"
        if libc.exists():
            (root / "lib/aarch64-linux-gnu").mkdir(parents=True, exist_ok=True)
            shutil.copy2(libc, root / "lib/aarch64-linux-gnu/libc.so.6")
        if ld.exists():
            (root / "lib").mkdir(parents=True, exist_ok=True)
            shutil.copy2(ld, root / "lib/ld-linux-aarch64.so.1")

    return (root / "bin/bash").exists()

def main():
    root = Path("/tmp/ubuntu_bash_root")
    if not ensure_ubuntu_bash_root(root):
        print("Error: Failed to prepare Ubuntu bash rootfs!")
        return 1

    # Replace symlinks with real files for standalone ext4 and direct vfs access
    tinfo_link = root / "lib/aarch64-linux-gnu/libtinfo.so.6"
    tinfo_real = root / "lib/aarch64-linux-gnu/libtinfo.so.6.3"
    if tinfo_real.exists() and (tinfo_link.is_symlink() or not tinfo_link.exists()):
        if tinfo_link.is_symlink():
            tinfo_link.unlink()
        shutil.copy2(tinfo_real, tinfo_link)

    tic_link = root / "usr/lib/aarch64-linux-gnu/libtic.so.6"
    tic_real = root / "usr/lib/aarch64-linux-gnu/libtic.so.6.3"
    if tic_real.exists() and (tic_link.is_symlink() or not tic_link.exists()):
        if tic_link.is_symlink():
            tic_link.unlink()
        shutil.copy2(tic_real, tic_link)

    real_ld = Path("/usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1")
    if real_ld.exists():
        (root / "lib").mkdir(parents=True, exist_ok=True)
        shutil.copy2(real_ld, root / "lib/ld-linux-aarch64.so.1")

    # Create standard directories
    for d in ["etc", "tmp", "root", "dev", "proc", "sys", "mnt"]:
        (root / d).mkdir(parents=True, exist_ok=True)

    # Create etc/passwd, etc/group, etc/profile
    (root / "etc/passwd").write_text("root:x:0:0:root:/root:/bin/bash\n")
    (root / "etc/group").write_text("root:x:0:\n")
    (root / "etc/profile").write_text(
        "export PATH=/bin:/usr/bin:/sbin:/usr/sbin\n"
        "export LD_LIBRARY_PATH=/lib/aarch64-linux-gnu:/usr/lib/aarch64-linux-gnu:/lib:/usr/lib\n"
        "export HOME=/root\n"
        "export TERM=linux\n"
    )

    # Create test script inside rootfs
    test_sh = root / "root/test_script.sh"
    test_sh.write_text(
        "#!/bin/bash\n"
        "echo \"[UBUNTU-BASH] Hello from GNU Bash inside CherryOS!\"\n"
        "export TEST_ENV=\"UbuntuRootFS_OK\"\n"
        "echo \"[UBUNTU-BASH] TEST_ENV=$TEST_ENV\"\n"
        "for item in alpha beta gamma; do\n"
        "    echo \"[UBUNTU-BASH] item=$item\"\n"
        "done\n"
        "echo \"[UBUNTU-BASH] Arithmetic: 12 + 30 = $((12 + 30))\"\n"
    )
    test_sh.chmod(0o755)

    bin_test_sh = root / "bin/test_bash.sh"
    bin_test_sh.write_text(test_sh.read_text())
    bin_test_sh.chmod(0o755)

    repo_root = Path(__file__).resolve().parent.parent
    out_img = repo_root / "build/ubuntu_rootfs.ext4"
    out_img.parent.mkdir(parents=True, exist_ok=True)
    if out_img.exists():
        out_img.unlink()

    # Generate 64MB ext4 image using mkfs.ext4 -d
    cmd = ["mkfs.ext4", "-d", str(root), "-L", "ubuntu_root", "-F", str(out_img), "64M"]
    subprocess.run(cmd, check=True)
    print(f"Successfully built {out_img} ({out_img.stat().st_size} bytes)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
