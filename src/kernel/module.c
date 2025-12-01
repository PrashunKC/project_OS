/*
 * NBOS Kernel Module Loader Implementation
 */

#include "module.h"
#include "elf.h"
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

/* ============================================================================
 * Global State
 * ============================================================================ */

static Module *loaded_modules = NULL;
static int module_count = 0;

// Kernel exported symbols
static KernelSymbol *kernel_symbols = NULL;
static int num_kernel_symbols = 0;

/* ============================================================================
 * Forward Declarations for Kernel Exports
 * ============================================================================ */

// These are the functions we export to modules
// Forward declare them here so we can build the symbol table

extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);
extern void *krealloc(void *ptr, uint64_t size);
extern void *kcalloc(uint64_t count, uint64_t size);

extern void console_print(const char *str, uint32_t color);
extern void console_putchar(char c, uint32_t color);
extern void console_clear(uint32_t bg_color);
extern void console_newline(void);

extern int device_register(void *dev);
extern int device_unregister(void *dev);
extern void *device_create(const char *name, uint32_t type, uint32_t major, uint32_t minor);
extern void device_destroy(void *dev);
extern int driver_register(void *drv);
extern int driver_unregister(void *drv);

extern void *vfs_lookup(const char *path);
extern int vfs_register_filesystem(void *fs);

/* ============================================================================
 * Built-in Kernel Symbol Table
 * ============================================================================ */

static KernelSymbol builtin_symbols[] = {
    // Memory
    { "kmalloc", (void *)kmalloc },
    { "kfree", (void *)kfree },
    { "krealloc", (void *)krealloc },
    { "kcalloc", (void *)kcalloc },
    
    // Console
    { "console_print", (void *)console_print },
    { "console_putchar", (void *)console_putchar },
    { "console_clear", (void *)console_clear },
    { "console_newline", (void *)console_newline },
    
    // Device
    { "device_register", (void *)device_register },
    { "device_unregister", (void *)device_unregister },
    { "device_create", (void *)device_create },
    { "device_destroy", (void *)device_destroy },
    { "driver_register", (void *)driver_register },
    { "driver_unregister", (void *)driver_unregister },
    
    // VFS
    { "vfs_lookup", (void *)vfs_lookup },
    { "vfs_register_filesystem", (void *)vfs_register_filesystem },
    
    // Module
    { "module_find", (void *)module_find },
    { "module_find_symbol", (void *)module_find_symbol },
    
    { NULL, NULL }
};

/* ============================================================================
 * Kernel Symbol Lookup (for ELF loader)
 * ============================================================================ */

typedef struct {
    const char *name;
    void *address;
} ElfKernelSymbol;

