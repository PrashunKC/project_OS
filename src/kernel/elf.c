/*
 * ELF64 Loader Implementation for NBOS
 */

#include "elf.h"
#include "heap.h"
#include "console.h"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static void *memcpy(void *dest, const void *src, uint64_t n) {
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static void *memset(void *s, int c, uint64_t n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static int strncpy(char *dest, const char *src, uint64_t n) {
    uint64_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

/* ============================================================================
 * ELF Validation
 * ============================================================================ */

int elf_validate(const void *data, uint64_t size) {
    if (size < sizeof(Elf64_Ehdr)) {
        return ELF_ERR_INVALID;
    }
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    
    // Check magic number
    if (*(uint32_t *)ehdr->e_ident != ELF_MAGIC) {
        return ELF_ERR_INVALID;
    }
    
    // Check 64-bit
    if (ehdr->e_ident[4] != ELFCLASS64) {
        return ELF_ERR_UNSUPPORTED;
    }
    
    // Check little-endian
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        return ELF_ERR_UNSUPPORTED;
    }
    
    // Check x86_64
    if (ehdr->e_machine != EM_X86_64) {
        return ELF_ERR_UNSUPPORTED;
    }
    
    return ELF_OK;
}

/* ============================================================================
 * Load Executable ELF
 * ============================================================================ */

int elf_load_executable(const void *data, uint64_t size, ElfLoadedImage *image) {
    int result = elf_validate(data, size);
    if (result != ELF_OK) {
        return result;
    }
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    
    // Must be executable or shared object
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return ELF_ERR_UNSUPPORTED;
    }
    
    // Calculate total memory needed
    uint64_t min_addr = UINT64_MAX;
    uint64_t max_addr = 0;
    
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = elf_get_phdr(data, i);
        
        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_vaddr < min_addr) {
                min_addr = phdr->p_vaddr;
            }
            if (phdr->p_vaddr + phdr->p_memsz > max_addr) {
                max_addr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
    }
    
    if (min_addr == UINT64_MAX) {
        return ELF_ERR_INVALID;
    }
    
    uint64_t total_size = max_addr - min_addr;
    
    // Allocate memory for the program
    // For PIE executables, we load at our chosen address
    // For non-PIE, we need to use the specified addresses
    void *base;
    
    if (ehdr->e_type == ET_DYN) {
        // Position-independent - we can load anywhere
        base = kmalloc(total_size);
        if (!base) {
            return ELF_ERR_NOMEM;
        }
        memset(base, 0, total_size);
    } else {
        // Fixed address executable - use specified addresses
        // For now, we'll still allocate and copy, but adjust entry point
        base = kmalloc(total_size);
        if (!base) {
            return ELF_ERR_NOMEM;
        }
        memset(base, 0, total_size);
    }
    
    // Load segments
    for (int i = 0; i < ehdr->e_phnum; i++) {
        Elf64_Phdr *phdr = elf_get_phdr(data, i);
        
        if (phdr->p_type == PT_LOAD) {
            uint64_t offset_in_image = phdr->p_vaddr - min_addr;
            void *segment_dest = (uint8_t *)base + offset_in_image;
            const void *segment_src = (const uint8_t *)data + phdr->p_offset;
            
            // Copy file contents
            if (phdr->p_filesz > 0) {
                memcpy(segment_dest, segment_src, phdr->p_filesz);
            }
            
            // Zero out BSS (memsz > filesz)
            if (phdr->p_memsz > phdr->p_filesz) {
                memset((uint8_t *)segment_dest + phdr->p_filesz, 0,
                       phdr->p_memsz - phdr->p_filesz);
            }
        }
    }
    
    // Fill in image info
    image->base = base;
    image->size = total_size;
    
    // Adjust entry point for PIE
    if (ehdr->e_type == ET_DYN) {
        image->entry = (uint64_t)base + (ehdr->e_entry - min_addr);
    } else {
        image->entry = (uint64_t)base + (ehdr->e_entry - min_addr);
    }
    
    image->init_func = NULL;
    image->cleanup_func = NULL;
    image->symtab = NULL;
    image->strtab = NULL;
    image->num_symbols = 0;
    strncpy(image->name, "executable", sizeof(image->name));
    
    // Try to load symbol table for debugging
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        
        if (shdr->sh_type == SHT_SYMTAB) {
            uint32_t num_syms = shdr->sh_size / sizeof(Elf64_Sym);
            Elf64_Sym *symtab = kmalloc(shdr->sh_size);
            if (symtab) {
                memcpy(symtab, (uint8_t *)data + shdr->sh_offset, shdr->sh_size);
                image->symtab = symtab;
                image->num_symbols = num_syms;
                
                // Get string table
                Elf64_Shdr *strhdr = elf_get_shdr(data, shdr->sh_link);
                char *strtab = kmalloc(strhdr->sh_size);
                if (strtab) {
                    memcpy(strtab, (uint8_t *)data + strhdr->sh_offset, strhdr->sh_size);
                    image->strtab = strtab;
                }
            }
            break;
        }
    }
    
    return ELF_OK;
}

