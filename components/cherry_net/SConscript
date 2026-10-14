# -*- python -*-
from SCons.Script import *

Import('env')

net_env = env.Clone()

net_env.Append(CPPPATH=[
    '#components/cherry_net/include',
    '#components/cherry_net/tests',
    '#components/cherry_vfs/include',
    '#components/cherry_vfs/tests',
    '#components/cherry_pci/include',
    '#components/cherry_pci/tests',
    '#components/cherry_kal/include',
    '#components/cherry_printk/include',
    '#components/cherry_timer/include',
    '#components/cherry_shell/include',
    '#include',
    '#bsp/include',
    '#arch/include',
])

sources = [
    'src/cherry_net.c',
    'src/net_buf.c',
    'src/net_if.c',
    'src/net_queue.c',
    'src/net_loopback.c',
    'src/net_eth.c',
    'src/net_ip4.c',
    'src/net_ip6.c',
    'src/net_icmp.c',
    'src/net_udp.c',
    'src/net_tcp.c',
    'src/net_socket.c',
    'src/net_virtio.c',
    'src/net_iperf2.c',
    'src/net_ping.c',
    'src/net_ifconfig.c',
    'src/net_cmd.c',
    'tests/net_test.c',
]

net_objs = net_env.Object(sources)

cherry_net_lib = net_env.Library(
    target='libcherry_net.a',
    source=net_objs
)

Return('cherry_net_lib')
