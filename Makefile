OVMF = /opt/homebrew/share/qemu/edk2-x86_64-code.fd
CXX = x86_64-elf-g++
ASM = nasm
# -mgeneral-regs-only: interrupt handlers return to interrupted code, so
# forbid SSE/xmm in the kernel (whose state we don't save across an interrupt).
CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -std=c++17 -fno-asynchronous-unwind-tables -fno-strict-aliasing -mgeneral-regs-only -O1 -mcmodel=kernel -fno-pic

# User programs: freestanding, no libc, static & non-PIE so load addr == link
# addr (no relocation), -mcmodel=large because the link base is at 512 GiB.
USER_CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -std=c++17 -fno-asynchronous-unwind-tables -fno-pic -no-pie -mcmodel=large -nostdlib -O2

all: iso/boot/hrafnos.bin

# ---- user programs -> ELFs, embedded into the kernel as the ramdisk ----
one.elf: one.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o one.elf one.cpp
two.elf: two.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o two.elf two.cpp
huginn.elf: huginn.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o huginn.elf huginn.cpp
help.elf: help.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o help.elf help.cpp
clear.elf: clear.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o clear.elf clear.cpp
echo.elf: echo.cpp user.ld
	$(CXX) $(USER_CXXFLAGS) -T user.ld -o echo.elf echo.cpp

kernel/embed.o: kernel/embed.asm one.elf two.elf huginn.elf help.elf clear.elf echo.elf
	$(ASM) -f elf64 kernel/embed.asm -o kernel/embed.o

# ---- kernel ----
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
kernel/elf.o: kernel/elf.cpp
	$(CXX) $(CXXFLAGS) -c kernel/elf.cpp -o kernel/elf.o
kernel/fb.o: kernel/fb.cpp
	$(CXX) $(CXXFLAGS) -c kernel/fb.cpp -o kernel/fb.o
kernerl/kbd.o: kernel/kbd.cpp
	$(CXX) $(CXXFLAGS) -c kernel/kbd.cpp -o kernel/kbd.o
kernel/console.o: kernel/console.cpp
	$(CXX) $(CXXFLAGS) -c kernel/console.cpp -o kernel/console.o
kernel/font.o: kernel/font.cpp
	$(CXX) $(CXXFLAGS) -c kernel/font.cpp -o kernel/font.o
kernel/ramdisk.o: kernel/ramdisk.cpp
	$(CXX) $(CXXFLAGS) -c kernel/ramdisk.cpp -o kernel/ramdisk.o

OBJS = boot/boot.o kernel/kernel.o kernel/serial.o kernel/heap.o kernel/idt.o kernel/isr.o kernel/pic.o kernel/pmm.o kernel/vmm.o kernel/sched.o kernel/gdt.o kernel/elf.o kernel/fb.o kernel/kbd.o kernel/console.o kernel/font.o kernel/ramdisk.o kernel/embed.o

iso/boot/hrafnos.bin: $(OBJS)
	x86_64-elf-ld -T linker.ld -o iso/boot/hrafnos.bin $(OBJS)

iso: iso/boot/hrafnos.bin
	x86_64-elf-grub-mkrescue -o hrafnos.iso iso/

run: iso
	qemu-system-x86_64 \
		-machine q35 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
		-cdrom hrafnos.iso \
		-boot d \
		-no-reboot \
		-serial mon:stdio

clean:
	rm -f boot/*.o kernel/*.o iso/boot/hrafnos.bin hrafnos.iso one.elf two.elf huginn.elf help.elf clear.elf echo.elf