/* ============================================================================
 * Load Relocatable Module
 * ============================================================================ */

// Kernel symbols that modules can link against
typedef struct {
    const char *name;
    void *address;
} KernelSymbol;

// Kernel symbol table (populated by kernel)
static KernelSymbol *kernel_symbols = NULL;
static uint32_t num_kernel_symbols = 0;

void elf_register_kernel_symbols(KernelSymbol *symbols, uint32_t count) {
    kernel_symbols = symbols;
    num_kernel_symbols = count;
}

static void *find_kernel_symbol(const char *name) {
    for (uint32_t i = 0; i < num_kernel_symbols; i++) {
        if (strcmp(kernel_symbols[i].name, name) == 0) {
            return kernel_symbols[i].address;
        }
    }
    return NULL;
}

int elf_load_module(const void *data, uint64_t size, ElfLoadedImage *image) {
    int result = elf_validate(data, size);
    if (result != ELF_OK) {
        return result;
    }
    
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    
    // Must be relocatable
    if (ehdr->e_type != ET_REL) {
        return ELF_ERR_UNSUPPORTED;
    }
    
    // Calculate total size needed
    uint64_t total_size = 0;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        if (shdr->sh_flags & SHF_ALLOC) {
            // Align
            total_size = (total_size + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1);
            total_size += shdr->sh_size;
        }
    }
    
    // Allocate memory
    void *base = kmalloc(total_size);
    if (!base) {
        return ELF_ERR_NOMEM;
    }
    memset(base, 0, total_size);
    
    // Array to track section addresses
    uint64_t *section_addrs = kmalloc(ehdr->e_shnum * sizeof(uint64_t));
    if (!section_addrs) {
        kfree(base);
        return ELF_ERR_NOMEM;
    }
    memset(section_addrs, 0, ehdr->e_shnum * sizeof(uint64_t));
    
    // Load sections
    uint64_t current_addr = (uint64_t)base;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        
        if (shdr->sh_flags & SHF_ALLOC) {
            // Align
            current_addr = (current_addr + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1);
            section_addrs[i] = current_addr;
            
            if (shdr->sh_type != SHT_NOBITS) {
                // Copy data
                memcpy((void *)current_addr, (uint8_t *)data + shdr->sh_offset, shdr->sh_size);
            }
            
            current_addr += shdr->sh_size;
        }
    }
    
    // Find symbol and string tables
    Elf64_Sym *symtab = NULL;
    char *strtab = NULL;
    uint32_t num_syms = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        
        if (shdr->sh_type == SHT_SYMTAB) {
            symtab = (Elf64_Sym *)((uint8_t *)data + shdr->sh_offset);
            num_syms = shdr->sh_size / sizeof(Elf64_Sym);
            
            Elf64_Shdr *strhdr = elf_get_shdr(data, shdr->sh_link);
            strtab = (char *)((uint8_t *)data + strhdr->sh_offset);
            break;
        }
    }
    
    if (!symtab || !strtab) {
        kfree(section_addrs);
        kfree(base);
        return ELF_ERR_INVALID;
    }
    
    // Process relocations
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        
        if (shdr->sh_type == SHT_RELA) {
            Elf64_Shdr *target_shdr = elf_get_shdr(data, shdr->sh_info);
            if (!(target_shdr->sh_flags & SHF_ALLOC)) {
                continue;  // Skip non-allocated sections
            }
            
            uint64_t target_base = section_addrs[shdr->sh_info];
            Elf64_Rela *rela = (Elf64_Rela *)((uint8_t *)data + shdr->sh_offset);
            uint32_t num_relas = shdr->sh_size / sizeof(Elf64_Rela);
            
            for (uint32_t j = 0; j < num_relas; j++) {
                uint32_t sym_idx = ELF64_R_SYM(rela[j].r_info);
                uint32_t rel_type = ELF64_R_TYPE(rela[j].r_info);
                
                Elf64_Sym *sym = &symtab[sym_idx];
                const char *sym_name = strtab + sym->st_name;
                
                // Resolve symbol
                uint64_t sym_val;
                if (sym->st_shndx == 0) {
                    // Undefined - look in kernel
                    void *addr = find_kernel_symbol(sym_name);
                    if (!addr) {
                        console_print("[ELF] Undefined symbol: ", CONSOLE_COLOR_RED);
                        console_print(sym_name, CONSOLE_COLOR_RED);
                        console_print("\n", CONSOLE_COLOR_RED);
                        kfree(section_addrs);
                        kfree(base);
                        return ELF_ERR_SYMBOL;
                    }
                    sym_val = (uint64_t)addr;
                } else {
                    sym_val = section_addrs[sym->st_shndx] + sym->st_value;
                }
                
                // Apply relocation
                uint64_t *target = (uint64_t *)(target_base + rela[j].r_offset);
                
                switch (rel_type) {
                    case R_X86_64_64:
                        *target = sym_val + rela[j].r_addend;
                        break;
                    case R_X86_64_PC32:
                    case R_X86_64_PLT32:
                        *(int32_t *)target = (int32_t)(sym_val + rela[j].r_addend - (uint64_t)target);
                        break;
                    case R_X86_64_32:
                        *(uint32_t *)target = (uint32_t)(sym_val + rela[j].r_addend);
                        break;
                    case R_X86_64_32S:
                        *(int32_t *)target = (int32_t)(sym_val + rela[j].r_addend);
                        break;
                    default:
                        console_print("[ELF] Unknown relocation type\n", CONSOLE_COLOR_YELLOW);
                        break;
                }
            }
        }
    }
    
    // Find module init/cleanup functions
    image->init_func = NULL;
    image->cleanup_func = NULL;
    
    for (uint32_t i = 0; i < num_syms; i++) {
        const char *name = strtab + symtab[i].st_name;
        
        if (strcmp(name, "module_init") == 0 || strcmp(name, "init_module") == 0) {
            image->init_func = (void *)(section_addrs[symtab[i].st_shndx] + symtab[i].st_value);
        } else if (strcmp(name, "module_cleanup") == 0 || strcmp(name, "cleanup_module") == 0) {
            image->cleanup_func = (void *)(section_addrs[symtab[i].st_shndx] + symtab[i].st_value);
        }
    }
    
    // Fill in image info
    image->base = base;
    image->size = total_size;
    image->entry = 0;  // Modules don't have an entry point in the traditional sense
    strncpy(image->name, "module", sizeof(image->name));
    
    // Copy symbol table for later use
    image->symtab = kmalloc(num_syms * sizeof(Elf64_Sym));
    if (image->symtab) {
        memcpy(image->symtab, symtab, num_syms * sizeof(Elf64_Sym));
        image->num_symbols = num_syms;
        
        // We need to adjust symbol values to runtime addresses
        for (uint32_t i = 0; i < num_syms; i++) {
            if (image->symtab[i].st_shndx != 0 && 
                image->symtab[i].st_shndx < ehdr->e_shnum) {
                image->symtab[i].st_value += section_addrs[image->symtab[i].st_shndx];
            }
        }
    }
    
    // Copy string table
    Elf64_Shdr *strtab_shdr = NULL;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *shdr = elf_get_shdr(data, i);
        if (shdr->sh_type == SHT_STRTAB && i != ehdr->e_shstrndx) {
            strtab_shdr = shdr;
            break;
        }
    }
    if (strtab_shdr) {
        image->strtab = kmalloc(strtab_shdr->sh_size);
        if (image->strtab) {
            memcpy(image->strtab, strtab, strtab_shdr->sh_size);
        }
    }
    
    kfree(section_addrs);
    
    return ELF_OK;
}

