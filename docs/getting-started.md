# 上手指南

从一台干净的 Ubuntu 开始：安装工具、获取代码、构建、在 QEMU 上运行，最后跑一遍自动化验收。全程大约 15 分钟（首次构建和下载占大部分时间）。

## 1. 准备环境

Ubuntu 20.04 或更新版本，实体机、虚拟机、WSL2 都可以。

```bash
sudo apt-get update
sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-system-arm \
                        python3 python3-pip e2fsprogs
pip3 install scons
```

| 工具 | 用途 |
| --- | --- |
| `gcc-aarch64-linux-gnu` | 交叉编译器；它自带的 AArch64 glibc 也会被拷进 Ubuntu 用户空间镜像 |
| `qemu-system-arm` | 提供 `qemu-system-aarch64` |
| `scons` | 构建系统（`pip3` 安装在 `~/.local/bin`，确认它在 `PATH` 里） |
| `e2fsprogs` | `mkfs.ext4`，生成根文件系统镜像 |

检查：

```bash
aarch64-linux-gnu-gcc --version
qemu-system-aarch64 --version
scons --version
```

## 2. 获取代码

组件都是 git 子模块，克隆时必须带 `--recursive`：

```bash
git clone --recursive -b vibe-coding-dev https://github.com/Surest-ok/FreeRTOS.git
cd FreeRTOS
```

已经克隆但没有带 `--recursive`，补一句：

```bash
git submodule update --init --recursive
```

## 3. 构建

```bash
scons -j$(nproc)
```

产物在 `build/bsp/`：`freertos_smp.elf`（给 QEMU 用）和 `freertos_smp.bin`。每个组件的静态库在 `build/<组件>/lib<组件>.a`。

清理用 `scons -c`，或者 `make clean`（同时删除整个 `build/`）。

## 4. 生成根文件系统镜像

```bash
python3 scripts/build_ubuntu_rootfs.py
```

脚本从 `ports.ubuntu.com` 下载 Bash 和 `libtinfo6` 的 arm64 包，加上交叉工具链自带的 glibc，生成 64 MB 的 `build/ubuntu_rootfs.ext4`。它会作为第二块 SATA 盘（`/dev/sdb`）挂到 `/`。只需生成一次。

## 5. 运行

```bash
scons run              # 默认 2 核
scons CORES=4 run      # 4 核
```

QEMU 会接上这些设备：SDHCI + SD 卡、NVMe、AHCI（两块 SATA 盘）、virtio-net、virtio-gpu。SD、NVMe 和第一块 SATA 盘是 `build/` 下 64 MB 的空白镜像，首次运行时自动创建。

启动日志的格式是 `[时间] [级别:核:任务:任务号] 内容`。依次能看到：

```text
[    0.291295] [6:cpu0:swapper:0] freertos: freertos smp modular boot
[    0.331558] [6:cpu0:swapper:0] freertos: [boot] cores=2 timer=62500000hz
...
[    1.275031] [6:cpu0:sched-mon:55045] [printk-verify] basic=pass multicore=pass irq_safe=pass all=pass
[    1.476720] [6:cpu0:sched-mon:55045] [timer-verify] standard=pass hrtimer=pass ... all=pass
...
[    5.735724] [6:cpu0:sched-mon:55045] cherry_ext4: mounted '/dev/sdb' on '/' (vol='ubuntu_root', blksize=4096)
```

每个组件初始化时都会跑一遍自检，打印一行 `[<组件>-verify] ... all=pass`。

**退出 QEMU**：先按 `Ctrl+A`，松开，再按 `X`。

## 6. 运行验收

```bash
scons test                    # 全功能验收
scons SCHED_MODE=cross test   # 跨核同步压测
```

最后一行是结论：

```text
[SCHED-TEST] SUMMARY result=pass mode=full cores=2 markers=1
```

验收模式和判定规则见 [测试](testing.md)。

## 7. 接下来

- 看系统怎样分层、怎样启动：[架构说明](architecture.md)
- 改一个组件，或者新增一个组件：[组件说明](components.md)
- 每个组件的接口：`components/<组件>/README.md`

## 常见问题

**`scons: command not found`**：`pip3 install scons` 装在 `~/.local/bin`，执行 `export PATH=$HOME/.local/bin:$PATH`。

**构建时报找不到 `components/xxx/SConscript`**：子模块没有拉下来，执行 `git submodule update --init --recursive`。

**验收测试提示根文件系统相关错误**：确认已经生成 `build/ubuntu_rootfs.ext4`（第 4 步）。

**下载 Ubuntu 包很慢或失败**：脚本把包缓存在 `/tmp`，可以手动把 `bash_5.1-6ubuntu1.1_arm64.deb`、`libtinfo6_6.3-2ubuntu0.3_arm64.deb` 放进 `/tmp` 再运行。

**QEMU 报 `its` 参数不认识**：QEMU 版本太旧，请升级。
