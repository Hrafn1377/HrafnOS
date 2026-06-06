#include "idt.hpp"
#include "serial.hpp"
#include "pic.hpp"
#include "sched.hpp"

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

// Must match the push order in isr.asm exactly.
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

static idt_entry idt[256];
static idt_ptr   idtr;

extern "C" void* isr_stub_table[];

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

static void idt_set_gate(int n, void* handler) {
    uint64_t addr      = (uint64_t)handler;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_init() {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    for (int i = 0; i < 49; i++) idt_set_gate(i, isr_stub_table[i]);
    asm volatile("lidt %0" : : "m"(idtr));
}

static void dump_reg(const char* name, uint64_t value) {
    kprint(name);
    kprint("=");
    kprint_ptr((void*)value);
}

// Returns the stack pointer to resume on. For a timer tick the scheduler
// may hand back a different task's stack, switching context.
extern "C" uint64_t isr_handler(registers* regs) {
    if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = (uint8_t)(regs->int_no - 32);
        if (irq == 0) {
            pic_send_eoi(0);
            sched_tick();                       // advance the tick clock
            return schedule((uint64_t)regs);    // may switch tasks
        }
        pic_send_eoi(irq);
        return (uint64_t)regs;
    }

    if (regs->int_no == 48) {                   // software yield (int $0x30)
        return schedule((uint64_t)regs);        // no EOI, no tick advance
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
    return (uint64_t)regs;   // unreachable
}