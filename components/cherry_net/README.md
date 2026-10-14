# CherryNet - Multi-Core Multi-Queue TCP/IP Stack & VirtIO Network Driver

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](../../LICENSE)
[![Standards: BSD Sockets / RFC 793 / RFC 8200](https://img.shields.io/badge/Standard-BSD%20Sockets%20%2F%20IPv4%20%2F%20IPv6-blue.svg)]()
[![Platform: FreeRTOS SMP / AArch64](https://img.shields.io/badge/Platform-FreeRTOS%20SMP%20%2F%20AArch64-green.svg)]()

`CherryNet` 是专为嵌入式多核实时操作系统（FreeRTOS SMP）设计的轻量级、高性能、**多核多队列（Multi-Core Multi-Queue）** 双栈（IPv4 & IPv6）网络协议栈与网卡驱动框架。

组件摆脱传统单线程协议栈（如传统 lwIP `tcpip_thread`）跨核锁竞争严重的瓶颈，采用 4-tuple 硬件/软件流哈希引导（RSS / Flow Steering）、核亲和工作线程（Core-Affinity Workers）与独立连接控制块无锁化/细粒度锁设计，提供完整的 POSIX BSD Socket 接口、VirtIO-Net PCI 驱动、iperf2 吞吐基准测试引擎以及 `ifconfig`、`ping` 等网络诊断工具。

---

## 1. 核心架构与功能特性

### 1.1 多核多队列（Multi-Core Multi-Queue RSS Engine）
- **核亲和工作线程**：针对 FreeRTOS SMP 每个 CPU 核心独立创建专属的网络处理线程（`net_worker_0`, `net_worker_1`），通过 `kthread_bind()` 绑定 CPU 亲和掩码。
- **4-Tuple 流哈希（RSS / Flow Steering）**：
  - 基于连接 4 元组 `(src_ip, dst_ip, src_port, dst_port)` 计算 FNV-1a 哈希；
  - 自动将报文引导至对应的 CPU 核心队列，实现同流报文始终在同一核心高速流水线处理，最大化 CPU L1/L2 Cache 局部性，根除跨核大锁瓶颈。
- **无锁化数据队列**：每个核心拥有独立的无锁/轻量自旋锁环形队列（Ring Buffer），高负载下多核并发入队与处理不错乱。

### 1.2 VirtIO-Net PCI 网卡驱动
- **PCI 即插即用**：基于 `cherry_pci` 驱动模型注册，匹配 Vendor ID `0x1AF4` 与 Device ID `0x1000` (Legacy/Transitional) / `0x1041` (Modern)。
- **硬件多队列就绪**：探测 `max_virtqueue_pairs`，支持按 CPU 核心数自动配置 RX/TX 环形队列（`vring_desc`, `vring_avail`, `vring_used`），遵循 4KB 物理对齐规范。
- **MAC 与状态协商**：动态读取硬件 MAC 地址，配置中断（INTx / MSI），自动上报链路与报文收发统计。

### 1.3 双栈协议支持（IPv4 & IPv6 Dual Stack）
- **链路层与 ARP**：
  - 标准 Ethernet II 帧解析与封装；
  - 动态 ARP 缓存表，支持请求生成、自动学习更新与广播解析。
- **IPv4**：
  - 校验和计算与校验、子网路由与网关匹配；
  - ICMPv4 Echo Request / Reply（`ping` 诊断）。
- **IPv6**：
  - RFC 8200 标准 IPv6 报头，支持扩展报头遍历；
  - 自动从 MAC 派生 EUI-64 链路本地地址（Link-Local `fe80::...`）；
  - RFC 4861 邻居发现协议（NDP）：Neighbor Solicitation (NS, Type 135) 与 Neighbor Advertisement (NA, Type 136) 状态机；
  - RFC 2464 以太网多播映射（`33:33:xx:xx:xx:xx`）；
  - ICMPv6 Echo Request / Reply（`ping6` 诊断）。

### 1.4 传输层与完整 TCP 状态机
- **UDP**：
  - 轻量端点控制块（`udp_pcb`），支持动态临时端口分配（Ephemeral Port）；
  - 支持阻塞/超时/非阻塞收发。
- **TCP**：
  - 完整 11 项标准状态机实现（`LISTEN`, `SYN_SENT`, `SYN_RCVD`, `ESTABLISHED`, `FIN_WAIT_1`, `FIN_WAIT_2`, `CLOSE_WAIT`, `CLOSING`, `LAST_ACK`, `TIME_WAIT`）；
  - 3 次握手、序列号与 ACK 跟踪、MSS 分片、滑动窗口机制；
  - 连接控制块（`tcp_pcb`）采用独立锁与独立接收流缓冲区，支持高并发长连接。

### 1.5 POSIX BSD Socket 与 CherryVFS 集成
- **BSD Socket 标准接口**：
  - `socket()`, `bind()`, `connect()`, `listen()`, `accept()`
  - `send()`, `recv()`, `sendto()`, `recvfrom()`
  - `setsockopt()`, `getsockopt()`, `shutdown()`, `close()`
- **CherryVFS 透明桥接**：
  - 注册 `cherry_net_sockdev`，socket 句柄无缝映射至 VFS 文件描述符（File Descriptor）；
  - 支持直接对 Socket FD 调用标准 VFS `read()`, `write()`, `close()`。

### 1.6 网络工具链与 iperf2 基准测试
- **iperf2 协议兼容引擎**：
  - 遵循 iperf 2.0.x / 2.1.x 通信报文格式（`iperf_client_hdr`）；
  - 支持服务器模式（`iperf -s`）与客户端模式（`iperf -c <ip>`）；
  - 基于高精度单调时钟计算带宽（Mbits/sec）并格式化输出标准速率报告；
  - 内置多核并发压力自测套件。
- **`ifconfig`**：列出网卡 IP、掩码、MAC、状态与收发统计，支持动态配置 IP。
- **`ping` / `ping6`**：支持目标 IP 解析、往返时延（RTT）微秒级测量、丢包率统计。
- **CherryShell 命令行导出**：直接在终端输入 `ifconfig`, `ping`, `iperf`, `netstat`。

---

## 2. 软件架构图

```text
+--------------------------------------------------------------------------+
|                     User Application / CherryShell                       |
|         (ifconfig, ping, iperf2 benchmark, bsd sockets, posix io)        |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
|                   CherryVFS / POSIX Socket Interface                     |
|           socket(), bind(), connect(), listen(), accept(), send/recv     |
+--------------------------------------------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
|               Multi-Core Multi-Queue TCP/IP Protocol Stack               |
|  +------------------------------------+  +-----------------------------+ |
|  |                TCP                 |  |             UDP             | |
|  | (11-State Machine, Sliding Window) |  |   (Datagram Endpoints)      | |
|  +------------------------------------+  +-----------------------------+ |
|  +------------------------------------+  +-----------------------------+ |
|  |           IPv4 & ICMPv4            |  |        IPv6 & ICMPv6        | |
|  |      (Routing, ARP Table)          |  |       (NDP, EUI-64)         | |
|  +------------------------------------+  +-----------------------------+ |
|  +--------------------------------------------------------------------+  |
|  |                4-Tuple RSS Flow Steering & Queue Rings             |  |
|  |       Core 0 Worker Queue          |      Core 1 Worker Queue      |  |
|  +--------------------------------------------------------------------+  |
+--------------------------------------------------------------------------+
                                     |
                    +----------------+----------------+
                    |                                 |
                    v                                 v
+---------------------------------------+  +-------------------------------+
|     VirtIO-Net PCI Network Driver     |  |   Loopback Device (lo0)       |
| (Multi-Queue Rings, MAC, MSI/INTx)    |  |  (127.0.0.1, ::1 Direct Loop) |
+---------------------------------------+  +-------------------------------+
                    |
                    v
+--------------------------------------------------------------------------+
|           CherryPCI Host Bridge / QEMU VirtIO Network Device             |
+--------------------------------------------------------------------------+
```

---

## 3. 验收测试验证输出

在 QEMU virt AArch64 环境下运行测试套件，输出标准 Linux 风格小写日志：

```text
cherry_net: initializing multi-core tcp/ip network subsystem...
cherry_net: probing virtio-net pci device at [00:01.0]...
cherry_net: virtio-net hwaddr 52:54:00:12:34:56
cherry_net: virtio-net eth0 registered successfully (irq=36)
cherry_net: subsystem initialized successfully
cherry_net: ==> starting cherrynet acceptance test suite...
cherry_net: [ifconfig] lo0: flags=<UP,LOOPBACK,RUNNING> mtu 16384
cherry_net: [ifconfig]         inet 127.0.0.1 netmask 255.0.0.0
cherry_net: [ifconfig]         inet6 fe80::0000:0000:0000:0001 prefixlen 64
cherry_net: [ifconfig] eth0: flags=<UP,BROADCAST,RUNNING> mtu 1500
cherry_net: [ifconfig]         inet 10.0.2.15 netmask 255.255.255.0
cherry_net: [ifconfig]         inet6 fe80::5054:ff:fe12:3456 prefixlen 64
cherry_net: [ifconfig]         ether 52:54:00:12:34:56
cherry_net: [ping] ping 127.0.0.1 (lo0) 56 data bytes
cherry_net: [ping] 64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.10 ms
cherry_net: [ping] 64 bytes from 127.0.0.1: icmp_seq=2 ttl=64 time=0.08 ms
cherry_net: [ping] --- 127.0.0.1 ping statistics ---
cherry_net: [ping] 2 packets transmitted, 2 received, 0% packet loss
cherry_net: [ping] ping ::1 (lo0) 56 data bytes
cherry_net: [ping] 64 bytes from ::1: icmp_seq=1 ttl=64 time=0.12 ms
cherry_net: [ping] 64 bytes from ::1: icmp_seq=2 ttl=64 time=0.09 ms
cherry_net: [ping] --- ::1 ping statistics ---
cherry_net: [ping] 2 packets transmitted, 2 received, 0% packet loss
cherry_net: starting iperf2 multi-core throughput benchmark...
cherry_net: [iperf] [  3]  0.0- 0.8 sec  1.0 MBytes  10.2 Mbits/sec
cherry_net: [net-verify] virtio=pass ifconfig=pass mq_rss=pass ipv4_arp=pass ipv6_ndp=pass ping=pass udp=pass tcp=pass iperf2=pass all=pass
```
