# Dawn 风格图稿

图形采用与 Dawn 一致的深色背景、圆角分层容器和语义配色：应用为绿色，内核与组件为蓝色，内核抽象服务为紫色，硬件为金色。HTML 预览可切换浅色，文档 SVG 默认使用深色。

系统架构、模块组织和 I/O 分层图不画箭头，以层次和分组表示结构；启动、初始化和验收流程使用有方向的箭头。`master` 保留最小内核范围，`vibe-coding-dev` 保留组件系统范围，各分支独立维护。

| 图形 | 表达方式 | 可编辑源文件 | 预览 | 文档图片 |
| --- | --- | --- | --- | --- |
| 系统架构 | 结构图，无箭头 | [JSON](architecture.archify.json) | [HTML](architecture.html) | [SVG](../images/architecture.svg) |
| 启动流程 | 流程图，有方向箭头 | [JSON](boot.archify.json) | [HTML](boot.html) | [SVG](../images/boot.svg) |
| 自动化验收 | 流程图，有方向箭头 | [JSON](testing.archify.json) | [HTML](testing.html) | [SVG](../images/testing.svg) |
| 模块组织 | 结构图，无箭头 | [JSON](modules.archify.json) | [HTML](modules.html) | [SVG](../images/modules.svg) |
| 存储与网络 | 结构图，无箭头 | [JSON](io-stack.archify.json) | [HTML](io-stack.html) | [SVG](../images/io-stack.svg) |

## 修改和导出

编辑对应的 `*.archify.json`，然后在仓库根目录运行：

```sh
python3 docs/diagrams/render_dawn.py
python3 docs/diagrams/render_dawn.py --check
```

导出脚本只使用 Python 3 标准库，从同一份 JSON 同时生成 `docs/images/*.svg` 与本目录的 HTML，不需要浏览器或网络。`--check` 检查所有导出文件是否与源图一致；导出时还会检查箭头分类和文字所需高度，拒绝裁切文字。

JSON 沿用 Archify 的可编辑结构。为固定 Dawn 的配色、字体和箭头规则，文档资源由本仓库的 `render_dawn.py` 导出，而非 Archify 默认主题。重新生成后应检查各张 SVG 的文字、分组和箭头方向。PNG 仅用于目视复核，文档继续引用原有 SVG 路径。

JSON 中的 `meta.repository.revision` 与 `sources` 保留原图的来源记录；原 Excalidraw 图稿可从对应提交的 Git 历史查看。启动图的组件调用顺序对照 `bsp/src/app.c`，验收流程对照 `tests/acceptance/accept_qemu_sched.py`。

验收图中的组件自检结果仅供查看，不参与脚本的最终通过判定。图稿导出与检查不等同于重新完成内核运行或板卡验证。
