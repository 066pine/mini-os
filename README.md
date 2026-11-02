# mini-os

Repository for **SharkX OS**, an educational, independent bare-metal x86_64
operating system written in freestanding C++ and NASM. It boots through GRUB 2
using the Multiboot2 protocol and runs directly on QEMU system emulation.

The OS project lives in [`SharkX-OS/`](SharkX-OS).

- Full project documentation: [`SharkX-OS/README.md`](SharkX-OS/README.md)
- Build: `cd SharkX-OS && make`
- Run: `cd SharkX-OS && make run`

SharkX OS is not a Linux distribution, not a userspace application, and not an
emulator or simulator. It is a small monolithic kernel with its own boot path,
interrupt handling, memory management, scheduler, synchronization primitives,
in-memory file system, access control, and command shell.
