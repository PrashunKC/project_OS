/*
 * NBOS Virtual File System (VFS)
 * 
 * Provides an abstraction layer for file systems and devices.
 * Drivers register their file operations here.
 */

#ifndef _VFS_H
#define _VFS_H

#include "stdint.h"

/* ============================================================================
 * VFS Limits and Constants
 * ============================================================================ */

#define VFS_MAX_PATH        256
#define VFS_MAX_NAME        64
#define VFS_MAX_FILESYSTEMS 16
#define VFS_MAX_MOUNTS      32
#define VFS_MAX_OPEN_FILES  256

/* ============================================================================
 * File Types
 * ============================================================================ */

#define VFS_TYPE_FILE       1
#define VFS_TYPE_DIR        2
#define VFS_TYPE_CHARDEV    3   // Character device (e.g., /dev/tty)
#define VFS_TYPE_BLOCKDEV   4   // Block device (e.g., /dev/sda)
#define VFS_TYPE_PIPE       5
#define VFS_TYPE_SYMLINK    6
#define VFS_TYPE_SOCKET     7

/* ============================================================================
 * File Open Flags
 * ============================================================================ */

#define VFS_O_RDONLY        0x0000
#define VFS_O_WRONLY        0x0001
#define VFS_O_RDWR          0x0002
#define VFS_O_CREAT         0x0040
#define VFS_O_EXCL          0x0080
#define VFS_O_TRUNC         0x0200
#define VFS_O_APPEND        0x0400
#define VFS_O_NONBLOCK      0x0800
#define VFS_O_DIRECTORY     0x10000

/* ============================================================================
 * Seek Positions
 * ============================================================================ */

#define VFS_SEEK_SET        0
#define VFS_SEEK_CUR        1
#define VFS_SEEK_END        2

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct vfs_node;
struct vfs_mount;
struct vfs_file;
struct vfs_dirent;
struct vfs_stat;

/* ============================================================================
 * File Operations Structure
 * 
 * Each filesystem/device driver provides these operations.
 * ============================================================================ */

typedef struct vfs_file_ops {
    // File operations
    int64_t (*read)(struct vfs_file *file, void *buf, uint64_t count);
    int64_t (*write)(struct vfs_file *file, const void *buf, uint64_t count);
    int64_t (*seek)(struct vfs_file *file, int64_t offset, int whence);
    int (*close)(struct vfs_file *file);
    int (*ioctl)(struct vfs_file *file, uint64_t request, void *arg);
    int (*mmap)(struct vfs_file *file, void *addr, uint64_t length, int prot, int flags);
    
    // Directory operations
    int (*readdir)(struct vfs_file *file, struct vfs_dirent *dirent, uint32_t count);
    
    // Node operations (called on the vfs_node)
    int (*open)(struct vfs_node *node, struct vfs_file *file, int flags);
    int (*create)(struct vfs_node *parent, const char *name, int mode);
    int (*unlink)(struct vfs_node *parent, const char *name);
    int (*mkdir)(struct vfs_node *parent, const char *name, int mode);
    int (*rmdir)(struct vfs_node *parent, const char *name);
    struct vfs_node *(*lookup)(struct vfs_node *parent, const char *name);
    
    // Stat
    int (*stat)(struct vfs_node *node, struct vfs_stat *stat);
} VfsFileOps;

/* ============================================================================
 * VFS Node (inode equivalent)
 * 
 * Represents a file, directory, or device in the VFS.
 * ============================================================================ */

typedef struct vfs_node {
    char            name[VFS_MAX_NAME];
    uint32_t        type;           // VFS_TYPE_*
    uint32_t        permissions;    // Unix-style permissions
    uint32_t        uid;            // Owner user ID
    uint32_t        gid;            // Owner group ID
    uint64_t        size;           // File size
    uint64_t        inode;          // Inode number
    
    // Timestamps
    uint64_t        atime;          // Last access time
    uint64_t        mtime;          // Last modification time
    uint64_t        ctime;          // Creation/change time
    
    // Device info (for device nodes)
    uint32_t        dev_major;
    uint32_t        dev_minor;
    
    // Operations
    VfsFileOps      *ops;
    
    // Private data for filesystem implementation
    void            *private_data;
    
    // Tree structure
    struct vfs_node *parent;
    struct vfs_node *children;      // First child (for directories)
    struct vfs_node *next;          // Next sibling
    
    // Reference counting
    int             ref_count;
    
    // Mount point (if this node is a mount point)
    struct vfs_mount *mount;
} VfsNode;

