/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * Acceptance and Verification Test Suite Implementation
 */

#include "cherry_vfs.h"
#include "vfs_test.h"
#include <string.h>

extern int printk(const char *fmt, ...);

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printk("<3>[vfs-fail] %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return -1; \
        } \
    } while (0)

static int test_platform_bus(void)
{
    /* 1. Verify Platform Bus existence */
    struct vfs_bus_type *bus = vfs_bus_find("platform");
    TEST_ASSERT(bus != NULL, "Platform bus must exist");

    /* 2. Verify PL011 UART Platform Driver registration */
    struct vfs_driver *drv = vfs_driver_find("pl011_uart");
    TEST_ASSERT(drv != NULL, "PL011 platform driver must be registered");

    /* 3. Verify PL011 Device is bound to PL011 Driver */
    struct vfs_device *dev = vfs_device_find("pl011_uart");
    TEST_ASSERT(dev != NULL, "PL011 platform device must be registered");
    TEST_ASSERT(dev->driver == drv, "PL011 platform device must be bound to driver");

    return 0;
}

static int test_cdev_null_zero(void)
{
    /* Test /dev/null */
    int fd_null = open("/dev/null", O_WRONLY);
    TEST_ASSERT(fd_null >= 0, "open /dev/null for write");
    static const char write_buf[] = "CherryVFS Null Device Test";
    ssize_t written = write(fd_null, write_buf, sizeof(write_buf));
    TEST_ASSERT(written == (ssize_t)sizeof(write_buf), "write to /dev/null should return count");
    close(fd_null);


    int fd_null_rd = open("/dev/null", O_RDONLY);
    TEST_ASSERT(fd_null_rd >= 0, "open /dev/null for read");
    char read_buf[32];
    ssize_t nread = read(fd_null_rd, read_buf, sizeof(read_buf));
    TEST_ASSERT(nread == 0, "read /dev/null must return 0 (EOF)");
    close(fd_null_rd);

    /* Test /dev/zero */
    int fd_zero = open("/dev/zero", O_RDONLY);
    TEST_ASSERT(fd_zero >= 0, "open /dev/zero");
    memset(read_buf, 0xAA, sizeof(read_buf));
    nread = read(fd_zero, read_buf, 16);
    TEST_ASSERT(nread == 16, "read 16 bytes from /dev/zero");
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(read_buf[i] == 0, "/dev/zero bytes must be 0");
    }
    close(fd_zero);

    return 0;
}

static int test_cdev_uart_console(void)
{
    int fd = open("/dev/console", O_WRONLY);
    TEST_ASSERT(fd >= 0, "open /dev/console");

    static const char msg[] = "cherry_vfs: [/dev/console] posix write through cdev successful\n";
    ssize_t w = write(fd, msg, strlen(msg));
    TEST_ASSERT(w == (ssize_t)strlen(msg), "write to /dev/console");
    close(fd);


    return 0;
}

