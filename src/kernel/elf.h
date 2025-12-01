/*
 * ELF64 Loader for NBOS
 * 
 * Supports loading and executing ELF64 executables and
 * relocatable objects (for kernel modules/drivers).
 */

#ifndef _ELF_H
#define _ELF_H

#include "stdint.h"

/* ============================================================================
 * ELF Magic Numbers and Identifiers
 * ============================================================================ */

#define ELF_MAGIC       0x464C457F  // "\x7FELF" in little-endian

// ELF Class (32-bit vs 64-bit)
#define ELFCLASS32      1
#define ELFCLASS64      2

// ELF Data Encoding
#define ELFDATA2LSB     1   // Little-endian
#define ELFDATA2MSB     2   // Big-endian

// ELF OS/ABI
#define ELFOSABI_NONE   0   // UNIX System V
#define ELFOSABI_LINUX  3   // Linux

// ELF Type
#define ET_NONE         0   // No file type
#define ET_REL          1   // Relocatable file (object file)
#define ET_EXEC         2   // Executable file
#define ET_DYN          3   // Shared object file
#define ET_CORE         4   // Core file

// ELF Machine Type
#define EM_386          3   // Intel 80386
#define EM_X86_64       62  // AMD x86-64

/* ============================================================================
 * Program Header Types
 * ============================================================================ */

#define PT_NULL         0   // Unused entry
#define PT_LOAD         1   // Loadable segment
#define PT_DYNAMIC      2   // Dynamic linking info
#define PT_INTERP       3   // Interpreter path
#define PT_NOTE         4   // Auxiliary info
#define PT_SHLIB        5   // Reserved
#define PT_PHDR         6   // Program header table
#define PT_TLS          7   // Thread-local storage

// Program Header Flags
#define PF_X            0x1 // Executable
#define PF_W            0x2 // Writable
#define PF_R            0x4 // Readable

/* ============================================================================
 * Section Header Types
 * ============================================================================ */

#define SHT_NULL        0   // Inactive section
#define SHT_PROGBITS    1   // Program data
#define SHT_SYMTAB      2   // Symbol table
#define SHT_STRTAB      3   // String table
#define SHT_RELA        4   // Relocation with addends
#define SHT_HASH        5   // Symbol hash table
#define SHT_DYNAMIC     6   // Dynamic linking info
#define SHT_NOTE        7   // Notes
#define SHT_NOBITS      8   // BSS (uninitialized data)
#define SHT_REL         9   // Relocation without addends
#define SHT_SHLIB       10  // Reserved
#define SHT_DYNSYM      11  // Dynamic symbol table

// Section Header Flags
#define SHF_WRITE       0x1     // Writable
#define SHF_ALLOC       0x2     // Occupies memory
#define SHF_EXECINSTR   0x4     // Executable

/* ============================================================================
 * Symbol Table
 * ============================================================================ */

// Symbol Binding (upper 4 bits of st_info)
#define STB_LOCAL       0   // Local symbol
#define STB_GLOBAL      1   // Global symbol
#define STB_WEAK        2   // Weak symbol

// Symbol Type (lower 4 bits of st_info)
#define STT_NOTYPE      0   // No type
#define STT_OBJECT      1   // Data object
#define STT_FUNC        2   // Function
#define STT_SECTION     3   // Section
#define STT_FILE        4   // Source file name

// Macros for symbol info
#define ELF64_ST_BIND(i)    ((i) >> 4)
#define ELF64_ST_TYPE(i)    ((i) & 0xf)
#define ELF64_ST_INFO(b,t)  (((b) << 4) + ((t) & 0xf))

/* ============================================================================
 * Relocation Types (x86_64)
 * ============================================================================ */

#define R_X86_64_NONE       0   // No relocation
#define R_X86_64_64         1   // Direct 64-bit
#define R_X86_64_PC32       2   // PC relative 32-bit signed
#define R_X86_64_GOT32      3   // 32-bit GOT entry
#define R_X86_64_PLT32      4   // 32-bit PLT address
#define R_X86_64_COPY       5   // Copy symbol at runtime
#define R_X86_64_GLOB_DAT   6   // Create GOT entry
#define R_X86_64_JUMP_SLOT  7   // Create PLT entry
#define R_X86_64_RELATIVE   8   // Adjust by program base
#define R_X86_64_32         10  // Direct 32-bit zero-extended
#define R_X86_64_32S        11  // Direct 32-bit sign-extended

// Macros for relocation info
#define ELF64_R_SYM(i)      ((i) >> 32)
#define ELF64_R_TYPE(i)     ((i) & 0xffffffffL)
#define ELF64_R_INFO(s,t)   (((uint64_t)(s) << 32) + (uint64_t)(t))

/* ============================================================================
 * ELF64 Header Structure
 * ============================================================================ */

typedef struct {
    uint8_t     e_ident[16];    // ELF identification
    uint16_t    e_type;         // Object file type
    uint16_t    e_machine;      // Machine type
    uint32_t    e_version;      // Object file version
    uint64_t    e_entry;        // Entry point address
    uint64_t    e_phoff;        // Program header offset
    uint64_t    e_shoff;        // Section header offset
    uint32_t    e_flags;        // Processor-specific flags
    uint16_t    e_ehsize;       // ELF header size
    uint16_t    e_phentsize;    // Program header entry size
    uint16_t    e_phnum;        // Number of program headers
    uint16_t    e_shentsize;    // Section header entry size
    uint16_t    e_shnum;        // Number of section headers
    uint16_t    e_shstrndx;     // Section name string table index
} __attribute__((packed)) Elf64_Ehdr;

