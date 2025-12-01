/*
 * NBOS Kernel Module Loader
 * 
 * Loads, initializes, and manages kernel modules (drivers).
 * Modules are ELF relocatable objects that export init/cleanup functions.
 */

#ifndef _MODULE_H
#define _MODULE_H

#include "stdint.h"
#include "elf.h"

/* ============================================================================
 * Module States
 * ============================================================================ */

#define MODULE_STATE_UNLOADED   0
#define MODULE_STATE_LOADING    1
#define MODULE_STATE_LOADED     2
#define MODULE_STATE_RUNNING    3
#define MODULE_STATE_ERROR      4

/* ============================================================================
 * Module Flags
 * ============================================================================ */

#define MODULE_FLAG_BUILTIN     0x01    // Built into kernel
#define MODULE_FLAG_ESSENTIAL   0x02    // Cannot be unloaded
#define MODULE_FLAG_AUTOLOAD    0x04    // Load automatically

/* ============================================================================
 * Maximum Limits
 * ============================================================================ */

#define MAX_MODULES             64
#define MAX_MODULE_NAME         64
#define MAX_MODULE_DEPS         16

/* ============================================================================
 * Module Information Structure
 * 
 * Modules should define this structure and export it as 'module_info'.
 * ============================================================================ */

typedef struct module_info {
    const char  *name;          // Module name
    const char  *description;   // Human-readable description
    const char  *author;        // Author name
    const char  *version;       // Version string
    const char  *license;       // License (e.g., "GPL", "MIT")
    
    // Dependencies
    const char  **depends;      // NULL-terminated array of dependency names
    
    // Callbacks
    int (*init)(void);          // Called when module loads
    void (*cleanup)(void);      // Called when module unloads
} ModuleInfo;

/* ============================================================================
 * Loaded Module Structure
 * ============================================================================ */

typedef struct module {
    char            name[MAX_MODULE_NAME];
    uint32_t        state;
    uint32_t        flags;
    
    // ELF image
    ElfLoadedImage  image;
    
    // Module info (from module itself)
    ModuleInfo      *info;
    
    // Reference counting
    int             ref_count;
    
    // Dependencies
    struct module   *deps[MAX_MODULE_DEPS];
    int             num_deps;
    
    // Users (modules that depend on this one)
    struct module   *users[MAX_MODULE_DEPS];
    int             num_users;
    
    struct module   *next;
} Module;

/* ============================================================================
 * Kernel Symbol Export
 * 
 * Modules can access these exported kernel symbols.
 * ============================================================================ */

typedef struct kernel_symbol {
    const char  *name;
    void        *address;
} KernelSymbol;

// Macro to export a kernel symbol
#define EXPORT_SYMBOL(sym) \
    { #sym, (void *)&sym }

// Macro to define module info
#define MODULE_DEFINE(mod_name, mod_desc, mod_author, mod_version, mod_license) \
    static ModuleInfo __module_info = { \
        .name = mod_name, \
        .description = mod_desc, \
        .author = mod_author, \
        .version = mod_version, \
        .license = mod_license, \
        .depends = NULL, \
        .init = NULL, \
        .cleanup = NULL \
    }; \
    ModuleInfo *module_info __attribute__((used)) = &__module_info;

// Macro to set init function
#define MODULE_INIT(fn) \
    static int (*__module_init)(void) __attribute__((used, section(".init"))) = fn; \
    int module_init(void) { return fn(); }

// Macro to set cleanup function  
#define MODULE_CLEANUP(fn) \
    static void (*__module_cleanup)(void) __attribute__((used, section(".cleanup"))) = fn; \
    void module_cleanup(void) { fn(); }

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

// Module System Initialization
void module_init_system(void);

// Load a module from ELF data
int module_load(const char *name, const void *data, uint64_t size);

// Load a module from file path
int module_load_file(const char *path);

// Unload a module
int module_unload(const char *name);

// Get module by name
Module *module_find(const char *name);

// List all modules
void module_list(void);

// Register kernel symbols for modules to use
void module_register_symbols(KernelSymbol *symbols, int count);

// Find a symbol across all loaded modules
void *module_find_symbol(const char *name);

// Module reference counting
void module_ref(Module *mod);
void module_unref(Module *mod);

// Debug
void module_print_info(Module *mod);

#endif /* _MODULE_H */
