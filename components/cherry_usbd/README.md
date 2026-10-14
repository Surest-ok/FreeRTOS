# CherryUSBD - USB 2.0 / 3.0 Device Protocol Stack

`cherry_usbd` 是为 CherryOS / FreeRTOS 深度研发的现代化 USB Device 协议栈与设备子系统。具备清晰的 UDC (USB Device Controller) 硬件抽象层、完整的 USB 规范第 9 章 (Chapter 9) 请求解析状态机、以及主流 USB 设备类驱动。

---

## 架构特性

- **标准 Chapter 9 框架与状态机**：
  - 支持 `GET_DESCRIPTOR`、`SET_ADDRESS`、`SET_CONFIGURATION`、`GET_STATUS`、`CLEAR_FEATURE`、`SET_FEATURE` 等标准控制传输；
  - 规范的状态机演进：`DEFAULT` -> `ADDRESSED` -> `CONFIGURED`；
  - 复合设备配置描述符 (Composite Gadget)，集成了 CDC-ACM、MSC BOT 与 HID 键盘/鼠标。
- **虚拟 UDC (Virtual UDC Loopback Driver)**：
  - 在缺乏物理外设 UDC 芯片的环境 (如 QEMU virt AArch64) 下，提供基于端点环形缓冲区的虚拟 UDC 驱动；
  - 支持在内核态内环回验证控制传输 Setup 事务、批量端点传输与中断端点通知。
- **CDC-ACM 虚拟串口**：
  - 遵循 USB CDC 规范与 ACM Abstract Control Model；
  - 动态响应 `SET_LINE_CODING`、`GET_LINE_CODING` (支持高达 921600 波特率) 与 `SET_CONTROL_LINE_STATE`；
  - 自动注册 CherryVFS 字符设备 `/dev/ttyUSB0`，无缝对接标准 read/write/ioctl 文件接口。
- **MSC 块存储 (Bulk-Only Transport & SCSI)**：
  - 规范处理 31 字节 Command Block Wrapper (`CBW`) 与 13 字节 Command Status Wrapper (`CSW`)；
  - 内置 SCSI-2 命令引擎：`INQUIRY`、`TEST_UNIT_READY`、`READ_CAPACITY_10`、`REQUEST_SENSE`、`READ_10`、`WRITE_10`、`MODE_SENSE_6` 等；
  - 支持虚拟磁盘与 CherryVFS 块存储后备对接。
- **HID 人机交互设备**：
  - 标准 63 字节键盘报告描述符；
  - 支持键盘按键与鼠标位移输入事件打包发送。
- **CherryShell 交互命令**：
  - `usb_info`：打印 USB 设备状态、当前地址、配置值与端点类状态；
  - `usb_cdc_echo [msg]`：向 `/dev/ttyUSB0` 写入数据并验证回显。
- **自动化验收测试**：
  - 内置 8 项全覆盖自动化验证，输出标准小写 `[usbd-verify]` 标记。
