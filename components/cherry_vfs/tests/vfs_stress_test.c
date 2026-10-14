/*
 * CherryVFS - Large-scale File Operation Stress & Concurrency Test Suite
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 */

#include "vfs_stress_test.h"
#include "cherry_vfs.h"
#include "cherry_kal.h"
#include "uart.h" /* snprintf */
#include <string.h>

#define STRESS_DEFAULT_FILES 50
#define STRESS_CHUNK_SIZE    256

/* =========================================================================
 * Phase 1: Deep Nested Directories (mkdir -p / traversal / stat)
 * ========================================================================= */
static bool test_deep_hierarchy(void)
{
    mkdir("/stress", 0777);
    mkdir("/stress/lvl1", 0777);
    mkdir("/stress/lvl1/lvl2", 0777);
    mkdir("/stress/lvl1/lvl2/lvl3", 0777);
    mkdir("/stress/lvl1/lvl2/lvl3/lvl4", 0777);

    struct stat st;
    if (stat("/stress/lvl1/lvl2/lvl3/lvl4", &st) != 0 || !S_ISDIR(st.st_mode)) {
        pr_err("[vfs-stress] failed to stat deep directory\n");
        return false;
    }

    /* Create file at the deepest level */
    int fd = open("/stress/lvl1/lvl2/lvl3/lvl4/canary.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) {
        pr_err("[vfs-stress] failed to create file in deep directory\n");
        return false;
    }
    const char msg[] = "deep_canary_verified";
    write(fd, msg, strlen(msg));
    close(fd);

    /* Verify directory traversal */
    DIR *dir = opendir("/stress/lvl1/lvl2/lvl3/lvl4");
    if (!dir) {
        pr_err("[vfs-stress] failed to opendir deep directory\n");
        return false;
    }
    bool found_canary = false;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, "canary.txt") == 0) {
            found_canary = true;
        }
    }
    closedir(dir);

    return found_canary;
}

/* =========================================================================
 * Phase 2: Massive Batch File Operations (Create, Write, Read, Stat, Unlink)
 * ========================================================================= */
static bool test_batch_files(int count)
{
    mkdir("/stress/batch", 0777);

    char path[64];
    char write_buf[64];
    char read_buf[64];

    /* 1. Batch Create & Write */
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/stress/batch/file_%03d.dat", i);
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd < 0) {
            pr_err("[vfs-stress] batch create failed at %d (fd=%d)\n", i, fd);
            return false;
        }
        snprintf(write_buf, sizeof(write_buf), "PATTERN_MAGIC_0x%04X_FILE_%d", i * 17, i);
        ssize_t written = write(fd, write_buf, strlen(write_buf));
        close(fd);

        if (written != (ssize_t)strlen(write_buf)) {
            pr_err("[vfs-stress] batch write failed at %d\n", i);
            return false;
        }
    }

    /* 2. Batch Read & Verification */
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/stress/batch/file_%03d.dat", i);
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            pr_err("[vfs-stress] batch reopen failed at %d\n", i);
            return false;
        }
        memset(read_buf, 0, sizeof(read_buf));
        ssize_t r = read(fd, read_buf, sizeof(read_buf) - 1);
        close(fd);

        snprintf(write_buf, sizeof(write_buf), "PATTERN_MAGIC_0x%04X_FILE_%d", i * 17, i);
        if (r != (ssize_t)strlen(write_buf) || strcmp(read_buf, write_buf) != 0) {
            pr_err("[vfs-stress] batch read mismatch at %d: got '%s' expected '%s'\n", i, read_buf, write_buf);
            return false;
        }

        struct stat st;
        if (stat(path, &st) != 0 || st.st_size != (off_t)strlen(write_buf) || !S_ISREG(st.st_mode)) {
            pr_err("[vfs-stress] batch stat check failed at %d\n", i);
            return false;
        }
    }

    /* 3. Batch Unlink */
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof(path), "/stress/batch/file_%03d.dat", i);
        if (unlink(path) != 0) {
            pr_err("[vfs-stress] batch unlink failed at %d\n", i);
            return false;
        }
    }

    /* Confirm directory is clean */
    DIR *dir = opendir("/stress/batch");
    if (!dir) return false;
    int remaining = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        remaining++;
    }
    closedir(dir);

    rmdir("/stress/batch");
    return (remaining == 0);
}

/* =========================================================================
 * Phase 3: Large Multi-Block File I/O, Seek, Append & Truncate
 * ========================================================================= */
