all:
	nasm -f elf64 some_asm.asm -o some_asm.o
	ld -m elf_x86_64 some_asm.o -o t_elf.elf
	nasm -f elf64 simple.asm -o simple.o
	ld -m elf_x86_64 simple.o -o s_elf.elf
	
	# sbrk test
	nasm -f elf64 sbrk.asm -o sbrk.o
	ld -m elf_x86_64 sbrk.o -o sbrk.elf