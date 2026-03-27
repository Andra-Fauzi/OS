#!/bin/bash
# Helper script to compile hello.c for AndraOS

CLANG="/c/msys64/clang64/bin/clang"
TARGET="x86_64-andraos"

# Include paths
INCLUDES="-I/home/ANDRA/workspace/build-andraos/targ-include \
          -I/home/ANDRA/workspace/newlib-cygwin/newlib/libc/include \
          -I/c/msys64/clang64/lib/clang/22/include"

# Compilation flags
# -ffreestanding: Don't assume standard library is available
# -nostdlib: Don't use standard startup files or libraries
FLAGS="-ffreestanding -nostdlib -static -fuse-ld=lld"

echo "Step 1: Compiling hello.c..."
$CLANG --target=$TARGET -ffreestanding $INCLUDES -c hello.c -o hello.o

if [ $? -ne 0 ]; then
    echo "Compilation of hello.c failed"
    exit 1
fi

echo "Step 2: Linking hello..."
# Use ld.lld directly with symbol aliasing
SYSCALLS_OBJ="/home/ANDRA/workspace/build-andraos/libc/sys/andraos/libc_a-syscalls.o"
/c/msys64/clang64/bin/ld.lld -m elf_x86_64 \
  -static \
  hello.o crt0.o $SYSCALLS_OBJ \
  -L. -lc \
  --defsym=read=_read \
  --defsym=write=_write \
  --defsym=open=_open \
  --defsym=close=_close \
  --defsym=fstat=_fstat \
  --defsym=lseek=_lseek \
  --defsym=isatty=_isatty \
  --defsym=sbrk=_sbrk \
  --defsym=exit=_exit \
  --defsym=kill=_kill \
  --defsym=getpid=_getpid \
  -o hello

if [ $? -eq 0 ]; then
    echo "Successfully compiled 'hello'"
    file hello
    readelf -h hello | grep -E "Entry|Machine|OS/ABI"
else
    echo "Compilation failed"
    exit 1
fi