static int test_bdev_ramdisk(void)
{
    int fd = open("/dev/ram0", O_RDWR);
    TEST_ASSERT(fd >= 0, "open /dev/ram0");

    /* IOCTL tests */
    uint32_t sector_size = 0;
    int ret = ioctl(fd, BLOCK_IOCTL_GET_SECTOR_SIZE, (unsigned long)&sector_size);
    TEST_ASSERT(ret == 0 && sector_size == 512, "ram0 sector size must be 512");

    uint64_t sector_count = 0;
    ret = ioctl(fd, BLOCK_IOCTL_GET_SECTOR_COUNT, (unsigned long)&sector_count);
    TEST_ASSERT(ret == 0 && sector_count == 64, "ram0 sector count must be 64");

    /* Write pattern across 2 blocks (1024 bytes) */
    static uint8_t tx_pattern[1024];
    for (size_t i = 0; i < sizeof(tx_pattern); i++) {
        tx_pattern[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    }
    ssize_t w = write(fd, tx_pattern, sizeof(tx_pattern));
    TEST_ASSERT(w == (ssize_t)sizeof(tx_pattern), "write 2 blocks to ram0");

    /* Seek back to 0 */
    off_t pos = lseek(fd, 0, SEEK_SET);
    TEST_ASSERT(pos == 0, "lseek to 0");

    /* Read back and verify */
    static uint8_t rx_pattern[1024];
    memset(rx_pattern, 0, sizeof(rx_pattern));
    ssize_t r = read(fd, rx_pattern, sizeof(rx_pattern));
    TEST_ASSERT(r == (ssize_t)sizeof(rx_pattern), "read 2 blocks from ram0");
    TEST_ASSERT(memcmp(tx_pattern, rx_pattern, sizeof(tx_pattern)) == 0, "ram0 readback must match written pattern");

    close(fd);
    return 0;
}

static int test_ramfs_file_and_dir(void)
{
    /* 1. Create directory */
    int ret = mkdir("/etc", 0755);
    TEST_ASSERT(ret == 0, "mkdir /etc");

    /* 2. Create regular file inside directory */
    int fd = open("/etc/config.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "create /etc/config.txt");

    static const char text[] = "smp_cores=2\nrtos=FreeRTOS-V11\nvfs=CherryVFS\n";
    size_t len = strlen(text);
    ssize_t w = write(fd, text, len);

    TEST_ASSERT(w == (ssize_t)len, "write to /etc/config.txt");

    /* 3. Stat test */
    struct stat st;
    ret = fstat(fd, &st);
    TEST_ASSERT(ret == 0 && st.st_size == (off_t)len, "fstat size match");

    ret = stat("/etc/config.txt", &st);
    TEST_ASSERT(ret == 0 && S_ISREG(st.st_mode) && st.st_size == (off_t)len, "stat /etc/config.txt");

    /* 4. Seek and read back */
    lseek(fd, 0, SEEK_SET);
    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t r = read(fd, buf, sizeof(buf));
    TEST_ASSERT(r == (ssize_t)len, "read from /etc/config.txt");
    TEST_ASSERT(strcmp(buf, text) == 0, "content match");
    close(fd);

    /* 5. Directory traversal */
    DIR *dirp = opendir("/etc");
    TEST_ASSERT(dirp != NULL, "opendir /etc");
    struct dirent *de = readdir(dirp);
    TEST_ASSERT(de != NULL, "readdir /etc");
    TEST_ASSERT(strcmp(de->d_name, "config.txt") == 0, "entry name config.txt");
    closedir(dirp);

    /* 6. DevFS directory traversal */
    dirp = opendir("/dev");
    TEST_ASSERT(dirp != NULL, "opendir /dev");
    int found_uart = 0, found_ram = 0, found_null = 0;
    while ((de = readdir(dirp)) != NULL) {
        if (strcmp(de->d_name, "uart0") == 0) found_uart = 1;
        if (strcmp(de->d_name, "ram0") == 0) found_ram = 1;
        if (strcmp(de->d_name, "null") == 0) found_null = 1;
    }
    closedir(dirp);
    TEST_ASSERT(found_uart && found_ram && found_null, "devfs contains uart0, ram0, null");

    return 0;
}

static int test_socket_abstraction(void)
{
    /* 1. Create socket */
    int sfd = socket(0, 0, 0);
    TEST_ASSERT(sfd >= 0, "socket() call");

    /* 2. Bind */
    int ret = bind(sfd, NULL, 0);
    TEST_ASSERT(ret == 0, "bind() call");

    /* 3. Send over socket */
    static const char msg[] = "CHERRY_SOCKET_PING";
    ssize_t s = send(sfd, msg, strlen(msg), 0);

    TEST_ASSERT(s == (ssize_t)strlen(msg), "send() call");

    /* 4. Recv over socket */
    char rx[32];
    memset(rx, 0, sizeof(rx));
    ssize_t r = recv(sfd, rx, sizeof(rx), 0);
    TEST_ASSERT(r == (ssize_t)strlen(msg), "recv() call");
    TEST_ASSERT(strcmp(rx, msg) == 0, "socket payload match");

    close(sfd);
    return 0;
}

int cherry_vfs_run_tests(void)
{
    printk("<6>cherry_vfs: ==> starting cherryvfs acceptance test suite...\n");

    int pdev_ok = (test_platform_bus() == 0);
    int cdev_null_ok = (test_cdev_null_zero() == 0);
    int cdev_uart_ok = (test_cdev_uart_console() == 0);
    int bdev_ok = (test_bdev_ramdisk() == 0);
    int ramfs_ok = (test_ramfs_file_and_dir() == 0);
    int sock_ok = (test_socket_abstraction() == 0);

    int cdev_ok = (cdev_null_ok && cdev_uart_ok);
    int all_ok = (pdev_ok && cdev_ok && bdev_ok && ramfs_ok && sock_ok);

    printk("<6>cherry_vfs: [vfs-verify] pdev=%s cdev=%s bdev=%s ramfs=%s sock=%s all=%s\n",
           pdev_ok ? "pass" : "fail",
           cdev_ok ? "pass" : "fail",
           bdev_ok ? "pass" : "fail",
           ramfs_ok ? "pass" : "fail",
           sock_ok ? "pass" : "fail",
           all_ok ? "pass" : "fail");

    return all_ok ? 0 : -1;
}
