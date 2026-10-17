# 组件说明

除内核、移植层和 BSP 外，系统的功能都在 `components/` 下的组件里。每个组件是一个独立的 git 仓库，作为子模块引入主仓库。

## 依赖关系

链接顺序（`bsp/SConscript`）就是依赖顺序，靠前的依赖靠后的：

```text
cherry_mesa → cherry_drm → cherry_process → cherry_usbd → cherry_ahci → cherry_nvme → cherry_net
  → cherry_fstools → cherry_ext4 → cherry_sdio → cherry_pci → cherry_shell → cherry_vfs
  → cherry_timer → cherry_printk → cherry_kal → FreeRTOS 内核
```

`cherry_kmod` 和 `cherry_ebpf` 与 `cherry_process` 处在同一层。

几条规则：

- 组件只包含 `cherry_kal.h` 和其他组件的公开头文件（`include/`），不包含 FreeRTOS 的头文件。
- 驱动通过 `cherry_vfs` 的设备模型拿到板级资源，不直接写死板级地址。
- 块设备、字符设备、套接字都注册到 `cherry_vfs`，组件之间通过它交互，例如 `cherry_ext4` 只认块设备，不关心下面是 AHCI 还是 NVMe。

## 组件的结构

以 `cherry_timer` 为例：

```text
components/cherry_timer/
├── include/          公开头文件（cherry_timer.h、timer.h、hrtimer.h）
├── src/              实现，内部头文件放在这里
├── tests/            自检代码，提供 cherry_timer_run_tests()
├── SConscript        构建成 libcherry_timer.a
└── README.md
```

每个组件对外提供两个入口：

| 函数 | 作用 |
| --- | --- |
| `cherry_<组件>_init()` | 初始化，启动时由 `bsp/src/app.c` 调用 |
| `cherry_<组件>_run_tests()` | 自检，打印一行 `[<组件>-verify] ... all=pass` |

## 新增一个组件

假设新组件叫 `cherry_foo`：

1. **建仓库**：按上面的结构放 `include/`、`src/`、`tests/`、`SConscript`、`README.md`。`SConscript` 参照 `components/cherry_timer/SConscript`：`Import('env')`，`Clone()` 一份环境，加上自己和 `cherry_kal` 的头文件路径，`Library('libcherry_foo.a', ...)`，最后 `Return`。
2. **加为子模块**：

   ```bash
   git submodule add https://github.com/<你>/cherry_foo.git components/cherry_foo
   ```

3. **接入构建**：在 `SConstruct` 里仿照其他组件加一段 `env.SConscript('components/cherry_foo/SConscript', variant_dir=f'{build_dir}/cherry_foo', ...)`，把得到的库加到 `bsp/SConscript` 的 `Import` 和链接列表里，位置按依赖关系决定（依赖它的组件在前，它依赖的组件在后）。
4. **接入启动**：在 `bsp/src/app.c` 的组件初始化序列中，在它依赖的组件之后调用 `cherry_foo_init()` 和 `cherry_foo_run_tests()`。
5. **验证**：`scons test`，确认日志里出现 `[foo-verify] ... all=pass`，并且最终 `SUMMARY result=pass`。

## 修改已有组件

组件是子模块，修改分两步提交：

```bash
cd components/cherry_net
git checkout master          # 子模块默认处在分离头指针状态，先切到分支
# ...修改、构建、测试...
git commit -am "net: ..."
git push

cd ../..
git add components/cherry_net   # 主仓库记录子模块的新版本
git commit -m "chore: bump cherry_net"
```

只提交了子模块、没有在主仓库更新引用，其他人克隆主仓库时拿到的仍是旧版本。