/* ============================================================================
 * VFS File (open file descriptor)
 * ============================================================================ */

typedef struct vfs_file {
    VfsNode         *node;          // The file's node
    int             flags;          // Open flags
    uint64_t        offset;         // Current position
    int             ref_count;      // Reference count
    void            *private_data;  // Driver private data
} VfsFile;

/* ============================================================================
 * VFS Directory Entry
 * ============================================================================ */

typedef struct vfs_dirent {
    uint64_t        inode;
    uint32_t        type;
    char            name[VFS_MAX_NAME];
} VfsDirent;

/* ============================================================================
 * VFS Stat Structure
 * ============================================================================ */

typedef struct vfs_stat {
    uint64_t        st_dev;
    uint64_t        st_ino;
    uint32_t        st_mode;
    uint32_t        st_nlink;
    uint32_t        st_uid;
    uint32_t        st_gid;
    uint64_t        st_rdev;
    int64_t         st_size;
    int64_t         st_blksize;
    int64_t         st_blocks;
    uint64_t        st_atime;
    uint64_t        st_mtime;
    uint64_t        st_ctime;
} VfsStat;

/* ============================================================================
 * Filesystem Type
 * 
 * Each filesystem driver registers one of these.
 * ============================================================================ */

typedef struct vfs_filesystem {
    char            name[32];       // e.g., "ext2", "fat32", "devfs"
    
    // Mount a filesystem
    int (*mount)(struct vfs_mount *mount, const char *device, const char *options);
    
    // Unmount a filesystem
    int (*unmount)(struct vfs_mount *mount);
    
    // Get filesystem statistics
    int (*statfs)(struct vfs_mount *mount, void *buf);
    
    // Sync filesystem
    int (*sync)(struct vfs_mount *mount);
} VfsFilesystem;

/* ============================================================================
 * VFS Mount
 * ============================================================================ */

typedef struct vfs_mount {
    VfsFilesystem   *fs;            // Filesystem type
    VfsNode         *root;          // Root node of mounted fs
    VfsNode         *mount_point;   // Where it's mounted
    char            device[VFS_MAX_PATH];  // Device path
    char            path[VFS_MAX_PATH];    // Mount path
    uint32_t        flags;          // Mount flags
    void            *private_data;  // Filesystem private data
} VfsMount;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

// VFS Initialization
void vfs_init(void);

// Filesystem registration
int vfs_register_filesystem(VfsFilesystem *fs);
int vfs_unregister_filesystem(const char *name);

// Mounting
int vfs_mount(const char *source, const char *target, 
              const char *fstype, uint32_t flags, const char *options);
int vfs_unmount(const char *target);

// Path resolution
VfsNode *vfs_lookup(const char *path);
VfsNode *vfs_lookup_parent(const char *path, char *basename);

// File operations (high-level)
int vfs_open(const char *path, int flags, VfsFile **file);
int vfs_close(VfsFile *file);
int64_t vfs_read(VfsFile *file, void *buf, uint64_t count);
int64_t vfs_write(VfsFile *file, const void *buf, uint64_t count);
int64_t vfs_seek(VfsFile *file, int64_t offset, int whence);
int vfs_ioctl(VfsFile *file, uint64_t request, void *arg);

// Directory operations
int vfs_mkdir(const char *path, int mode);
int vfs_rmdir(const char *path);
int vfs_readdir(VfsFile *dir, VfsDirent *dirent, uint32_t count);

// File management
int vfs_create(const char *path, int mode);
int vfs_unlink(const char *path);
int vfs_stat(const char *path, VfsStat *stat);
int vfs_fstat(VfsFile *file, VfsStat *stat);

// Node management
VfsNode *vfs_create_node(const char *name, uint32_t type);
void vfs_destroy_node(VfsNode *node);
int vfs_add_child(VfsNode *parent, VfsNode *child);
int vfs_remove_child(VfsNode *parent, VfsNode *child);

// Reference counting
void vfs_node_ref(VfsNode *node);
void vfs_node_unref(VfsNode *node);
void vfs_file_ref(VfsFile *file);
void vfs_file_unref(VfsFile *file);

// Get root node
VfsNode *vfs_get_root(void);

// Debug
void vfs_debug_tree(VfsNode *node, int depth);

#endif /* _VFS_H */