static void *kernel_symbol_lookup(const char *name) {
    // Search built-in symbols
    for (int i = 0; builtin_symbols[i].name; i++) {
        if (strcmp(builtin_symbols[i].name, name) == 0) {
            return builtin_symbols[i].address;
        }
    }
    
    // Search registered symbols
    for (int i = 0; i < num_kernel_symbols; i++) {
        if (strcmp(kernel_symbols[i].name, name) == 0) {
            return kernel_symbols[i].address;
        }
    }
    
    // Search loaded modules
    for (Module *mod = loaded_modules; mod; mod = mod->next) {
        if (mod->state == MODULE_STATE_RUNNING) {
            void *addr = elf_find_symbol(&mod->image, name);
            if (addr) return addr;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Module System Initialization
 * ============================================================================ */

void module_init_system(void) {
    loaded_modules = NULL;
    module_count = 0;
    
    // Set up ELF loader to use our symbol lookup
    // Convert builtin_symbols to the format ELF loader expects
    int count = 0;
    while (builtin_symbols[count].name) count++;
    
    // Register with ELF loader
    // Note: elf_register_kernel_symbols expects a different struct type
    // For now we use our own lookup
    
    console_print("[Module] Module system initialized\n", CONSOLE_COLOR_GREEN);
    console_print("[Module] ", CONSOLE_COLOR_GRAY);
    print_number(count);
    console_print(" kernel symbols exported\n", CONSOLE_COLOR_GRAY);
}

/* ============================================================================
 * Symbol Management
 * ============================================================================ */

void module_register_symbols(KernelSymbol *symbols, int count) {
    kernel_symbols = symbols;
    num_kernel_symbols = count;
}

void *module_find_symbol(const char *name) {
    return kernel_symbol_lookup(name);
}

/* ============================================================================
 * Module Loading
 * ============================================================================ */

int module_load(const char *name, const void *data, uint64_t size) {
    // Check if already loaded
    if (module_find(name)) {
        console_print("[Module] Already loaded: ", CONSOLE_COLOR_YELLOW);
        console_print(name, CONSOLE_COLOR_YELLOW);
        console_print("\n", CONSOLE_COLOR_YELLOW);
        return -1;
    }
    
    // Check limit
    if (module_count >= MAX_MODULES) {
        console_print("[Module] Too many modules loaded\n", CONSOLE_COLOR_RED);
        return -2;
    }
    
    // Validate ELF
    int result = elf_validate(data, size);
    if (result != ELF_OK) {
        console_print("[Module] Invalid ELF: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -3;
    }
    
    // Create module structure
    Module *mod = kmalloc(sizeof(Module));
    if (!mod) return -4;
    
    memset(mod, 0, sizeof(Module));
    strncpy_safe(mod->name, name, MAX_MODULE_NAME);
    mod->state = MODULE_STATE_LOADING;
    mod->ref_count = 1;
    
    // Load ELF
    result = elf_load_module(data, size, &mod->image);
    if (result != ELF_OK) {
        console_print("[Module] Failed to load: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        kfree(mod);
        return -5;
    }
    
    // Find module_info symbol
    mod->info = elf_find_symbol(&mod->image, "module_info");
    
    // Add to list
    mod->next = loaded_modules;
    loaded_modules = mod;
    module_count++;
    
    // Call init function
    int (*init_func)(void) = mod->image.init_func;
    if (!init_func && mod->info && mod->info->init) {
        init_func = mod->info->init;
    }
    
    if (init_func) {
        console_print("[Module] Initializing: ", CONSOLE_COLOR_CYAN);
        console_print(name, CONSOLE_COLOR_WHITE);
        console_print("\n", CONSOLE_COLOR_WHITE);
        
        result = init_func();
        if (result != 0) {
            console_print("[Module] Init failed: ", CONSOLE_COLOR_RED);
            console_print(name, CONSOLE_COLOR_RED);
            console_print("\n", CONSOLE_COLOR_RED);
            
            mod->state = MODULE_STATE_ERROR;
            return -6;
        }
    }
    
    mod->state = MODULE_STATE_RUNNING;
    
    console_print("[Module] Loaded: ", CONSOLE_COLOR_GREEN);
    console_print(name, CONSOLE_COLOR_WHITE);
    if (mod->info && mod->info->version) {
        console_print(" v", CONSOLE_COLOR_GRAY);
        console_print(mod->info->version, CONSOLE_COLOR_GRAY);
    }
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    return 0;
}

int module_load_file(const char *path) {
    // TODO: Implement when we have filesystem support
    // For now, modules must be loaded from memory
    (void)path;
    console_print("[Module] File loading not implemented yet\n", CONSOLE_COLOR_YELLOW);
    return -1;
}

/* ============================================================================
 * Module Unloading
 * ============================================================================ */

int module_unload(const char *name) {
    Module *mod = module_find(name);
    if (!mod) {
        console_print("[Module] Not found: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -1;
    }
    
    // Check if essential
    if (mod->flags & MODULE_FLAG_ESSENTIAL) {
        console_print("[Module] Cannot unload essential module: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -2;
    }
    
    // Check reference count
    if (mod->ref_count > 1) {
        console_print("[Module] Module in use: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -3;
    }
    
    // Check if other modules depend on this one
    if (mod->num_users > 0) {
        console_print("[Module] Other modules depend on: ", CONSOLE_COLOR_RED);
        console_print(name, CONSOLE_COLOR_RED);
        console_print("\n", CONSOLE_COLOR_RED);
        return -4;
    }
    
    // Call cleanup function
    void (*cleanup_func)(void) = mod->image.cleanup_func;
    if (!cleanup_func && mod->info && mod->info->cleanup) {
        cleanup_func = mod->info->cleanup;
    }
    
    if (cleanup_func) {
        console_print("[Module] Cleaning up: ", CONSOLE_COLOR_CYAN);
        console_print(name, CONSOLE_COLOR_WHITE);
        console_print("\n", CONSOLE_COLOR_WHITE);
        cleanup_func();
    }
    
    // Remove from dependency lists
    for (int i = 0; i < mod->num_deps; i++) {
        Module *dep = mod->deps[i];
        if (dep) {
            // Remove us from dep's users list
            for (int j = 0; j < dep->num_users; j++) {
                if (dep->users[j] == mod) {
                    // Shift remaining
                    for (int k = j; k < dep->num_users - 1; k++) {
                        dep->users[k] = dep->users[k + 1];
                    }
                    dep->num_users--;
                    break;
                }
            }
            module_unref(dep);
        }
    }
    
    // Remove from list
    Module **pp = &loaded_modules;
    while (*pp) {
        if (*pp == mod) {
            *pp = mod->next;
            module_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    // Unload ELF image
    elf_unload(&mod->image);
    
    console_print("[Module] Unloaded: ", CONSOLE_COLOR_GREEN);
    console_print(name, CONSOLE_COLOR_WHITE);
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    kfree(mod);
    
    return 0;
}

/* ============================================================================
 * Module Lookup
 * ============================================================================ */

Module *module_find(const char *name) {
    for (Module *mod = loaded_modules; mod; mod = mod->next) {
        if (strcmp(mod->name, name) == 0) {
            return mod;
        }
    }
    return NULL;
}

/* ============================================================================
 * Reference Counting
 * ============================================================================ */

void module_ref(Module *mod) {
    if (mod) mod->ref_count++;
}

void module_unref(Module *mod) {
    if (mod && mod->ref_count > 0) {
        mod->ref_count--;
    }
}

/* ============================================================================
 * Debug/Info Functions
 * ============================================================================ */

void module_list(void) {
    console_print("=== Loaded Modules ===\n", CONSOLE_COLOR_CYAN);
    
    for (Module *mod = loaded_modules; mod; mod = mod->next) {
        module_print_info(mod);
    }
    
    console_print("Total: ", CONSOLE_COLOR_GRAY);
    print_number(module_count);
    console_print(" modules\n", CONSOLE_COLOR_GRAY);
}

void module_print_info(Module *mod) {
    if (!mod) return;
    
    console_print("  ", CONSOLE_COLOR_WHITE);
    console_print(mod->name, CONSOLE_COLOR_WHITE);
    
    // State
    console_print(" [", CONSOLE_COLOR_GRAY);
    switch (mod->state) {
        case MODULE_STATE_LOADING:
            console_print("loading", CONSOLE_COLOR_YELLOW);
            break;
        case MODULE_STATE_LOADED:
            console_print("loaded", CONSOLE_COLOR_CYAN);
            break;
        case MODULE_STATE_RUNNING:
            console_print("running", CONSOLE_COLOR_GREEN);
            break;
        case MODULE_STATE_ERROR:
            console_print("error", CONSOLE_COLOR_RED);
            break;
        default:
            console_print("unknown", CONSOLE_COLOR_GRAY);
    }
    console_print("]", CONSOLE_COLOR_GRAY);
    
    // Size
    console_print(" ", CONSOLE_COLOR_WHITE);
    print_number(mod->image.size);
    console_print(" bytes", CONSOLE_COLOR_GRAY);
    
    // Version
    if (mod->info && mod->info->version) {
        console_print(" v", CONSOLE_COLOR_GRAY);
        console_print(mod->info->version, CONSOLE_COLOR_GRAY);
    }
    
    // Ref count
    if (mod->ref_count > 1) {
        console_print(" (refs: ", CONSOLE_COLOR_GRAY);
        print_number(mod->ref_count);
        console_print(")", CONSOLE_COLOR_GRAY);
    }
    
    console_print("\n", CONSOLE_COLOR_WHITE);
    
    // Description
    if (mod->info && mod->info->description) {
        console_print("    ", CONSOLE_COLOR_WHITE);
        console_print(mod->info->description, CONSOLE_COLOR_GRAY);
        console_print("\n", CONSOLE_COLOR_WHITE);
    }
}
