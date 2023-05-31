#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Was MOUNT_MIXED 1, this spot could be reused. */
#define MOUNT_BINARY 2
/* Was MOUNT_SILENT 4, this spot could be reused. */
#define MOUNT_SYSTEM 8
/* Possibly we'll want to define MOUNT_EXEC 16 here. */
#define MOUNT_AUTO 32

int mount (const char *, const char *, int __flags);
int umount (const char *);
int cygwin_umount (const char *__path, const int __flags);

#ifdef __cplusplus
};
#endif

#endif /* _SYS_MOUNT_H */
