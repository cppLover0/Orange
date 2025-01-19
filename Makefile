CC=g++
ASM_C=nasm
TARGET=orange
SOURCEFILE=source/main.cpp
LINKFILE=build_etc/link.ld
CFLAGS=-m64
ASMFLAGS=-f elf64
LDFLAGS=-m elf_x86_64 -T $(LINKFILE) 
QEMU=qemu-system-x86_64
QEMUFLAGS= -m 1G -d int -no-reboot
TARGETISO=orange.iso
GRUBMKRESCUE=grub-mkrescue

ASMCODE=$(shell find source -name "*.asm")
OBJ=$(addprefix obj/,$(SOURCEFILE:.cpp=.cpp.o) $(ASMCODE:.asm=.asm.o))

build: clean $(TARGET)
	echo "Done"
	rm -rf build/*
	cp -r iso/* build
	cp orange build/boot
	$(GRUBMKRESCUE) -o $(TARGETISO) build
	$(QEMU) $(QEMUFLAGS) -cdrom $(TARGETISO)

clean:
	rm -rf obj/*
	rm -rf $(TARGET)
	rm -rf $(TARGETISO)

$(TARGET): $(OBJ)
	ld -o $@ $^ $(LDFLAGS)

obj/%.cpp.o: %.cpp
	mkdir -p "$$(dirname $@)"
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.asm.o: %.asm
	mkdir -p "$$(dirname $@)"
	$(ASM_C) $(ASMFLAGS) $< -o $@