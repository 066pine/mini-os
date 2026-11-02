# SharkX OS

SharkX OS is an educational, independent **x86_64 bare-metal operating system**
written in freestanding C++ and NASM. GRUB 2 loads the kernel using
**Multiboot2**, the kernel enters 64-bit long mode, initializes its own hardware
and software subsystems, and presents an internal command shell over serial
and VGA text output.

It is **not** a Linux distribution, Linux application, userspace desktop, or
emulator. It is a small monolithic kernel that runs directly on QEMU system
emulation.

---

## Status

Functional prototype, manually verified in QEMU on **2026-11-02**.

Implemented and demonstrated:

- Multiboot2 boot through GRUB 2 into 64-bit long mode
- Serial console (COM1), VGA text console, PS/2 keyboard, serial RX IRQ
- GDT, IDT, exception handlers, PIC remapping, PIT at 100 Hz
- Multiboot2 memory-map parsing and bitmap physical frame allocator
- 8 MiB kernel heap with allocation, deallocation, coalescing, double-free
  detection, and safe out-of-memory handling
- 32-thread control-block table, 16 KiB kernel stacks, thread states
- Preemptive round-robin scheduler driven by the PIT, with real context switches
- Mutex with wait queues, deadlock cycle detection, a deadlock watchdog, and
  lock-ordering prevention demonstration
- RAMDisk VFS with create/read/update/delete and file metadata
- Software discretionary access control (DAC) with root/guest users and modes
- SharkX Shell with real commands for every subsystem