/* ============================================================================
 * Unload ELF Image
 * ============================================================================ */

void elf_unload(ElfLoadedImage *image) {
    if (image->symtab) {
        kfree(image->symtab);
    }
    if (image->strtab) {
        kfree(image->strtab);
    }
    if (image->base) {
        kfree(image->base);
    }
    
    memset(image, 0, sizeof(ElfLoadedImage));
}

/* ============================================================================
 * Find Symbol
 * ============================================================================ */

void *elf_find_symbol(ElfLoadedImage *image, const char *name) {
    if (!image->symtab || !image->strtab) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < image->num_symbols; i++) {
        const char *sym_name = image->strtab + image->symtab[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            return (void *)image->symtab[i].st_value;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Execute Loaded Executable
 * ============================================================================ */

int elf_execute(ElfLoadedImage *image, int argc, char **argv) {
    if (!image->entry) {
        return ELF_ERR_NOENTRY;
    }
    
    // Cast entry point to function pointer
    // For executables expecting Linux ABI: main(argc, argv, envp)
    typedef int (*MainFunc)(int, char **, char **);
    MainFunc entry = (MainFunc)image->entry;
    
    console_print("[ELF] Executing at 0x", CONSOLE_COLOR_CYAN);
    
    // Print address in hex
    char hex[] = "0123456789ABCDEF";
    uint64_t addr = image->entry;
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[addr & 0xF];
        addr >>= 4;
    }
    buf[16] = '\0';
    console_print(buf, CONSOLE_COLOR_CYAN);
    console_print("\n", CONSOLE_COLOR_CYAN);
    
    // Execute
    char *envp[] = { NULL };
    int result = entry(argc, argv, envp);
    
    return result;
}