static bool test_large_seek_truncate(void)
{
    const char *path = "/stress/large.bin";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        pr_err("[vfs-stress] failed to create large.bin\n");
        return false;
    }

    /* Write 8KB of pattern data */
    char chunk[STRESS_CHUNK_SIZE];
    for (int i = 0; i < STRESS_CHUNK_SIZE; i++) {
        chunk[i] = (char)('A' + (i % 26));
    }
    for (int b = 0; b < 32; b++) { /* 32 * 256 = 8192 bytes */
        if (write(fd, chunk, STRESS_CHUNK_SIZE) != STRESS_CHUNK_SIZE) {
            pr_err("[vfs-stress] large file write failed\n");
            close(fd);
            return false;
        }
    }

    /* Test random seek: seek to 4096 and write marker */
    off_t pos = lseek(fd, 4096, SEEK_SET);
    if (pos != 4096) {
        pr_err("[vfs-stress] lseek to 4096 failed\n");
        close(fd);
        return false;
    }
    const char marker[] = "MIDDLE_MARKER_0x1234";
    write(fd, marker, strlen(marker));

    /* Seek back and verify marker */
    lseek(fd, 4096, SEEK_SET);
    char check_buf[32];
    memset(check_buf, 0, sizeof(check_buf));
    read(fd, check_buf, strlen(marker));
    if (strcmp(check_buf, marker) != 0) {
        pr_err("[vfs-stress] seek marker mismatch: '%s'\n", check_buf);
        close(fd);
        return false;
    }

    /* Test seek end and append */
    lseek(fd, 0, SEEK_END);
    const char append_tag[] = "TAIL_APPEND_VERIFIED";
    write(fd, append_tag, strlen(append_tag));

    struct stat st;
    fstat(fd, &st);
    if (st.st_size != (off_t)(8192 + strlen(append_tag))) {
        pr_err("[vfs-stress] append size check failed (size=%ld)\n", (long)st.st_size);
        close(fd);
        return false;
    }

    /* Test truncation down to 4096 bytes */
    ftruncate(fd, 4096);
    fstat(fd, &st);
    if (st.st_size != 4096) {
        pr_err("[vfs-stress] ftruncate failed (size=%ld)\n", (long)st.st_size);
        close(fd);
        return false;
    }

    close(fd);
    unlink(path);
    return true;
}

/* =========================================================================
 * Phase 4: File Move & Rename
 * ========================================================================= */
static bool test_rename(void)
{
    const char *src = "/stress/rename_src.txt";
    const char *dst = "/stress/rename_dst.txt";

    int fd = open(src, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd < 0) return false;
    const char content[] = "rename_atomic_payload_test";
    write(fd, content, strlen(content));
    close(fd);

    if (rename(src, dst) != 0) {
        pr_err("[vfs-stress] rename failed\n");
        return false;
    }

    /* Ensure old path no longer exists */
    int old_fd = open(src, O_RDONLY);
    if (old_fd >= 0) {
        close(old_fd);
        pr_err("[vfs-stress] old file still exists after rename\n");
        return false;
    }

    /* Ensure new path has correct content */
    int new_fd = open(dst, O_RDONLY);
    if (new_fd < 0) {
        pr_err("[vfs-stress] failed to open new file after rename\n");
        return false;
    }
    char buf[64];
    memset(buf, 0, sizeof(buf));
    read(new_fd, buf, sizeof(buf) - 1);
    close(new_fd);

    unlink(dst);
    return (strcmp(buf, content) == 0);
}

/* =========================================================================
 * Phase 5: Concurrent Multi-Thread File Stress (SMP Safe)
 * ========================================================================= */
struct stress_worker_arg {
    int id;
    int iterations;
    struct semaphore *done_sem;
    volatile bool ok;
};

static int stress_worker_thread(void *data)
{
    struct stress_worker_arg *arg = (struct stress_worker_arg *)data;
    char path[64];
    snprintf(path, sizeof(path), "/stress/conc_w%d.dat", arg->id);

    char wbuf[64];
    char rbuf[64];

    for (int i = 0; i < arg->iterations; i++) {
        int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if (fd < 0) {
            arg->ok = false;
            break;
        }
        snprintf(wbuf, sizeof(wbuf), "WORKER_%d_ITER_%d_PAYLOAD", arg->id, i);
        write(fd, wbuf, strlen(wbuf));
        close(fd);

        fd = open(path, O_RDONLY);
        if (fd < 0) {
            arg->ok = false;
            break;
        }
        memset(rbuf, 0, sizeof(rbuf));
        read(fd, rbuf, sizeof(rbuf) - 1);
        close(fd);

        if (strcmp(wbuf, rbuf) != 0) {
            arg->ok = false;
            break;
        }

        unlink(path);
        msleep(1);
    }

    up(arg->done_sem);
    return 0;
}

static bool test_concurrent_smp(void)
{
    struct semaphore done_sem;
    sema_init(&done_sem, 0);

    struct stress_worker_arg arg0 = { .id = 0, .iterations = 25, .done_sem = &done_sem, .ok = true };
    struct stress_worker_arg arg1 = { .id = 1, .iterations = 25, .done_sem = &done_sem, .ok = true };

    struct task_struct *t0 = kthread_run(stress_worker_thread, &arg0, "vfs_w0");
    struct task_struct *t1 = kthread_run(stress_worker_thread, &arg1, "vfs_w1");

    if (t0) kthread_bind(t0, 0);
    if (t1) kthread_bind(t1, 1);

    down(&done_sem);
    down(&done_sem);

    return (arg0.ok && arg1.ok);
}

/* =========================================================================
 * Master Entry Point for Stress Tests
 * ========================================================================= */
int vfs_run_stress_tests(int file_count)
{
    if (file_count <= 0) {
        file_count = STRESS_DEFAULT_FILES;
    }

    pr_info("[vfs-stress] starting large-scale file operation stress tests (batch_files=%d)...\n", file_count);

    bool p1 = test_deep_hierarchy();
    bool p2 = test_batch_files(file_count);
    bool p3 = test_large_seek_truncate();
    bool p4 = test_rename();
    bool p5 = test_concurrent_smp();

    bool all_ok = (p1 && p2 && p3 && p4 && p5);

    pr_info("[vfs-stress] hierarchy=%s batch_files=%s large_seek_trunc=%s rename=%s concurrent_smp=%s all=%s\n",
            p1 ? "pass" : "fail",
            p2 ? "pass" : "fail",
            p3 ? "pass" : "fail",
            p4 ? "pass" : "fail",
            p5 ? "pass" : "fail",
            all_ok ? "pass" : "fail");

    return all_ok ? 0 : -1;
}
