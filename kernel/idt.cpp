#include "idt.hpp"
#include "serial.hpp"
#include "pic.hpp"
#include "sched.hpp"
#include "syscall.hpp"
#include "io.hpp"
#include "kbd.hpp"
#include "vfs.hpp"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));


static idt_entry idt[256];
static idt_ptr   idtr;

extern "C" void* isr_stub_table[];
extern "C" void  syscall_stub();

static const char* exception_names[32] = {
    "#DE Divide Error", "#DB Debug", "NMI", "#BP Breakpoint",
    "#OF Overflow", "#BR Bound Range", "#UD Invalid Opcode",
    "#NM Device Not Available", "#DF Double Fault",
    "Coprocessor Segment Overrun", "#TS Invalid TSS",
    "#NP Segment Not Present", "#SS Stack-Segment Fault",
    "#GP General Protection Fault", "#PF Page Fault", "Reserved",
    "#MF x87 FPU Error", "#AC Alignment Check", "#MC Machine Check",
    "#XM SIMD FP Exception", "#VE Virtualization", "#CP Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "#HV Hypervisor", "#VC VMM Comm", "#SX Security", "Reserved"
};

static void idt_set_gate_ex(int n, void* handler, uint8_t type_attr) {
    uint64_t addr      = (uint64_t)handler;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

static void idt_set_gate(int n, void* handler) {
    idt_set_gate_ex(n, handler, 0x8E);
}

void idt_init() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    for (int i = 0; i < 49; i++) idt_set_gate(i, isr_stub_table[i]);
    idt_set_gate_ex(0x80, (void*)syscall_stub, 0xEE);   // DPL3 syscall gate
    asm volatile("lidt %0" : : "m"(idtr));
}

static void dump_reg(const char* name, uint64_t value) {
    kprint(name);
    kprint("=");
    kprint_ptr((void*)value);
}

extern "C" uint64_t isr_handler(registers* regs) {
    if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        if (irq == 0) {
            pic_send_eoi(0);
            sched_tick();
            return schedule((uint64_t)regs);
        }
        if (irq == 1) {                       // keyboard
            uint8_t sc = inb(0x60);
            char c = kbd_handle_scancode(sc);
            if (c >= ' ' || c == '\n' || c == '\b' || c == '\t')  // printable + newline + bksp + tab 
                kbd_buffer_push(c);
            pic_send_eoi(1);
            return (uint64_t)regs;
        }
    }
    if (regs->int_no == 48) {                   // software yield (int $0x30)
        return schedule((uint64_t)regs);
    }

    if (regs->int_no == 0x80) {                 // system call (int $0x80)
        switch (regs->rax) {
            case SYS_WRITE: {
                int         fd  = (int)regs->rdi;
                const char* buf = (const char*)regs->rsi;
                uint64_t    len = regs->rdx;
                if (fd == 1 || fd == 2) {              // stdout / stderr -> console
                    for (uint64_t i = 0; i < len; i++) kprint_char(buf[i]);
                    regs->rax = len;
                } else {                                // a real file
                    regs->rax = (uint64_t)vfs_write(fd, buf, (uint32_t)len);
                }
                break;
            }
            case SYS_READ: {
                int      fd  = (int)regs->rdi;
                char*    buf = (char*)regs->rsi;
                uint64_t len = regs->rdx;
                if (fd == 0) {                          // stdin -> keyboard/serial
                    uint64_t n = 0;
                    while (n < len) {
                        int c = kbd_getchar();             // keyboard first
                        if (c < 0) c = serial_getchar();   // then serial fallback
                        if (c < 0) break;
                        if (c == 0x7F) c = '\b';          // DEL -> BS (serial terminals)
                        buf[n++] = (char)c;
                    }
                    regs->rax = n;
                } else {                                // a real file
                    regs->rax = (uint64_t)vfs_read(fd, buf, (uint32_t)len);
                }
                break;
            }
            case SYS_OPEN:
                regs->rax = (uint64_t)vfs_open((const char*)regs->rdi, (int)regs->rsi);
                break;
            case SYS_CLOSE:
                regs->rax = (uint64_t)vfs_close((int)regs->rdi);
                break;
            case SYS_READDIR:
                regs->rax = (uint64_t)vfs_readdir((int)regs->rdi, (int)regs->rsi, (char*)regs->rdx);
                break;
            case SYS_MKDIR:
                regs->rax = (uint64_t)vfs_mkdir((const char*)regs->rdi);
                break;
            case SYS_UNLINK:
                regs->rax = (uint64_t)vfs_unlink((const char*)regs->rdi);
                break;
            case SYS_SYNC:
                regs->rax = (uint64_t)vfs_sync();
                break;
            case SYS_EXEC: {
                uint64_t new_rsp = exec_current((char**)regs->rdi, (int)regs->rsi);
                if (new_rsp) return new_rsp;   // resume in the new program
                regs->rax = (uint64_t)-1;       // load failed; tell the caller
                break;
            }
            case SYS_FORK:
                regs->rax = (uint64_t)fork_current(regs);        // child id to parent
                break;                        // (child gets 0 in its own frame)
            case SYS_WAIT:
                return wait_current((uint64_t)regs);
            case SYS_YIELD:
                return schedule((uint64_t)regs);
            case SYS_EXIT:
                sched_kill_current();
                return schedule((uint64_t)regs);
            default:
                regs->rax = (uint64_t)-1;
                break;
        }
        return (uint64_t)regs;
    }

    // CPU exception: dump registers and halt.
    kprint("\n*** CPU EXCEPTION ***\n");
    if (regs->int_no < 32) kprint(exception_names[regs->int_no]);
    else                   kprint("Unknown");
    kprint("  (vector ");
    kprint_uint((uint32_t)regs->int_no);
    kprint(", error ");
    kprint_hex((uint32_t)regs->err_code);
    kprint(")\n");

    dump_reg("RIP", regs->rip);
    kprint("  CS="); kprint_hex((uint32_t)regs->cs);
    kprint("  RFLAGS="); kprint_hex((uint32_t)regs->rflags);
    kprint_char('\n');
    dump_reg("RAX", regs->rax); dump_reg("  RBX", regs->rbx);
    dump_reg("  RCX", regs->rcx); dump_reg("  RDX", regs->rdx);
    kprint_char('\n');
    dump_reg("RSI", regs->rsi); dump_reg("  RDI", regs->rdi);
    dump_reg("  RBP", regs->rbp); dump_reg("  RSP", regs->rsp);
    kprint_char('\n');

    if (regs->int_no == 14) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        dump_reg("CR2", cr2);
        kprint_char('\n');
    }

    kprint("Halting.\n");
    asm volatile("cli");
    for (;;) asm volatile("hlt");
    return (uint64_t)regs;
}
