/*
 * NBOS Device Manager Implementation
 */

#include "device.h"
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

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static void strncpy_safe(char *dest, const char *src, uint64_t n) {
    uint64_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

/* ============================================================================
 * Global State
 * ============================================================================ */

static Device *device_list = NULL;
static Driver *driver_list = NULL;
static int device_count = 0;
static int driver_count = 0;

// DevFS state
static VfsNode *devfs_root = NULL;
static VfsFilesystem devfs_type;

/* ============================================================================
 * DevFS File Operations
 * ============================================================================ */

static int64_t devfs_read(VfsFile *file, void *buf, uint64_t count) {
    if (!file || !file->node || !file->node->private_data) {
        return -1;
    }
    
    Device *dev = (Device *)file->node->private_data;
    
    if (!dev->ops || !dev->ops->read) {
        return -2;
    }
    
    int64_t result = dev->ops->read(dev, buf, count, file->offset);
    if (result > 0) {
        file->offset += result;
        dev->read_bytes += result;
        dev->read_ops++;
    }
    
    return result;
}

static int64_t devfs_write(VfsFile *file, const void *buf, uint64_t count) {
    if (!file || !file->node || !file->node->private_data) {
        return -1;
    }
    
    Device *dev = (Device *)file->node->private_data;
    
    if (!dev->ops || !dev->ops->write) {
        return -2;
    }
    
    int64_t result = dev->ops->write(dev, buf, count, file->offset);
    if (result > 0) {
        file->offset += result;
        dev->write_bytes += result;
        dev->write_ops++;
    }
    
    return result;
}

static int devfs_ioctl_handler(VfsFile *file, uint64_t request, void *arg) {
    if (!file || !file->node || !file->node->private_data) {
        return -1;
    }
    
    Device *dev = (Device *)file->node->private_data;
    
    if (!dev->ops || !dev->ops->ioctl) {
        return -2;
    }
    
    return dev->ops->ioctl(dev, request, arg);
}

static int devfs_open_handler(VfsNode *node, VfsFile *file, int flags) {
    if (!node || !node->private_data) {
        return -1;
    }
    
    Device *dev = (Device *)node->private_data;
    
    if (dev->ops && dev->ops->open) {
        return dev->ops->open(dev, flags);
    }
    
    return 0;
}

static int devfs_close_handler(VfsFile *file) {
    if (!file || !file->node || !file->node->private_data) {
        return -1;
    }
    
    Device *dev = (Device *)file->node->private_data;
    
    if (dev->ops && dev->ops->close) {
        return dev->ops->close(dev);
    }
    
    return 0;
}

static VfsFileOps devfs_file_ops = {
    .read = devfs_read,
    .write = devfs_write,
    .ioctl = devfs_ioctl_handler,
    .open = devfs_open_handler,
    .close = devfs_close_handler,
    .seek = NULL,
    .readdir = NULL,
    .create = NULL,
    .unlink = NULL,
    .mkdir = NULL,
    .rmdir = NULL,
    .lookup = NULL,
    .stat = NULL,
    .mmap = NULL,
};

/* ============================================================================
 * DevFS Filesystem
 * ============================================================================ */

static int devfs_mount(VfsMount *mount, const char *device, const char *options) {
    (void)device;
    (void)options;
    
    if (!devfs_root) {
        devfs_root = vfs_create_node("dev", VFS_TYPE_DIR);
        if (!devfs_root) {
            return -1;
        }
        devfs_root->permissions = 0755;
    }
    
    mount->root = devfs_root;
    mount->private_data = NULL;
    
    return 0;
}

static int devfs_unmount(VfsMount *mount) {
    (void)mount;
    // Don't actually destroy devfs_root
    return 0;
}

/* ============================================================================
 * DevFS Initialization
 * ============================================================================ */

int devfs_init(void) {
    // Initialize devfs filesystem type
    strncpy_safe(devfs_type.name, "devfs", sizeof(devfs_type.name));
    devfs_type.mount = devfs_mount;
    devfs_type.unmount = devfs_unmount;
    devfs_type.statfs = NULL;
    devfs_type.sync = NULL;
    
    // Register filesystem
    vfs_register_filesystem(&devfs_type);
    
    // Create /dev directory in VFS root
    VfsNode *root = vfs_get_root();
    if (root) {
        VfsNode *dev_dir = vfs_create_node("dev", VFS_TYPE_DIR);
        if (dev_dir) {
            dev_dir->permissions = 0755;
            vfs_add_child(root, dev_dir);
            
            // Mount devfs on /dev
            vfs_mount(NULL, "/dev", "devfs", 0, NULL);
        }
    }
    
    return 0;
}

/* ============================================================================
 * Device Manager Initialization
 * ============================================================================ */

void device_init(void) {
    device_list = NULL;
    driver_list = NULL;
    device_count = 0;
    driver_count = 0;
    
    // Initialize devfs
    devfs_init();
    
    console_print("[Device] Device manager initialized\n", CONSOLE_COLOR_GREEN);
}

/* ============================================================================
 * Device Management
 * ============================================================================ */

Device *device_create(const char *name, uint32_t type, uint32_t major, uint32_t minor) {
    Device *dev = kmalloc(sizeof(Device));
    if (!dev) return NULL;
    
    memset(dev, 0, sizeof(Device));
    strncpy_safe(dev->name, name, MAX_DEV_NAME);
    dev->type = type;
    dev->major = major;
    dev->minor = minor;
    
    return dev;
}

int device_register(Device *dev) {
    if (!dev) return -1;
    if (device_count >= MAX_DEVICES) return -2;
    
    // Check for duplicate
    if (device_find_by_name(dev->name)) {
        return -3;
    }
    
    // Add to list
    dev->next = device_list;
    device_list = dev;
    device_count++;
    
    // Create device node in /dev
    devfs_create_node(dev);
    
    // Try to find a driver
    for (Driver *drv = driver_list; drv; drv = drv->next) {
        if (drv->type == dev->type || drv->type == DEV_TYPE_UNKNOWN) {
            if (drv->probe && drv->probe(dev) == 0) {
                if (drv->attach && drv->attach(dev) == 0) {
                    dev->driver = drv;
                    if (!dev->ops) {
                        dev->ops = drv->ops;
                    }
                    break;
                }
            }
        }
    }
    
    console_print("[Device] Registered: ", CONSOLE_COLOR_GREEN);
    console_print(dev->name, CONSOLE_COLOR_WHITE);
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    return 0;
}

int device_unregister(Device *dev) {
    if (!dev) return -1;
    
    // Remove from list
    Device **pp = &device_list;
    while (*pp) {
        if (*pp == dev) {
            *pp = dev->next;
            device_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    // Remove device node
    devfs_remove_node(dev);
    
    // Detach driver
    if (dev->driver && dev->driver->detach) {
        dev->driver->detach(dev);
    }
    
    return 0;
}

void device_destroy(Device *dev) {
    if (dev) {
        device_unregister(dev);
        kfree(dev);
    }
}

/* ============================================================================
 * Device Lookup
 * ============================================================================ */

Device *device_find_by_name(const char *name) {
    for (Device *dev = device_list; dev; dev = dev->next) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
    }
    return NULL;
}

Device *device_find_by_number(uint32_t major, uint32_t minor) {
    for (Device *dev = device_list; dev; dev = dev->next) {
        if (dev->major == major && dev->minor == minor) {
            return dev;
        }
    }
    return NULL;
}

Device *device_get_first(void) {
    return device_list;
}

/* ============================================================================
 * Driver Management
 * ============================================================================ */

int driver_register(Driver *drv) {
    if (!drv) return -1;
    if (driver_count >= MAX_DRIVERS) return -2;
    
    // Check for duplicate
    if (driver_find_by_name(drv->name)) {
        return -3;
    }
    
    // Add to list
    drv->next = driver_list;
    driver_list = drv;
    driver_count++;
    
    console_print("[Driver] Registered: ", CONSOLE_COLOR_CYAN);
    console_print(drv->name, CONSOLE_COLOR_WHITE);
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    // Try to attach to existing devices
    for (Device *dev = device_list; dev; dev = dev->next) {
        if (!dev->driver && (drv->type == dev->type || drv->type == DEV_TYPE_UNKNOWN)) {
            if (drv->probe && drv->probe(dev) == 0) {
                if (drv->attach && drv->attach(dev) == 0) {
                    dev->driver = drv;
                    if (!dev->ops) {
                        dev->ops = drv->ops;
                    }
                }
            }
        }
    }
    
    return 0;
}

int driver_unregister(Driver *drv) {
    if (!drv) return -1;
    
    // Detach from all devices
    for (Device *dev = device_list; dev; dev = dev->next) {
        if (dev->driver == drv) {
            if (drv->detach) {
                drv->detach(dev);
            }
            dev->driver = NULL;
        }
    }
    
    // Remove from list
    Driver **pp = &driver_list;
    while (*pp) {
        if (*pp == drv) {
            *pp = drv->next;
            driver_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    return 0;
}

Driver *driver_find_by_name(const char *name) {
    for (Driver *drv = driver_list; drv; drv = drv->next) {
        if (strcmp(drv->name, name) == 0) {
            return drv;
        }
    }
    return NULL;
}

/* ============================================================================
 * Device Operations
 * ============================================================================ */

int device_open(Device *dev, int flags) {
    if (!dev) return -1;
    if (!dev->ops || !dev->ops->open) return 0;
    return dev->ops->open(dev, flags);
}

int device_close(Device *dev) {
    if (!dev) return -1;
    if (!dev->ops || !dev->ops->close) return 0;
    return dev->ops->close(dev);
}

int64_t device_read(Device *dev, void *buf, uint64_t count, uint64_t offset) {
    if (!dev) return -1;
    if (!dev->ops || !dev->ops->read) return -2;
    
    int64_t result = dev->ops->read(dev, buf, count, offset);
    if (result > 0) {
        dev->read_bytes += result;
        dev->read_ops++;
    }
    return result;
}

int64_t device_write(Device *dev, const void *buf, uint64_t count, uint64_t offset) {
    if (!dev) return -1;
    if (!dev->ops || !dev->ops->write) return -2;
    
    int64_t result = dev->ops->write(dev, buf, count, offset);
    if (result > 0) {
        dev->write_bytes += result;
        dev->write_ops++;
    }
    return result;
}

int device_ioctl(Device *dev, uint64_t request, void *arg) {
    if (!dev) return -1;
    if (!dev->ops || !dev->ops->ioctl) return -2;
    return dev->ops->ioctl(dev, request, arg);
}

/* ============================================================================
 * Block Device Operations
 * ============================================================================ */

int device_read_blocks(Device *dev, uint64_t start, uint32_t count, void *buf) {
    if (!dev || !buf) return -1;
    if (dev->type != DEV_TYPE_BLOCK) return -2;
    if (!dev->ops || !dev->ops->read_block) return -3;
    
    uint8_t *p = buf;
    uint32_t block_size = 512;
    
    if (dev->ops->get_block_size) {
        block_size = dev->ops->get_block_size(dev);
    }
    
    for (uint32_t i = 0; i < count; i++) {
        int result = dev->ops->read_block(dev, start + i, p);
        if (result != 0) return result;
        p += block_size;
    }
    
    return 0;
}

int device_write_blocks(Device *dev, uint64_t start, uint32_t count, const void *buf) {
    if (!dev || !buf) return -1;
    if (dev->type != DEV_TYPE_BLOCK) return -2;
    if (!dev->ops || !dev->ops->write_block) return -3;
    if (dev->flags & DEV_FLAG_READONLY) return -4;
    
    const uint8_t *p = buf;
    uint32_t block_size = 512;
    
    if (dev->ops->get_block_size) {
        block_size = dev->ops->get_block_size(dev);
    }
    
    for (uint32_t i = 0; i < count; i++) {
        int result = dev->ops->write_block(dev, start + i, p);
        if (result != 0) return result;
        p += block_size;
    }
    
    return 0;
}

/* ============================================================================
 * Device Tree
 * ============================================================================ */

int device_add_child(Device *parent, Device *child) {
    if (!parent || !child) return -1;
    
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    
    return 0;
}

int device_remove_child(Device *parent, Device *child) {
    if (!parent || !child) return -1;
    
    Device **pp = &parent->children;
    while (*pp) {
        if (*pp == child) {
            *pp = child->next_sibling;
            child->next_sibling = NULL;
            child->parent = NULL;
            return 0;
        }
        pp = &(*pp)->next_sibling;
    }
    
    return -1;
}

/* ============================================================================
 * DevFS Node Management
 * ============================================================================ */

int devfs_create_node(Device *dev) {
    if (!dev || !devfs_root) return -1;
    
    // Determine VFS type
    uint32_t vfs_type;
    if (dev->type == DEV_TYPE_CHAR) {
        vfs_type = VFS_TYPE_CHARDEV;
    } else if (dev->type == DEV_TYPE_BLOCK) {
        vfs_type = VFS_TYPE_BLOCKDEV;
    } else {
        vfs_type = VFS_TYPE_CHARDEV;  // Default to char
    }
    
    // Create node
    VfsNode *node = vfs_create_node(dev->name, vfs_type);
    if (!node) return -2;
    
    node->dev_major = dev->major;
    node->dev_minor = dev->minor;
    node->permissions = 0666;
    node->ops = &devfs_file_ops;
    node->private_data = dev;
    
    // Add to /dev
    vfs_add_child(devfs_root, node);
    
    dev->vfs_node = node;
    
    return 0;
}

int devfs_remove_node(Device *dev) {
    if (!dev || !dev->vfs_node) return -1;
    
    VfsNode *node = dev->vfs_node;
    
    if (devfs_root) {
        vfs_remove_child(devfs_root, node);
    }
    
    vfs_node_unref(node);
    dev->vfs_node = NULL;
    
    return 0;
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

static void print_number(uint64_t n) {
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    
    if (n == 0) {
        buf[--i] = '0';
    } else {
        while (n > 0 && i > 0) {
            buf[--i] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    console_print(&buf[i], CONSOLE_COLOR_WHITE);
}

void device_list_all(void) {
    console_print("=== Registered Devices ===\n", CONSOLE_COLOR_CYAN);
    
    for (Device *dev = device_list; dev; dev = dev->next) {
        device_print_info(dev);
    }
    
    console_print("Total: ", CONSOLE_COLOR_GRAY);
    print_number(device_count);
    console_print(" devices\n", CONSOLE_COLOR_GRAY);
}

void device_print_info(Device *dev) {
    if (!dev) return;
    
    console_print("  ", CONSOLE_COLOR_WHITE);
    console_print(dev->name, CONSOLE_COLOR_WHITE);
    console_print(" (", CONSOLE_COLOR_GRAY);
    print_number(dev->major);
    console_print(",", CONSOLE_COLOR_GRAY);
    print_number(dev->minor);
    console_print(") ", CONSOLE_COLOR_GRAY);
    
    // Type
    switch (dev->type) {
        case DEV_TYPE_CHAR:
            console_print("[char]", CONSOLE_COLOR_YELLOW);
            break;
        case DEV_TYPE_BLOCK:
            console_print("[block]", CONSOLE_COLOR_GREEN);
            break;
        case DEV_TYPE_NET:
            console_print("[net]", CONSOLE_COLOR_CYAN);
            break;
        case DEV_TYPE_INPUT:
            console_print("[input]", CONSOLE_COLOR_LIGHT_GREEN);
            break;
        default:
            console_print("[unknown]", CONSOLE_COLOR_GRAY);
    }
    
    // Driver
    if (dev->driver) {
        console_print(" driver=", CONSOLE_COLOR_GRAY);
        console_print(dev->driver->name, CONSOLE_COLOR_WHITE);
    }
    
    console_print("\n", CONSOLE_COLOR_WHITE);
}
