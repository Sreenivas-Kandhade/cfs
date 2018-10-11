#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/statfs.h>
#include <stdlib.h>
#include <libgen.h>
#ifdef HAVE_SETXATTR
# include <sys/xattr.h>
#endif

#include "constants.h"
#include "main.h"
#include "391.h"
#include "394.h"
#include "369.h"

#ifdef HAVE_SETXATTR

static struct fuse_operations cfs_oper =
{
    .getattr = cfs_getattr,
    .readlink = cfs_readlink,
    .getdir = cfs_getdir,
    .mknod = cfs_mknod,
    .mkdir = cfs_mkdir,
    .symlink = cfs_symlink,
    .unlink = cfs_unlink,
    .rmdir = cfs_rmdir,
    .rename = cfs_rename,
    .link = cfs_link,
    .chmod = cfs_chmod,
    .chown = cfs_chown,
    .truncate = cfs_truncate,
    .utime = cfs_utime,
    .open = cfs_open,
    .read = cfs_read,
    .write = cfs_write,
    .statfs = cfs_statfs,
    .release = cfs_release,
    .fsync = cfs_fsync,
    #ifdef HAVE_SETXATTR
        .setxattr = cfs_setxattr,
        .getxattr = cfs_getxattr,
        .listxattr = cfs_listxattr,
        .removexattr = cfs_removexattr,
    #endif
};

int main (int argc, char *argv[])
{
    int ret = 0;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    ret = fuse_main (argc, argv, &cfs_oper);
    return ret;
}
