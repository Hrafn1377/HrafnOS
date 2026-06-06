#include "pic.hpp"
#include "io.hpp"

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1
#define PIC_EOI    0x20

#define PIT_CH0    0x40
#define PIT_CMD    0x43
#define PIT_FREQ   1193182

void pic_remap() {
    // ICW1: begin initialization, expect ICW4
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    // ICW2: vector offsets — master -> 0x20 (32), slave -> 0x28 (40)
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    // ICW3: tell master the slave is on IRQ2; give slave its cascade identity
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();

    // ICW4: 8086/88 mode
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // Masks: unmask only IRQ0 (timer) on the master; mask everything else.
    outb(PIC1_DATA, 0xFE);   // 1111 1110 -> IRQ0 enabled
    outb(PIC2_DATA, 0xFF);   // all slave lines masked
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);   // slave first, if applicable
    outb(PIC1_CMD, PIC_EOI);
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_FREQ / hz;
    outb(PIT_CMD, 0x36);                              // ch0, lo/hi, mode 3, binary
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));         // divisor low byte
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));  // divisor high byte
}