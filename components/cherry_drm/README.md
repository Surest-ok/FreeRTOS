# CherryDRM - Direct Rendering Manager Subsystem

Linux 标准直连渲染管理（DRM/KMS）内核子系统，专为 CherryOS / FreeRTOS 架构定制，提供 VirtIO-GPU 显示输出与 ARM Mali Panfrost (Bifrost/Midgard) 显示及 3D 渲染支持。

## 核心特性
- **设备节点**：
  - `/dev/dri/card0`（VirtIO-GPU 主显示与 KMS 控制节点）
  - `/dev/dri/card1`（ARM Mali Panfrost Bifrost 独立 KMS 显示控制节点）
  - `/dev/dri/renderD128`（Mali Panfrost 3D 渲染加速节点）
  - `/dev/fb0`（Linux Framebuffer 控制台仿真，支持动态绑定 VirtIO / Panfrost）
- **内存管理**：GEM Dumb Buffers 分配、mmap 映射、DMA-BUF / PRIME 跨设备零拷贝内存共享。
- **KMS 显示管道**：CRTC（`Mali-CRTC-0` / `Virtual-CRTC-0`）、Connector（`Mali-DP-1` / `Virtual-1`）、Encoder、Framebuffer，支持 1024x768、1280x720、1920x1080@60Hz 分辨率与 PageFlip 双缓冲翻页。
- **硬件驱动**：
  - `virtio_gpu`：基于 PCIe 总线的 VirtIO-GPU 协议驱动（2D/3D VirGL）。
  - `panfrost`：ARM Mali Bifrost (v7)/Midgard GPU 驱动（KMS 显示控制、Job Manager、Job Chain 提交、Bifrost UAPI 参数）。
- **Shell 工具**：`drm info`、`drm modes`、`drm display [virtio|panfrost]`、`drm test`、`mali info`、`bifrost info`。