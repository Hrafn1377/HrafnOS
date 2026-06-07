OVMF = /tmp/OVMF.fd
CXX = x86_64-elf-g++
ASM = nasm
# -mgeneral-regs-only: interrupt handlers return to interrupted code, so
# forbid SSE/xmm in the kernel (whose state we don't save across an interrupt).
CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -std=c++17 -fno-asynchronous-unwind-tables -fno-strict-aliasing -mgeneral-regs-only -O1 -mcmodel=kernel -fno-pic

all: iso/boot/hrafnos.bin

boot/boot.o: boot/boot.asm
	$(ASM) -f elf64 boot/boot.asm -o boot/boot.o

kernel/isr.o: kernel/isr.asm
	$(ASM) -f elf64 kernel/isr.asm -o kernel/isr.o
kernel/gdt.o: kernel/gdt.cpp
	$(CXX) $(CXXFLAGS) -c kernel/gdt.cpp -o kernel/gdt.o

kernel/kernel.o: kernel/kernel.cpp
	$(CXX) $(CXXFLAGS) -c kernel/kernel.cpp -o kernel/kernel.o
kernel/serial.o: kernel/serial.cpp
	$(CXX) $(CXXFLAGS) -c kernel/serial.cpp -o kernel/serial.o
kernel/heap.o: kernel/heap.cpp
	$(CXX) $(CXXFLAGS) -c kernel/heap.cpp -o kernel/heap.o
kernel/idt.o: kernel/idt.cpp
	$(CXX) $(CXXFLAGS) -c kernel/idt.cpp -o kernel/idt.o
kernel/pic.o: kernel/pic.cpp
	$(CXX) $(CXXFLAGS) -c kernel/pic.cpp -o kernel/pic.o
kernel/pmm.o: kernel/pmm.cpp
	$(CXX) $(CXXFLAGS) -c kernel/pmm.cpp -o kernel/pmm.o
kernel/vmm.o: kernel/vmm.cpp
	$(CXX) $(CXXFLAGS) -c kernel/vmm.cpp -o kernel/vmm.o
kernel/sched.o: kernel/sched.cpp
	$(CXX) $(CXXFLAGS) -c kernel/sched.cpp -o kernel/sched.o

OBJS = boot/boot.o kernel/kernel.o kernel/serial.o kernel/heap.o kernel/idt.o kernel/isr.o kernel/pic.o kernel/pmm.o kernel/vmm.o kernel/sched.o kernel/gdt.o

iso/boot/hrafnos.bin: $(OBJS)
	x86_64-elf-ld -T linker.ld -o iso/boot/hrafnos.bin $(OBJS)

iso: iso/boot/hrafnos.bin
	x86_64-elf-grub-mkrescue -o hrafnos.iso iso/

run: iso
	qemu-system-x86_64 \
		-machine q35 \
		-bios $(OVMF) \
		-cdrom hrafnos.iso \
		-boot d \
		-no-reboot \
		-nographic \
		-serial mon:stdio

clean:
	rm -f boot/*.o kernel/*.o iso/boot/hrafnos.bin hrafnos.iso