See [Known limitations](#known-limitations) for what is intentionally not
implemented.

---

## Build requirements

Tested on Ubuntu 24.04 under WSL2.

Required tools:

- `g++` with C++17 support (GCC 13.3 used here)
- `x86_64-elf-ld` (GNU binutils, ELF64 target)
- `nasm`
- `qemu-system-x86_64`
- `grub-mkrescue`, `xorriso`

Ubuntu package names (may vary slightly):

```bash
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso mtools
```

Check the detected toolchain:

```bash
make check-toolchain
```

### Toolchain note

The development environment did **not** contain `x86_64-elf-g++`. The Makefile
therefore uses the host x86_64 `g++` in freestanding mode, together with
`x86_64-elf-ld`:

- `-ffreestanding`, `-nostdinc`, `-fno-exceptions`, `-fno-rtti`
- `-fno-stack-protector`, `-fno-pic`, `-fno-pie`, `-fno-builtin`
- `-mno-red-zone`, `-mcmodel=small`, `-mno-sse`, `-mno-sse2`
- kernel-provided `memcpy`/`memset`/string/printf runtime

This produces ELF64 objects with no host libc or host C++ runtime dependency.
It is sufficient on an x86_64 Linux host, but it is not a fully isolated cross
toolchain. On a non-x86_64 host, build or install a real `x86_64-elf` GCC.

---

## Build and run

```bash
cd SharkX-OS

# Build kernel.elf and build/SharkX.iso
make

# Build if needed, then boot in QEMU (serial console)
make run

# Clean build outputs
make clean

# Rebuild from scratch
make clean && make
```

`make run` launches:

```bash
qemu-system-x86_64 \
  -cdrom build/SharkX.iso \
  -m 512M \
  -serial stdio \
  -monitor none \
  -display none \
  -no-reboot \
  -device isa-debug-exit,iobase=0x501,iosize=0x04
```

To use the VGA framebuffer and PS/2 keyboard, run QEMU with a display:

```bash
qemu-system-x86_64 -cdrom build/SharkX.iso -m 512M \
  -serial stdio -monitor none \
  -device isa-debug-exit,iobase=0x501,iosize=0x04
```

Serial output is mirrored to VGA. Input is accepted from both the serial port
and the PS/2 keyboard.

---

## Project structure

```text
SharkX-OS/
├── boot/
│   ├── entry.asm             Multiboot2 entry, page tables, 64-bit transition
│   ├── interrupts.asm        ISR stubs, register frame, dispatcher bridge
│   └── context_switch.asm    Context switch and new-thread trampoline
├── grub/grub.cfg             GRUB configuration source copy
├── include/
│   ├── kernel/               types, I/O, console, GDT/IDT, PIC, timer, panic
│   ├── drivers/              serial, VGA, keyboard
│   ├── memory/               PMM and heap interfaces
│   ├── process/              thread control block and scheduler API
│   ├── scheduler/            scheduler reporting API
│   ├── sync/                 mutex API
│   ├── filesystem/           VFS API
│   ├── security/             access-control API
│   └── lib/                  freestanding string and printf declarations
├── kernel/
│   ├── main.cpp              kernel initialization sequence
│   ├── kernel.cpp            banner, constructors, demo kernel threads
│   ├── panic.cpp             kernel panic path
│   ├── arch/                 GDT, IDT, interrupts, PIC, PIT
│   ├── drivers/              serial, VGA, keyboard, console
│   ├── memory/               Multiboot2 parsing, PMM, heap
│   ├── process/              threads and context scheduling
│   ├── scheduler/            scheduler reporting
│   ├── sync/                 mutex, deadlock detection, watchdog
│   ├── filesystem/           RAMDisk VFS
│   ├── security/             software DAC
│   ├── shell/                SharkX Shell
│   └── lib/                  memcpy/memset/string/printf runtime
├── iso/boot/grub/grub.cfg    GRUB ISO configuration
├── linker.ld                 kernel linker script
├── Makefile
└── README.md
```

`build/` and `iso/boot/kernel.elf` are generated artifacts and are ignored by
Git.

---

## SharkX Shell commands

Start the shell with `make run`, then type `help`.

| Command | What it demonstrates |
|---|---|
| `help` | Command list |
| `about` | OS identity and subsystem overview |
| `version` | Kernel version |
| `clear` | Clear serial and VGA consoles |
| `uptime` | PIT tick count and seconds |
| `meminfo` | Heap usage, Multiboot memory, PMM frames, kernel range |
| `heap-test` | Allocation, free, double-free detection, safe OOM |
| `threads` | All TCBs, states, CPU time, switches, wait reason |
| `ps` | Live threads only |
| `scheduler` | Policy, quantum, context switches, per-thread CPU time |
| `mutex-test` | Two threads + mutex, deterministic shared counter |
| `race-test` | Unsynchronized race vs. mutex-protected run |
| `deadlock-test` | Real circular wait + watchdog detection and recovery |
| `deadlock-prevent` | Lock ordering (`m1 -> m2`) prevents deadlock |
| `ls` | List RAMDisk files with mode, owner, size |
| `touch <file>` | Create a file |
| `cat <file>` | Read a file (permission checked) |
| `write <file> <text>` | Update file contents (permission checked) |
| `rm <file>` | Delete a file (permission checked) |
| `permissions [file]` | Show mode, owner, and allowed/denied operations |
| `whoami` | Current user |
| `su root` / `su guest` | Switch software user identity |
| `chmod <file> <octal>` | Change mode (root only) |
| `echo <text>` | Echo text |
| `halt` | Halt the CPU |
| `reboot` | Reset through the keyboard controller / ACPI port |
| `exit` | QEMU `isa-debug-exit` helper for automated tests |

---

## Suggested demonstration sequence

```text
help
scheduler
threads
meminfo
heap-test
mutex-test
race-test
deadlock-test
deadlock-prevent
ls
cat /readme.txt
su guest
cat /system/secret
permissions /system/secret
chmod /readme.txt 666
touch /guest.txt
write /guest.txt hello from guest
cat /guest.txt
rm /guest.txt
su root
cat /system/secret
threads
```

Expected highlights:

- `mutex-test`: `expected=2000 actual=2000 CONSISTENT`
- `race-test`: unsafe run loses updates; mutex-protected run reaches 2000
- `deadlock-test`: both threads block, watchdog reports the four Coffman
  conditions after 200 ticks, then force-releases the mutexes so both complete
- `deadlock-prevent`: both threads complete because they acquire locks in the
  same order
- `su guest` followed by `cat /system/secret`: permission denied (`0600`, root)
- `su root` followed by `cat /system/secret`: allowed

---

## Subsystem implementation

| Subsystem | Main files |
|---|---|
| Boot / long mode | `boot/entry.asm`, `linker.ld` |
| Interrupts / context switch | `boot/interrupts.asm`, `boot/context_switch.asm`, `kernel/arch/interrupts.cpp` |
| GDT / IDT / PIC / PIT | `kernel/arch/gdt.cpp`, `idt.cpp`, `pic.cpp`, `timer.cpp` |
| Console / drivers | `kernel/drivers/console.cpp`, `serial.cpp`, `vga.cpp`, `keyboard.cpp` |
| Multiboot2 / PMM | `kernel/memory/multiboot2.cpp`, `kernel/memory/pmm.cpp` |
| Heap | `kernel/memory/heap.cpp` |
| Threads / scheduler | `kernel/process/thread.cpp`, `kernel/scheduler/scheduler.cpp` |
| Mutex / deadlock | `kernel/sync/mutex.cpp` |
| VFS / RAMDisk | `kernel/filesystem/vfs.cpp` |
| Security / DAC | `kernel/security/security.cpp` |
| Shell | `kernel/shell/shell.cpp` |

### Scheduler

The scheduler is a preemptive round-robin scheduler. The PIT fires at 100 Hz;
each tick charges CPU time to the current thread and invokes the scheduler.
Real register state is saved and restored by `context_switch.asm`. New threads
start through `thread_trampoline`, which enables interrupts and calls the
thread entry function. An idle thread is selected only when no other thread is
runnable.

Thread states are `NEW`, `READY`, `RUNNING`, `WAITING`, and `TERMINATED`.
`threads`/`ps` expose the current state, wait reason, CPU time, and switch count.

CPU time is measured with the TSC, calibrated against PIT channel 2 during
boot. This gives nonzero per-thread CPU time even for short bursts between
timer ticks.

### Synchronization and deadlock

The mutex uses an owner pointer, a waiter array, and interrupt-disabled
critical sections. Blocking a thread sets `WAITING`, records the mutex in the
TCB, and calls the scheduler.

Two deadlock demonstrations are provided:

- `deadlock-test`: detection is disabled and two threads acquire two mutexes in
  opposite order. They really block. A watchdog in the timer path detects two
  waiters after 200 ticks, prints the four Coffman conditions, force-releases
  both mutexes, and wakes the threads.
- `deadlock-prevent`: both threads acquire `m1` then `m2`. Lock ordering removes
  circular wait, so both complete.

Normal mutex acquisition also performs circular-wait detection by following the
owner chain, so an attempted cycle can be refused.

### Memory

The PMM parses the Multiboot2 memory map, manages up to 1 GiB of physical
frames in a static bitmap, reserves the kernel image and Multiboot information,
and exposes free/used frame statistics.

The kernel heap is an 8 MiB aligned static pool with a first-fit free list,
16-byte alignment, block splitting, coalescing on free, header magic, and
double-free detection. Oversized requests return `nullptr` without corrupting
kernel state.

Paging is a simple identity map of the first 1 GiB using 2 MiB pages. There is
no demand paging or per-process address space yet.

### File system and security

The VFS is an in-memory RAMDisk supporting up to 64 files, each up to 2048
bytes, with a flat namespace. Each file stores owner UID, mode bits, size, and
creation/modification tick counters.

Access control is software DAC:

- `root` (uid 0) bypasses permission checks.
- `guest` (uid 1) is checked against owner/other mode bits.
- `/system/secret` is mode `0600`, owned by root.

This is a real permission check inside the kernel, but it is **not** hardware
privilege separation. There is no ring-3 user mode and no per-process MMU
isolation. See limitations below.

---

## Verified test results

Manually run in QEMU 512 MiB on 2026-11-02 after the final build:

| Test | Result |
|---|---|
| GRUB Multiboot2 boot into long mode | PASS |
| Serial shell input/output | PASS |
| VGA text and PS/2 keyboard drivers initialized | Initialized; automated input used serial (VGA/keyboard not visually verified in headless run) |
| `heap-test` | PASS: alloc/free, double-free detected, OOM returned `0x0` |
| `meminfo` | PASS: 511 MiB available, 501 MiB free, 8 MiB heap |
| `scheduler` | PASS: Round Robin, 100 Hz, TSC 2849 MHz, nonzero per-thread CPU time |
| `threads` / `ps` | PASS: RUNNING/READY/WAITING/TERMINATED states observed |
| `mutex-test` | PASS: expected 2000, actual 2000 |
| `race-test` | PASS: unsafe 1000/2000; mutex-protected 2000/2000 |
| `deadlock-test` | PASS: actual deadlock, watchdog fired after 200 ticks, recovery succeeded |
| `deadlock-prevent` | PASS: both threads completed with lock ordering |
| VFS create/read/update/delete | PASS |
| DAC denied operation | PASS: guest denied reading `/system/secret` |
| DAC allowed operation | PASS: root can read `/system/secret` |

These are manual serial-console observations, not fabricated screenshots. The
automated QEMU test script is not yet implemented.

---

## Known limitations

- **Uniprocessor only.** Interrupt-disable critical sections are sufficient for
  one CPU but are not SMP-safe.
- **Software DAC, not hardware isolation.** No ring-3 user mode, no per-process
  page tables, no syscall boundary. Root bypasses permission checks. There is no
  password authentication; `su` is an educational identity switch.
- **Identity-mapped memory.** The kernel identity maps the first 1 GiB with
  2 MiB pages. The PMM manages at most that 1 GiB.
- **Fixed heap.** The heap is an 8 MiB static pool; it cannot grow by requesting
  more physical frames.
- **RAMDisk only.** No persistent disk driver, no block layer, no journaling.
  The VFS is a flat in-memory namespace with fixed file sizes.
- **Deadlock watchdog is a demonstration/recovery mechanism.** Production
  prevention relies on lock ordering and the owner-chain cycle check.
- **Exceptions halt the kernel.** There is no process-level fault recovery.
- **Limited device support.** Serial, VGA text, PS/2 keyboard, PIC, and PIT only.
  No networking, storage, USB, APIC, or SMP.
- **BIOS/Multiboot2 path only.** No native UEFI loader.
- **`exit` is a QEMU test helper** using `isa-debug-exit`; it is not a real
  poweroff mechanism.

---

## AI usage

This implementation was developed with AI assistance during the development
session. The human operator requested the project, reviewed the results, and
ran the OS in QEMU. Detailed AI-usage documentation is planned for
`docs/AI_USAGE.md`.

---

## License

No license has been selected yet. Treat this as educational coursework unless a
license is added.
