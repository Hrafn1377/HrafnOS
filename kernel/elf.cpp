#include "elf.hpp"
#include "pmm.hpp"
#include "vmm.hpp"
#include "serial.hpp"

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed));

#define PT_LOAD 1

uint64_t* elf_load(uint8_t* elf, uint64_t* entry_out) {
    Elf64_Ehdr* eh = (Elf64_Ehdr*)elf;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        kprint("ELF: bad magic\n");
        return nullptr;
    }
    if (eh->e_ident[4] != 2) {                 // EI_CLASS != ELFCLASS64
        kprint("ELF: not 64-bit\n");
        return nullptr;
    }

    uint64_t* space  = vmm_create_address_space();
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        Elf64_Phdr* p = &phdr[i];
        if (p->p_type != PT_LOAD) continue;

        uint64_t va_start = p->p_vaddr & ~0xFFFULL;
        uint64_t va_end   = (p->p_vaddr + p->p_memsz + 0xFFF) & ~0xFFFULL;

        for (uint64_t va = va_start; va < va_end; va += 0x1000) {
            uint8_t* frame = (uint8_t*)pmm_alloc_frame();   // identity-accessible
            for (int b = 0; b < 4096; b++) frame[b] = 0;    // zero (covers .bss)

            // Copy whatever file bytes fall on this page.
            for (uint64_t off = 0; off < 4096; off++) {
                uint64_t vaddr = va + off;
                if (vaddr >= p->p_vaddr && vaddr < p->p_vaddr + p->p_filesz) {
                    frame[off] = elf[p->p_offset + (vaddr - p->p_vaddr)];
                }
            }

            vmm_map_page_in(space, va, (uint64_t)frame,
                            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
    }

    *entry_out = eh->e_entry;
    return space;
}
