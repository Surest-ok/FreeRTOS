# Archify 架构图

本目录保存可编辑的 Archify JSON 与独立 HTML。文档继续引用 `../images/` 中同名 SVG，原有图片路径保持不变。

全部图形采用无箭头布局：分层和分组表示结构，编号表示启动与验收步骤。模块名称、初始化顺序和验收边界来自原图；`master` 保留最小内核范围，`vibe-coding-dev` 保留组件系统范围。

| 图形 | 可编辑源文件 | HTML | 文档图片 |
| --- | --- | --- | --- |
| 系统架构 | [JSON](architecture.archify.json) | [HTML](architecture.html) | [SVG](../images/architecture.svg) |
| 启动流程 | [JSON](boot.archify.json) | [HTML](boot.html) | [SVG](../images/boot.svg) |
| 自动化验收 | [JSON](testing.archify.json) | [HTML](testing.html) | [SVG](../images/testing.svg) |

## 修改和导出

使用 Archify 3.1.0，修改对应 `*.archify.json`，在仓库根目录运行：

```text
node <archify安装目录>/bin/archify.mjs finalize architecture docs/diagrams/architecture.archify.json docs/diagrams/architecture.html --repo-root . --quality showcase --out-dir <新的验证目录> --json
```

其他图替换同名文件即可。JSON 内的 `meta.repository.revision` 固定了原图所在提交，`sources` 指向该版本的原始图形；旧 Excalidraw 文件可从该提交的 Git 历史查看，不再作为当前版本的编辑源。

通过检查后用浏览器打开 HTML，在“导出”菜单选择“SVG · 浅色”，保存到 `docs/images/` 对应路径。导出后检查文字、分组及无箭头要求，再提交 JSON、HTML 和 SVG。PNG 可用于目视复核，不作为文档唯一来源。
