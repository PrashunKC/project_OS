/*
 * NBOS Virtual File System Implementation
 */

#include "vfs.h"
#include "heap.h"
#include "console.h"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void *memset(void *s, int c, uint64_t n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

static void *memcpy(void *dest, const void *src, uint64_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int strncmp(const char *s1, const char *s2, uint64_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    return n ? *(unsigned char *)s1 - *(unsigned char *)s2 : 0;
}

static uint64_t strlen(const char *s) {
    uint64_t len = 0;
    while (*s++) len++;
    return len;
}

static void strncpy_safe(char *dest, const char *src, uint64_t n) {
    uint64_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* ============================================================================
 * Global VFS State
 * ============================================================================ */

static VfsFilesystem *registered_fs[VFS_MAX_FILESYSTEMS];
static int num_filesystems = 0;

static VfsMount *mounts[VFS_MAX_MOUNTS];
static int num_mounts = 0;

static VfsNode *root_node = NULL;

/* Forward declarations */
static int vfs_fstat_node(VfsNode *node, VfsStat *stat);

/* ============================================================================
 * VFS Initialization
 * ============================================================================ */

void vfs_init(void) {
    // Clear state
    memset(registered_fs, 0, sizeof(registered_fs));
    memset(mounts, 0, sizeof(mounts));
    num_filesystems = 0;
    num_mounts = 0;
    
    // Create root node
    root_node = vfs_create_node("/", VFS_TYPE_DIR);
    if (root_node) {
        root_node->permissions = 0755;
    }
    
    console_print("[VFS] Initialized\n", CONSOLE_COLOR_GREEN);
}

/* ============================================================================
 * Filesystem Registration
 * ============================================================================ */

int vfs_register_filesystem(VfsFilesystem *fs) {
    if (num_filesystems >= VFS_MAX_FILESYSTEMS) {
        return -1;
    }
    
    // Check if already registered
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(registered_fs[i]->name, fs->name) == 0) {
            return -2;  // Already registered
        }
    }
    
    registered_fs[num_filesystems++] = fs;
    
    console_print("[VFS] Registered filesystem: ", CONSOLE_COLOR_CYAN);
    console_print(fs->name, CONSOLE_COLOR_WHITE);
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    return 0;
}

int vfs_unregister_filesystem(const char *name) {
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(registered_fs[i]->name, name) == 0) {
            // Shift remaining
            for (int j = i; j < num_filesystems - 1; j++) {
                registered_fs[j] = registered_fs[j + 1];
            }
            num_filesystems--;
            return 0;
        }
    }
    return -1;
}

static VfsFilesystem *find_filesystem(const char *name) {
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(registered_fs[i]->name, name) == 0) {
            return registered_fs[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Node Management
 * ============================================================================ */

VfsNode *vfs_create_node(const char *name, uint32_t type) {
    VfsNode *node = kmalloc(sizeof(VfsNode));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(VfsNode));
    strncpy_safe(node->name, name, VFS_MAX_NAME);
    node->type = type;
    node->ref_count = 1;
    
    return node;
}

void vfs_destroy_node(VfsNode *node) {
    if (node) {
        kfree(node);
    }
}

int vfs_add_child(VfsNode *parent, VfsNode *child) {
    if (!parent || !child) return -1;
    if (parent->type != VFS_TYPE_DIR) return -2;
    
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
    
    return 0;
}

int vfs_remove_child(VfsNode *parent, VfsNode *child) {
    if (!parent || !child) return -1;
    
    VfsNode **pp = &parent->children;
    while (*pp) {
        if (*pp == child) {
            *pp = child->next;
            child->next = NULL;
            child->parent = NULL;
            return 0;
        }
        pp = &(*pp)->next;
    }
    
    return -1;
}

void vfs_node_ref(VfsNode *node) {
    if (node) node->ref_count++;
}

void vfs_node_unref(VfsNode *node) {
    if (node && --node->ref_count <= 0) {
        vfs_destroy_node(node);
    }
}

void vfs_file_ref(VfsFile *file) {
    if (file) file->ref_count++;
}

void vfs_file_unref(VfsFile *file) {
    if (file && --file->ref_count <= 0) {
        if (file->node) {
            vfs_node_unref(file->node);
        }
        kfree(file);
    }
}

VfsNode *vfs_get_root(void) {
    return root_node;
}

/* ============================================================================
 * Path Resolution
 * ============================================================================ */

VfsNode *vfs_lookup(const char *path) {
    if (!path || !*path) return NULL;
    if (!root_node) return NULL;
    
    // Handle absolute paths
    if (path[0] != '/') {
        return NULL;  // Only absolute paths for now
    }
    
    VfsNode *current = root_node;
    const char *p = path + 1;  // Skip leading /
    
    while (*p) {
        // Skip multiple slashes
        while (*p == '/') p++;
        if (!*p) break;
        
        // Find end of component
        const char *end = p;
        while (*end && *end != '/') end++;
        
        uint64_t len = end - p;
        if (len == 0) continue;
        
        // Handle . and ..
        if (len == 1 && p[0] == '.') {
            p = end;
            continue;
        }
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            if (current->parent) {
                current = current->parent;
            }
            p = end;
            continue;
        }
        
        // Check for mount point
        if (current->mount) {
            current = current->mount->root;
        }
        
        // Look up child
        VfsNode *child = NULL;
        
        // Try filesystem lookup first
        if (current->ops && current->ops->lookup) {
            char name[VFS_MAX_NAME];
            if (len < VFS_MAX_NAME) {
                memcpy(name, p, len);
                name[len] = '\0';
                child = current->ops->lookup(current, name);
            }
        }
        
        // Fall back to VFS tree
        if (!child) {
            for (VfsNode *c = current->children; c; c = c->next) {
                if (strlen(c->name) == len && strncmp(c->name, p, len) == 0) {
                    child = c;
                    break;
                }
            }
        }
        
        if (!child) {
            return NULL;
        }
        
        current = child;
        p = end;
    }
    
    return current;
}

