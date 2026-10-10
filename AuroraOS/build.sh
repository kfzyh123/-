#!/bin/sh
# =====================================================================
#  AuroraOS build script
#  Produces ../AuroraOS.img : a 1.44 MiB bootable floppy image.
# =====================================================================
set -e

cd "$(dirname "$0")"

echo "[1/6] assembling boot sector (stage1)..."
nasm -f bin boot.asm -o boot.bin

echo "[2/6] assembling kernel entry..."
nasm -f elf32 entry.asm -o entry.o

echo "[3/6] compiling kernel.c..."
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
    -Wall -Wextra -O2 -c kernel.c -o kernel.o

echo "[4/6] linking kernel (flat binary @ 0x10000)..."
ld -m elf_i386 -T linker.ld -n --gc-sections -o kernel.elf entry.o kernel.o

echo "[5/6] extracting flat kernel binary..."
objcopy -O binary kernel.elf kernel.bin

KSIZE=$(stat -c %s kernel.bin 2>/dev/null || stat -f %z kernel.bin)
echo "    kernel.bin = ${KSIZE} bytes"
KSEC=$(( (KSIZE + 511) / 512 ))
echo "    needs ${KSEC} sectors"
if [ "${KSEC}" -gt 48 ]; then
    echo "ERROR: kernel too large for boot sector (loads 48 sectors). Edit KERNEL_SECTORS in boot.asm."
    exit 1
fi

echo "[6/6] building AuroraOS.img (1.44 MiB floppy)..."
# blank 1.44 MiB image (2880 sectors of 512 bytes)
dd if=/dev/zero of=../AuroraOS.img bs=512 count=2880 status=none
# sector 1: boot sector
dd if=boot.bin of=../AuroraOS.img bs=512 conv=notrunc status=none
# sector 2..: kernel
dd if=kernel.bin of=../AuroraOS.img bs=512 seek=1 conv=notrunc status=none

echo
echo "Done. Bootable image: ../AuroraOS.img"
echo "Boot it with:  qemu-system-i386 -drive format=raw,file=AuroraOS.img"
echo "(also boots on real PC BIOS, Bochs, VirtualBox floppy)"
