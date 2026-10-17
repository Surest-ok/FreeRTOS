# 测试

![自动化验收](images/testing.svg)

## 运行

```bash
scons test                          # SCHED_MODE=full，全功能验收
scons SCHED_MODE=cross test         # 跨核同步压测（信号量、队列、互斥锁、自旋锁）
scons SCHED_MODE=cross-sem test     # 只压测一种：cross-sem / cross-queue / cross-mutex / cross-spin
scons CORES=4 TIMEOUT=180 test      # 换核数、放宽超时
```

`make sched-full`、`make sched-cross`、`make sched-smp`（两者都跑）是同样的命令。

验收前请先生成 `build/ubuntu_rootfs.ext4`（`python3 scripts/build_ubuntu_rootfs.py`）。

## 过程

1. `scons test` 先构建镜像，再调用 `tests/acceptance/accept_qemu_sched.py`。
2. 脚本在 `build/` 下准备 64 MB 的 SD、NVMe、SATA 空白镜像，启动 QEMU（SDHCI、NVMe、AHCI 两块盘、virtio-net、virtio-gpu），读取串口输出。
3. 镜像里的探针任务把结果打印成一行标记，脚本据此判定。

## 判定

| 模式 | 必须出现的标记 | 检查的项目 |
| --- | --- | --- |
| `full` | `[sched-verify]` | 活跃、空闲任务、亲和性、核覆盖、进度、优先级、轮转、信号量、队列、互斥锁、自旋锁 |
| `cross*` | `[xsync-verify]` | 对应的跨核同步原语 |

标记里每一项都是 `pass` 时，脚本输出 `[SCHED-TEST] SUMMARY result=pass` 并以 0 退出。出现下列任何一种情况就判失败：

- 输出中出现 `[panic]`、`ASSERT FAILED`、`Data Abort`、`Synchronous Abort`、`Undefined Instruction`、`malloc failed`、`stack overflow` 等；
- 超时（`TIMEOUT`，默认 90 秒）仍没有等到标记。

各组件的自检结果（`[vfs-verify]`、`[ext4-verify]`、`[net-verify]`、`[process-verify]`、`[busybox-verify]` 等）同样打印在日志里，便于查看，但不参与判定。

## 日志

`scons test` 的输出就是完整的串口日志。保存下来方便对比：

```bash
scons test 2>&1 | tee build/test.log
grep -o '\[[a-z0-9]*-verify\].*' build/test.log
```

## 已知问题

- 当前版本约一半的验收运行会在 `cherry_drm` 打印 `virtio-gpu modern 1.0 hardware initialized` 之后崩溃：`[panic] core=0 reason=exception esr=0x2000000 elr=0x0`，即跳转到了地址 0。`full` 与 `cross` 模式都会出现（在 2 核上连续 7 次运行中通过 3 次），正在排查。遇到时保留日志并重跑。
