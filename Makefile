CC=g++
ASM_C=nasm
TARGET=build/boot/orange
SOURCEFILE=source/main.cpp
LINKFILE=build_etc/link.ld
CFLAGS=-fno-stack-protector -Wall -mgeneral-regs-only -Wextra -ffreestanding -m64 -march=x86-64 -I ./source/
ASMFLAGS=-f elf64
LDFLAGS=-m elf_x86_64 -T $(LINKFILE) 
QEMU=qemu-system-x86_64
QEMUFLAGS= -m 1G -d int -no-reboot -serial file:serial_output.txt
TARGETISO=build/orange.iso
GRUBMKRESCUE=grub-mkrescue

ASMCODE=$(shell find source -name "*.asm")
OBJ=$(addprefix obj/,$(SOURCEFILE:.cpp=.cpp.o) $(ASMCODE:.asm=.asm.o))

build: clean prepare $(TARGET)
	cp -rf iso/* build
	$(GRUBMKRESCUE) -o $(TARGETISO) build
	$(QEMU) $(QEMUFLAGS) -cdrom $(TARGETISO)

prepare:
	mkdir -p build/boot/grub

clean:
	rm -rf obj/*
	rm -rf $(TARGET)
	rm -rf $(TARGETISO)
	rm -rf build/*

$(TARGET): $(OBJ)
	ld -o $@ $^ $(LDFLAGS)

obj/%.cpp.o: %.cpp
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.asm.o: %.asm
	mkdir -p "$$(dirname $@)"
	$(ASM_C) $(ASMFLAGS) $< -o $@