/*
 * NBOS Device Manager
 * 
 * Manages device registration, discovery, and access.
 * Provides /dev filesystem for device nodes.
 */

#ifndef _DEVICE_H
#define _DEVICE_H

#include "stdint.h"
#include "vfs.h"

/* ============================================================================
 * Device Types
 * ============================================================================ */

#define DEV_TYPE_UNKNOWN    0
#define DEV_TYPE_CHAR       1   // Character device
#define DEV_TYPE_BLOCK      2   // Block device
#define DEV_TYPE_NET        3   // Network device
#define DEV_TYPE_INPUT      4   // Input device (keyboard, mouse)
#define DEV_TYPE_DISPLAY    5   // Display device
#define DEV_TYPE_SOUND      6   // Sound device
#define DEV_TYPE_STORAGE    7   // Storage controller
#define DEV_TYPE_USB        8   // USB device
#define DEV_TYPE_PCI        9   // PCI device

/* ============================================================================
 * Device Classes (major numbers)
 * ============================================================================ */

#define DEV_MAJOR_NULL      1   // /dev/null, /dev/zero
#define DEV_MAJOR_TTY       4   // TTY devices
#define DEV_MAJOR_CONSOLE   5   // Console
#define DEV_MAJOR_MEM       6   // /dev/mem, /dev/kmem
#define DEV_MAJOR_RANDOM    7   // /dev/random, /dev/urandom
#define DEV_MAJOR_DISK      8   // SCSI/SATA disks
#define DEV_MAJOR_FLOPPY    2   // Floppy disks
#define DEV_MAJOR_FB        29  // Framebuffer
#define DEV_MAJOR_INPUT     13  // Input devices

/* ============================================================================
 * Device Flags
 * ============================================================================ */

#define DEV_FLAG_REMOVABLE  0x01
#define DEV_FLAG_READONLY   0x02
#define DEV_FLAG_HOTPLUG    0x04
#define DEV_FLAG_VIRTUAL    0x08

/* ============================================================================
 * Maximum Limits
 * ============================================================================ */

#define MAX_DEVICES         128
#define MAX_DRIVERS         64
#define MAX_DEV_NAME        32

/* ============================================================================
 * Device Structure
 * ============================================================================ */

struct device;
struct driver;

typedef struct device_ops {
    // Basic operations
    int (*open)(struct device *dev, int flags);
    int (*close)(struct device *dev);
    int64_t (*read)(struct device *dev, void *buf, uint64_t count, uint64_t offset);
    int64_t (*write)(struct device *dev, const void *buf, uint64_t count, uint64_t offset);
    int (*ioctl)(struct device *dev, uint64_t request, void *arg);
    
    // Power management
    int (*suspend)(struct device *dev);
    int (*resume)(struct device *dev);
    
    // For block devices
    int (*read_block)(struct device *dev, uint64_t block, void *buf);
    int (*write_block)(struct device *dev, uint64_t block, const void *buf);
    uint64_t (*get_block_count)(struct device *dev);
    uint32_t (*get_block_size)(struct device *dev);
} DeviceOps;

typedef struct device {
    char            name[MAX_DEV_NAME];     // Device name (e.g., "sda", "tty0")
    uint32_t        type;                   // DEV_TYPE_*
    uint32_t        major;                  // Major number
    uint32_t        minor;                  // Minor number
    uint32_t        flags;                  // DEV_FLAG_*
    
    DeviceOps       *ops;                   // Device operations
    struct driver   *driver;                // Associated driver
    
    void            *private_data;          // Device-specific data
    
    // VFS integration
    VfsNode         *vfs_node;              // Device node in /dev
    
    // Device tree
    struct device   *parent;                // Parent device
    struct device   *children;              // First child
    struct device   *next_sibling;          // Next sibling
    struct device   *next;                  // Next in global list
    
    // Statistics
    uint64_t        read_bytes;
    uint64_t        write_bytes;
    uint64_t        read_ops;
    uint64_t        write_ops;
} Device;

/* ============================================================================
 * Driver Structure
 * ============================================================================ */

typedef struct driver {
    char            name[MAX_DEV_NAME];     // Driver name
    uint32_t        type;                   // Device type this driver handles
    
    // Driver operations
    int (*probe)(Device *dev);              // Check if driver supports device
    int (*attach)(Device *dev);             // Attach driver to device
    int (*detach)(Device *dev);             // Detach driver from device
    
    DeviceOps       *ops;                   // Default operations for devices
    
    void            *private_data;          // Driver-specific data
    
    struct driver   *next;                  // Next in driver list
} Driver;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

// Device Manager Initialization
void device_init(void);

// Device Registration
Device *device_create(const char *name, uint32_t type, uint32_t major, uint32_t minor);
int device_register(Device *dev);
int device_unregister(Device *dev);
void device_destroy(Device *dev);

// Device Lookup
Device *device_find_by_name(const char *name);
Device *device_find_by_number(uint32_t major, uint32_t minor);
Device *device_get_first(void);

// Driver Registration
int driver_register(Driver *drv);
int driver_unregister(Driver *drv);
Driver *driver_find_by_name(const char *name);

// Device Operations (high-level)
int device_open(Device *dev, int flags);
int device_close(Device *dev);
int64_t device_read(Device *dev, void *buf, uint64_t count, uint64_t offset);
int64_t device_write(Device *dev, const void *buf, uint64_t count, uint64_t offset);
int device_ioctl(Device *dev, uint64_t request, void *arg);

// Block Device Operations
int device_read_blocks(Device *dev, uint64_t start, uint32_t count, void *buf);
int device_write_blocks(Device *dev, uint64_t start, uint32_t count, const void *buf);

// Device Tree
int device_add_child(Device *parent, Device *child);
int device_remove_child(Device *parent, Device *child);

// DevFS (device filesystem)
int devfs_init(void);
int devfs_create_node(Device *dev);
int devfs_remove_node(Device *dev);

// Debug
void device_list_all(void);
void device_print_info(Device *dev);

#endif /* _DEVICE_H */
