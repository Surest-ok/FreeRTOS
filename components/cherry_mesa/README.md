# CherryMesa - 3D Graphics Subsystem

Linux Mesa/Gallium3D 标准定制 3D 图形驱动与运行库，专为 CherryOS / FreeRTOS 架构定制，提供 GBM、EGL 1.4、OpenGL ES 2.0 API 以及 VirtIO-GPU (VirGL) 与 ARM Mali (Panfrost Bifrost) 硬件加速。

## 核心特性
- **GBM 缓冲区管理**：支持跨 GPU 渲染缓冲与显示缓冲分配、mmap 映射与 PRIME DMA-BUF 零拷贝共享。
- **EGL 1.4 运行时**：实现 EGLDisplay、EGLConfig、EGLContext 与双缓冲窗口/离屏 Surface 生命周期管理及 `eglSwapBuffers`。
- **OpenGL ES 2.0 API**：完整的 GLSL 着色器编译、程序链接、顶点缓冲区（VBO）、Uniform 参数设置与 `glDrawArrays` 渲染管线。
- **Gallium3D 驱动架构**：
  - `panfrost`：ARM Mali Bifrost 架构 16x16 像素分块光栅化（Hierarchical Tiling）、多边形 Binning、Quad SIMD 子句执行与直接 Scanout 显示。
  - `virgl`：VirtIO-GPU 3D 指令流封装与主机硬件加速。
  - `softpipe`：高效质心重心坐标（Barycentric Coordinates）与深度测试软件光栅化保底。
- **经典基准与演示**：内置旋转 3D 彩色三角形、经典 `es2gears` 齿轮旋转基准程序，以及 Mali Bifrost 分块渲染演示。
- **Shell 工具**：`mesa info`、`mesa triangle [frames]`、`mesa gears [frames]`、`mesa test`、`es2gears`、`bifrost info`、`bifrost triangle [frames]`、`bifrost gears [frames]`、`bifrost test`。