VfsNode *vfs_lookup_parent(const char *path, char *basename) {
    if (!path || !*path || !basename) return NULL;
    
    // Find last component
    const char *last_slash = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    // Copy basename
    strncpy_safe(basename, last_slash + 1, VFS_MAX_NAME);
    
    // Handle root
    if (last_slash == path) {
        return root_node;
    }
    
    // Build parent path
    char parent_path[VFS_MAX_PATH];
    uint64_t len = last_slash - path;
    if (len >= VFS_MAX_PATH) len = VFS_MAX_PATH - 1;
    memcpy(parent_path, path, len);
    parent_path[len] = '\0';
    
    return vfs_lookup(parent_path);
}

/* ============================================================================
 * Mounting
 * ============================================================================ */

int vfs_mount(const char *source, const char *target, 
              const char *fstype, uint32_t flags, const char *options) {
    if (num_mounts >= VFS_MAX_MOUNTS) {
        return -1;
    }
    
    VfsFilesystem *fs = find_filesystem(fstype);
    if (!fs) {
        console_print("[VFS] Unknown filesystem: ", CONSOLE_COLOR_RED);
        console_print(fstype, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -2;
    }
    
    // Find mount point
    VfsNode *mount_point = vfs_lookup(target);
    if (!mount_point) {
        console_print("[VFS] Mount point not found: ", CONSOLE_COLOR_RED);
        console_print(target, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -3;
    }
    
    // Create mount structure
    VfsMount *mount = kmalloc(sizeof(VfsMount));
    if (!mount) return -4;
    
    memset(mount, 0, sizeof(VfsMount));
    mount->fs = fs;
    mount->mount_point = mount_point;
    mount->flags = flags;
    if (source) strncpy_safe(mount->device, source, VFS_MAX_PATH);
    strncpy_safe(mount->path, target, VFS_MAX_PATH);
    
    // Call filesystem mount
    int result = fs->mount(mount, source, options);
    if (result != 0) {
        kfree(mount);
        return result;
    }
    
    // Link mount to mount point
    mount_point->mount = mount;
    mounts[num_mounts++] = mount;
    
    console_print("[VFS] Mounted ", CONSOLE_COLOR_GREEN);
    console_print(fstype, CONSOLE_COLOR_WHITE);
    console_print(" on ", CONSOLE_COLOR_GREEN);
    console_print(target, CONSOLE_COLOR_WHITE);
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    return 0;
}

int vfs_unmount(const char *target) {
    for (int i = 0; i < num_mounts; i++) {
        if (strcmp(mounts[i]->path, target) == 0) {
            VfsMount *mount = mounts[i];
            
            // Call filesystem unmount
            if (mount->fs->unmount) {
                mount->fs->unmount(mount);
            }
            
            // Unlink from mount point
            if (mount->mount_point) {
                mount->mount_point->mount = NULL;
            }
            
            // Remove from list
            for (int j = i; j < num_mounts - 1; j++) {
                mounts[j] = mounts[j + 1];
            }
            num_mounts--;
            
            kfree(mount);
            return 0;
        }
    }
    
    return -1;
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

int vfs_open(const char *path, int flags, VfsFile **file) {
    VfsNode *node = vfs_lookup(path);
    
    // Handle O_CREAT
    if (!node && (flags & VFS_O_CREAT)) {
        char basename[VFS_MAX_NAME];
        VfsNode *parent = vfs_lookup_parent(path, basename);
        
        if (parent && parent->ops && parent->ops->create) {
            int result = parent->ops->create(parent, basename, 0644);
            if (result == 0) {
                node = vfs_lookup(path);
            }
        }
    }
    
    if (!node) {
        return -1;
    }
    
    // Create file structure
    VfsFile *f = kmalloc(sizeof(VfsFile));
    if (!f) return -2;
    
    memset(f, 0, sizeof(VfsFile));
    f->node = node;
    f->flags = flags;
    f->ref_count = 1;
    
    vfs_node_ref(node);
    
    // Call open handler
    if (node->ops && node->ops->open) {
        int result = node->ops->open(node, f, flags);
        if (result != 0) {
            vfs_node_unref(node);
            kfree(f);
            return result;
        }
    }
    
    // Handle truncate
    if (flags & VFS_O_TRUNC) {
        node->size = 0;
    }
    
    // Handle append
    if (flags & VFS_O_APPEND) {
        f->offset = node->size;
    }
    
    *file = f;
    return 0;
}

int vfs_close(VfsFile *file) {
    if (!file) return -1;
    
    int result = 0;
    if (file->node && file->node->ops && file->node->ops->close) {
        result = file->node->ops->close(file);
    }
    
    vfs_file_unref(file);
    return result;
}

int64_t vfs_read(VfsFile *file, void *buf, uint64_t count) {
    if (!file || !buf) return -1;
    if (!file->node) return -2;
    
    if (!file->node->ops || !file->node->ops->read) {
        return -3;  // Not readable
    }
    
    return file->node->ops->read(file, buf, count);
}

int64_t vfs_write(VfsFile *file, const void *buf, uint64_t count) {
    if (!file || !buf) return -1;
    if (!file->node) return -2;
    
    if (!file->node->ops || !file->node->ops->write) {
        return -3;  // Not writable
    }
    
    return file->node->ops->write(file, buf, count);
}

int64_t vfs_seek(VfsFile *file, int64_t offset, int whence) {
    if (!file) return -1;
    
    int64_t new_offset;
    
    switch (whence) {
        case VFS_SEEK_SET:
            new_offset = offset;
            break;
        case VFS_SEEK_CUR:
            new_offset = file->offset + offset;
            break;
        case VFS_SEEK_END:
            if (file->node) {
                new_offset = file->node->size + offset;
            } else {
                return -2;
            }
            break;
        default:
            return -3;
    }
    
    if (new_offset < 0) {
        return -4;
    }
    
    // Call driver seek if available
    if (file->node && file->node->ops && file->node->ops->seek) {
        int64_t result = file->node->ops->seek(file, offset, whence);
        if (result < 0) return result;
        new_offset = result;
    }
    
    file->offset = new_offset;
    return new_offset;
}

int vfs_ioctl(VfsFile *file, uint64_t request, void *arg) {
    if (!file || !file->node) return -1;
    
    if (!file->node->ops || !file->node->ops->ioctl) {
        return -2;  // ioctl not supported
    }
    
    return file->node->ops->ioctl(file, request, arg);
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

int vfs_mkdir(const char *path, int mode) {
    char basename[VFS_MAX_NAME];
    VfsNode *parent = vfs_lookup_parent(path, basename);
    
    if (!parent) return -1;
    
    if (parent->ops && parent->ops->mkdir) {
        return parent->ops->mkdir(parent, basename, mode);
    }
    
    // Fall back to VFS-level mkdir
    VfsNode *dir = vfs_create_node(basename, VFS_TYPE_DIR);
    if (!dir) return -2;
    
    dir->permissions = mode;
    vfs_add_child(parent, dir);
    
    return 0;
}

int vfs_rmdir(const char *path) {
    VfsNode *node = vfs_lookup(path);
    if (!node) return -1;
    if (node->type != VFS_TYPE_DIR) return -2;
    if (node->children) return -3;  // Not empty
    
    if (node->ops && node->ops->rmdir) {
        // TODO: get parent and basename
    }
    
    if (node->parent) {
        vfs_remove_child(node->parent, node);
    }
    vfs_node_unref(node);
    
    return 0;
}

int vfs_readdir(VfsFile *dir, VfsDirent *dirent, uint32_t count) {
    if (!dir || !dirent) return -1;
    if (!dir->node || dir->node->type != VFS_TYPE_DIR) return -2;
    
    if (dir->node->ops && dir->node->ops->readdir) {
        return dir->node->ops->readdir(dir, dirent, count);
    }
    
    // Fall back to VFS tree traversal
    VfsNode *child = dir->node->children;
    uint32_t index = 0;
    uint32_t offset = (uint32_t)dir->offset;
    uint32_t filled = 0;
    
    while (child && filled < count) {
        if (index >= offset) {
            dirent[filled].inode = child->inode;
            dirent[filled].type = child->type;
            strncpy_safe(dirent[filled].name, child->name, VFS_MAX_NAME);
            filled++;
        }
        child = child->next;
        index++;
    }
    
    dir->offset = index;
    return filled;
}

/* ============================================================================
 * File Management
 * ============================================================================ */

int vfs_create(const char *path, int mode) {
    char basename[VFS_MAX_NAME];
    VfsNode *parent = vfs_lookup_parent(path, basename);
    
    if (!parent) return -1;
    
    if (parent->ops && parent->ops->create) {
        return parent->ops->create(parent, basename, mode);
    }
    
    // Fall back to VFS-level create
    VfsNode *file = vfs_create_node(basename, VFS_TYPE_FILE);
    if (!file) return -2;
    
    file->permissions = mode;
    vfs_add_child(parent, file);
    
    return 0;
}

int vfs_unlink(const char *path) {
    VfsNode *node = vfs_lookup(path);
    if (!node) return -1;
    if (node->type == VFS_TYPE_DIR) return -2;
    
    if (node->parent && node->parent->ops && node->parent->ops->unlink) {
        return node->parent->ops->unlink(node->parent, node->name);
    }
    
    if (node->parent) {
        vfs_remove_child(node->parent, node);
    }
    vfs_node_unref(node);
    
    return 0;
}

int vfs_stat(const char *path, VfsStat *stat) {
    VfsNode *node = vfs_lookup(path);
    if (!node) return -1;
    
    return vfs_fstat_node(node, stat);
}

static int vfs_fstat_node(VfsNode *node, VfsStat *stat) {
    if (!node || !stat) return -1;
    
    if (node->ops && node->ops->stat) {
        return node->ops->stat(node, stat);
    }
    
    // Fill from node
    memset(stat, 0, sizeof(VfsStat));
    stat->st_ino = node->inode;
    stat->st_mode = node->permissions | (node->type << 12);
    stat->st_nlink = 1;
    stat->st_uid = node->uid;
    stat->st_gid = node->gid;
    stat->st_size = node->size;
    stat->st_atime = node->atime;
    stat->st_mtime = node->mtime;
    stat->st_ctime = node->ctime;
    stat->st_blksize = 4096;
    stat->st_blocks = (node->size + 511) / 512;
    
    if (node->type == VFS_TYPE_CHARDEV || node->type == VFS_TYPE_BLOCKDEV) {
        stat->st_rdev = (node->dev_major << 8) | node->dev_minor;
    }
    
    return 0;
}

int vfs_fstat(VfsFile *file, VfsStat *stat) {
    if (!file || !file->node) return -1;
    return vfs_fstat_node(file->node, stat);
}

/* ============================================================================
 * Debug
 * ============================================================================ */

void vfs_debug_tree(VfsNode *node, int depth) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        console_print("  ", CONSOLE_COLOR_WHITE);
    }
    
    // Print node info
    if (node->type == VFS_TYPE_DIR) {
        console_print("[D] ", CONSOLE_COLOR_CYAN);
    } else if (node->type == VFS_TYPE_CHARDEV) {
        console_print("[C] ", CONSOLE_COLOR_YELLOW);
    } else if (node->type == VFS_TYPE_BLOCKDEV) {
        console_print("[B] ", CONSOLE_COLOR_YELLOW);
    } else {
        console_print("[F] ", CONSOLE_COLOR_WHITE);
    }
    
    console_print(node->name, CONSOLE_COLOR_WHITE);
    
    if (node->mount) {
        console_print(" (mount: ", CONSOLE_COLOR_GREEN);
        console_print(node->mount->fs->name, CONSOLE_COLOR_GREEN);
        console_print(")", CONSOLE_COLOR_GREEN);
    }
    
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    // Recurse into children
    for (VfsNode *child = node->children; child; child = child->next) {
        vfs_debug_tree(child, depth + 1);
    }
}
