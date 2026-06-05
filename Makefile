OVMF = /tmp/OVMF.fd
CXX = x86_64-elf-g++
ASM = nasm
CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -std=c++17 -fno-asynchronous-unwind-tables -O1 -fno-pic

all: iso/boot/hrafnos.bin

boot/boot.o: boot/boot.asm
	$(ASM) -f elf64 boot/boot.asm -o boot/boot.o

kernel/kernel.o: kernel/kernel.cpp
	$(CXX) $(CXXFLAGS) -c kernel/kernel.cpp -o kernel/kernel.o
kernel/serial.o: kernel/serial.cpp
	$(CXX) $(CXXFLAGS) -c kernel/serial.cpp -o kernel/serial.o

iso/boot/hrafnos.bin: boot/boot.o kernel/kernel.o kernel/serial.o
	x86_64-elf-ld -T linker.ld -o iso/boot/hrafnos.bin boot/boot.o kernel/kernel.o kernel/serial.o

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