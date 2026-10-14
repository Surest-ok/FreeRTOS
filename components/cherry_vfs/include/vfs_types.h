/*
 * CherryVFS - Virtual File System for FreeRTOS
 *
 * Copyright (c) 2026 ForgeStitch
 * SPDX-License-Identifier: MIT
 *
 * POSIX Types, Constants, and Data Structures
 */

#ifndef _CHERRY_VFS_TYPES_H_
#define _CHERRY_VFS_TYPES_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard POSIX basic types */
typedef int32_t         mode_t;
typedef int64_t         off_t;
typedef long            ssize_t;
typedef uint32_t        ino_t;
typedef uint32_t        dev_t;
typedef uint32_t        uid_t;
typedef uint32_t        gid_t;
typedef int32_t         pid_t;

#define VFS_NAME_MAX    64
#define VFS_PATH_MAX    256
#define VFS_MAX_FD      64

/* File Type Bits in mode_t (matching standard POSIX) */
#define S_IFMT          00170000
#define S_IFSOCK        00140000
#define S_IFLNK         00120000
#define S_IFREG         00100000
#define S_IFBLK         00060000
#define S_IFDIR         00040000
#define S_IFCHR         00020000
#define S_IFIFO         00010000

#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)

/* File Permission Bits */
#define S_ISUID         0004000
#define S_ISGID         0002000
#define S_ISVTX         0001000

#define S_IRWXU         00700
#define S_IRUSR         00400
#define S_IWUSR         00200
#define S_IXUSR         00100

#define S_IRWXG         00070
#define S_IRGRP         00040
#define S_IWGRP         00020
#define S_IXGRP         00010

#define S_IRWXO         00007
#define S_IROTH         00004
#define S_IWOTH         00002
#define S_IXOTH         00001

/* File Access Modes and Flags */
#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_ACCMODE       0x0003

#define O_CREAT         0x0040
#define O_EXCL          0x0080
#define O_NOCTTY        0x0100
#define O_TRUNC         0x0200
#define O_APPEND        0x0400
#define O_NONBLOCK      0x0800
#define O_SYNC          0x1000
#define O_DIRECTORY     0x10000

/* Seek Whence Constants */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* Directory Entry Types */
#define DT_UNKNOWN      0
#define DT_FIFO         1
#define DT_CHR          2
#define DT_DIR          4
#define DT_BLK          6
#define DT_REG          8
#define DT_LNK          10
#define DT_SOCK         12

/* POSIX struct stat */
struct stat {
    dev_t       st_dev;      /* ID of device containing file */
    ino_t       st_ino;      /* Inode number */
    mode_t      st_mode;     /* File type and mode */
    uint32_t    st_nlink;    /* Number of hard links */
    uid_t       st_uid;      /* User ID of owner */
    gid_t       st_gid;      /* Group ID of owner */
    dev_t       st_rdev;     /* Device ID (if special file) */
    off_t       st_size;     /* Total size, in bytes */
    uint32_t    st_blksize;  /* Block size for filesystem I/O */
    uint32_t    st_blocks;   /* Number of 512B blocks allocated */
    uint64_t    st_atime;    /* Time of last access */
    uint64_t    st_mtime;    /* Time of last modification */
    uint64_t    st_ctime;    /* Time of last status change */
};

/* POSIX struct dirent */
struct dirent {
    ino_t       d_ino;                /* Inode number */
    uint8_t     d_type;               /* Type of file (DT_*) */
    char        d_name[VFS_NAME_MAX]; /* Null-terminated filename */
};

/* POSIX-compatible Error Codes */
#define VFS_OK          0
#define VFS_ERROR      -1

#ifndef EPERM
#define EPERM           1   /* Operation not permitted */
#endif
#ifndef ENOENT
#define ENOENT          2   /* No such file or directory */
#endif
#ifndef ESRCH
#define ESRCH           3   /* No such process */
#endif
#ifndef EINTR
#define EINTR           4   /* Interrupted system call */
#endif
#ifndef EIO
#define EIO             5   /* I/O error */
#endif
#ifndef ENXIO
#define ENXIO           6   /* No such device or address */
#endif
#ifndef E2BIG
#define E2BIG           7   /* Argument list too long */
#endif
#ifndef ENOEXEC
#define ENOEXEC         8   /* Exec format error */
#endif
#ifndef EBADF
#define EBADF           9   /* Bad file descriptor */
#endif
#ifndef ECHILD
#define ECHILD          10  /* No child processes */
#endif
#ifndef EAGAIN
#define EAGAIN          11  /* Try again */
#endif
#ifndef ENOMEM
#define ENOMEM          12  /* Out of memory */
#endif
#ifndef EACCES
#define EACCES          13  /* Permission denied */
#endif
#ifndef EFAULT
#define EFAULT          14  /* Bad address */
#endif
#ifndef EBUSY
#define EBUSY           16  /* Device or resource busy */
#endif
#ifndef EEXIST
#define EEXIST          17  /* File exists */
#endif
#ifndef EXDEV
#define EXDEV           18  /* Cross-device link */
#endif
#ifndef ENODEV
#define ENODEV          19  /* No such device */
#endif
#ifndef ENOTDIR
#define ENOTDIR         20  /* Not a directory */
#endif
#ifndef EISDIR
#define EISDIR          21  /* Is a directory */
#endif
#ifndef EINVAL
#define EINVAL          22  /* Invalid argument */
#endif
#ifndef ENFILE
#define ENFILE          23  /* File table overflow */
#endif
#ifndef EMFILE
#define EMFILE          24  /* Too many open files */
#endif
#ifndef ENOTTY
#define ENOTTY          25  /* Not a typewriter */
#endif
#ifndef ENOSPC
#define ENOSPC          28  /* No space left on device */
#endif
#ifndef ESPIPE
#define ESPIPE          29  /* Illegal seek */
#endif
#ifndef EROFS
#define EROFS           30  /* Read-only file system */
#endif
#ifndef EMLINK
#define EMLINK          31  /* Too many links */
#endif
#ifndef ENOSYS
#define ENOSYS          38  /* Function not implemented */
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY       39  /* Directory not empty */
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP      95  /* Operation not supported on transport endpoint */
#endif
#ifndef ECONNABORTED
#define ECONNABORTED    103 /* Software caused connection abort */
#endif


#ifdef __cplusplus
}
#endif

#endif /* _CHERRY_VFS_TYPES_H_ */