/* ============================================================================
 * ELF64 Program Header Structure
 * ============================================================================ */

typedef struct {
    uint32_t    p_type;         // Segment type
    uint32_t    p_flags;        // Segment flags
    uint64_t    p_offset;       // Offset in file
    uint64_t    p_vaddr;        // Virtual address in memory
    uint64_t    p_paddr;        // Physical address (if relevant)
    uint64_t    p_filesz;       // Size of segment in file
    uint64_t    p_memsz;        // Size of segment in memory
    uint64_t    p_align;        // Alignment
} __attribute__((packed)) Elf64_Phdr;

/* ============================================================================
 * ELF64 Section Header Structure
 * ============================================================================ */

typedef struct {
    uint32_t    sh_name;        // Section name (string table index)
    uint32_t    sh_type;        // Section type
    uint64_t    sh_flags;       // Section flags
    uint64_t    sh_addr;        // Virtual address in memory
    uint64_t    sh_offset;      // Offset in file
    uint64_t    sh_size;        // Size of section
    uint32_t    sh_link;        // Link to another section
    uint32_t    sh_info;        // Additional info
    uint64_t    sh_addralign;   // Address alignment
    uint64_t    sh_entsize;     // Entry size if section holds table
} __attribute__((packed)) Elf64_Shdr;

/* ============================================================================
 * ELF64 Symbol Table Entry
 * ============================================================================ */

typedef struct {
    uint32_t    st_name;        // Symbol name (string table index)
    uint8_t     st_info;        // Symbol type and binding
    uint8_t     st_other;       // Symbol visibility
    uint16_t    st_shndx;       // Section index
    uint64_t    st_value;       // Symbol value
    uint64_t    st_size;        // Symbol size
} __attribute__((packed)) Elf64_Sym;

/* ============================================================================
 * ELF64 Relocation Entries
 * ============================================================================ */

typedef struct {
    uint64_t    r_offset;       // Address to relocate
    uint64_t    r_info;         // Symbol and type
} __attribute__((packed)) Elf64_Rel;

typedef struct {
    uint64_t    r_offset;       // Address to relocate
    uint64_t    r_info;         // Symbol and type
    int64_t     r_addend;       // Addend
} __attribute__((packed)) Elf64_Rela;

/* ============================================================================
 * ELF64 Dynamic Section Entry
 * ============================================================================ */

typedef struct {
    int64_t     d_tag;          // Dynamic entry type
    union {
        uint64_t d_val;         // Integer value
        uint64_t d_ptr;         // Address value
    } d_un;
} __attribute__((packed)) Elf64_Dyn;

/* ============================================================================
 * Loaded Program/Module Information
 * ============================================================================ */

typedef struct {
    void        *base;          // Base address where loaded
    uint64_t    entry;          // Entry point
    uint64_t    size;           // Total size in memory
    
    // For modules
    void        *init_func;     // Module init function
    void        *cleanup_func;  // Module cleanup function
    char        name[64];       // Module name
    
    // Symbol table (for dynamic linking)
    Elf64_Sym   *symtab;        // Symbol table
    char        *strtab;        // String table
    uint32_t    num_symbols;    // Number of symbols
} ElfLoadedImage;

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define ELF_OK              0
#define ELF_ERR_INVALID     -1  // Invalid ELF file
#define ELF_ERR_UNSUPPORTED -2  // Unsupported ELF type
#define ELF_ERR_NOMEM       -3  // Out of memory
#define ELF_ERR_NOENTRY     -4  // No entry point
#define ELF_ERR_RELOC       -5  // Relocation failed
#define ELF_ERR_SYMBOL      -6  // Symbol not found

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

// Validate ELF header
int elf_validate(const void *data, uint64_t size);

// Load an executable ELF into memory
int elf_load_executable(const void *data, uint64_t size, ElfLoadedImage *image);

// Load a relocatable object (kernel module)
int elf_load_module(const void *data, uint64_t size, ElfLoadedImage *image);

// Unload a loaded ELF image
void elf_unload(ElfLoadedImage *image);

// Find a symbol by name in a loaded image
void *elf_find_symbol(ElfLoadedImage *image, const char *name);

// Execute a loaded executable
int elf_execute(ElfLoadedImage *image, int argc, char **argv);

// Get ELF header from data
static inline Elf64_Ehdr *elf_get_header(const void *data) {
    return (Elf64_Ehdr *)data;
}

// Get program header at index
static inline Elf64_Phdr *elf_get_phdr(const void *data, int index) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    return (Elf64_Phdr *)((uint8_t *)data + ehdr->e_phoff + 
                          index * ehdr->e_phentsize);
}

// Get section header at index
static inline Elf64_Shdr *elf_get_shdr(const void *data, int index) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    return (Elf64_Shdr *)((uint8_t *)data + ehdr->e_shoff + 
                          index * ehdr->e_shentsize);
}

#endif /* _ELF_H */
