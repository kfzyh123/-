#!/usr/bin/env python3
"""myos build orchestrator.

Assembles a self-contained, bootable OS image (os.img) from:
  boot.asm    - 512-byte MBR bootloader (real mode -> protected mode)
  entry.asm   - kernel entry + ISR stubs
  kernel.c    - the graphical OS kernel (VGA, window mgr, users, recovery)
  linker.ld   - links the kernel at 0x10000

Output: os.img  (raw disk image: boot sector + kernel, padded to 1 MiB).
        Write it to a USB with `dd` and boot from the USB - no OS required.

Usage:  python3 build.py
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))

CC = "gcc"
NASM = "nasm"
LD = "ld"
OBJCOPY = "objcopy"

CFLAGS = [
    "-m32", "-ffreestanding", "-nostdlib", "-fno-pie",
    "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
    "-fno-builtin", "-O2", "-Wall", "-Wextra",
]


def run(cmd):
    print("  $", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def main():
    print("[1/5] assembling bootloader (nasm)")
    run([NASM, "-f", "bin", "-o", "boot.bin", "boot.asm"])

    print("[2/5] assembling kernel entry (nasm)")
    run([NASM, "-f", "elf32", "-o", "entry.o", "entry.asm"])

    print("[3/5] compiling kernel (gcc)")
    run([CC] + CFLAGS + ["-c", "kernel.c", "-o", "kernel.o"])

    print("[4/5] linking kernel (ld)")
    run([LD, "-m", "elf_i386", "-T", "linker.ld",
         "-o", "kernel.elf", "entry.o", "kernel.o"])

    # extract flat binary
    run([OBJCOPY, "-O", "binary", "kernel.elf", "kernel.bin"])

    print("[5/5] building os.img")
    boot = open(os.path.join(ROOT, "boot.bin"), "rb").read()
    kern = open(os.path.join(ROOT, "kernel.bin"), "rb").read()
    if len(boot) != 512:
        sys.exit(f"boot.bin must be 512 bytes, got {len(boot)}")
    if len(kern) > 64 * 1024:
        sys.exit(f"kernel too big for bootloader load: {len(kern)} bytes "
                 f"(bootloader only loads 64 KiB). Trim it down.")
    img = bytearray(boot + kern)
    # pad to 1 MiB so the image is a sane disk size
    target = 1 * 1024 * 1024
    if len(img) < target:
        img += b"\x00" * (target - len(img))
    out = os.path.join(ROOT, "os.img")
    with open(out, "wb") as f:
        f.write(img)

    print()
    print(f"  -> {out}")
    print(f"     total image : {len(img)} bytes ({len(img)//1024} KiB)")
    print(f"     bootloader  : {len(boot)} bytes (1 sector)")
    print(f"     kernel      : {len(kern)} bytes ({len(kern)} B, "
          f"{(len(kern)+511)//512} sectors)")
    print()
    print("Write to USB (replace /dev/sdX with your USB device, ALL data on it is wiped):")
    print("   sudo dd if=os.img of=/dev/sdX bs=4M conv=fsync && sync")
    print("   then set your BIOS to boot from the USB first.")
    print()
    print("Test in QEMU (if installed):")
    print("   qemu-system-x86_64 -drive format=raw,file=os.img")
    print()
    print("Boot-time recovery: press ANY key TWICE within ~1.2s of power-on")
    print("to enter the recovery environment. Any kernel CPU fault also drops")
    print("into recovery automatically.")


if __name__ == "__main__":
    main()
