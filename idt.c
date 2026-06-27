struct idt_entry {
    unsigned short base_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load();
extern void timer_handler();
extern void keyboard_handler();
extern void mouse_handler();
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

void idt_set_gate(unsigned char num, unsigned long base, unsigned short selector, unsigned char flags) {
    idt[num].base_low = (base & 0xffff);
    idt[num].base_high = (base >> 16) & 0xffff;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void mouse_wait(unsigned char type) {
    unsigned int timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

void mouse_write(unsigned char write) {
    mouse_wait(1);
    outb(0x64, 0xd4);
    mouse_wait(1);
    outb(0x60, write);
}

unsigned char mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void pic_remap() {
    outb(0x20, 0x11); // ICW1: Init Master PIC
    outb(0xa0, 0x11); // ICW1: Init Slave PIC
    outb(0x21, 0x20); // ICW2: Master PIC offset to 0x20 (32)
    outb(0xa1, 0x28); // ICW2: Slave PIC offset to 0x28 (40)
    outb(0x21, 0x04); // ICW3: Tell Master PIC there is a Slave PIC at IRQ2
    outb(0xa1, 0x02); // ICW3: Tell Slave PIC its cascade identity (2)
    outb(0x21, 0x01); // ICW4: Use 8086 mode
    outb(0xa1, 0x01); // ICW4: Use 8086 mode
    outb(0x21, 0x00); // Clear mask registers
    outb(0xa1, 0x00); // Clear mask registers
}

void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;
    
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    pic_remap();
    
    idt_set_gate(32, (unsigned long)timer_handler, 0x08, 0x8e);
    idt_set_gate(33, (unsigned long)keyboard_handler, 0x08, 0x8e);
    idt_set_gate(44, (unsigned long)mouse_handler, 0x08, 0x8e);
    
    idt_load();

    unsigned int divisor = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));

    mouse_wait(1);
    outb(0x64, 0xa8);
    
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    unsigned char status = (inb(0x60) | 2);
    
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    
    mouse_write(0xf4);
    mouse_read();

    outb(0x21, 0x00);
    outb(0xa1, 0x00);
}
