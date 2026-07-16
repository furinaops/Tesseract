# Tesseract — The Meta-Kernel

<p align="center">
  <video src="./demo.mp4" width="640" controls></video>
  <br>
  <em>Demo of my kernel with DOOM</em>
</p>

> *"One Hyperkernel to rule them all. One Hyperkernel to bind them.
> One Hyperkernel to bring them all, and in the isolation bind them."*

Tesseract is a **Ring – 1 hyperkernel** that spawns, monitors, and destroys
fully isolated Ring 0 kernel instances.  Each instance is a complete,
independent kernel with its own memory region, scheduler, and page tables.
If one kernel crashes, the hyperkernel detects the failure, kills it,
and spawns a replacement — the other kernels keep running untouched.

The project also includes a **standalone demo kernel** (`demo/`) that
boots directly under GRUB, provides a shell with a RAM filesystem, a
mini Python interpreter, and runs **DOOM** on bare metal with no
operating system underneath.

---

## Repository Structure

```
tesseract-kernel/       ← The Hyperkernel itself (Ring –1)
    arch/x86_64/        ← Boot, GDT, paging, interrupts, syscalls
    hyperkernel/        ← Core: instance manager, scheduler, heartbeat, crypto, logs
    kernels/            ← Base kernel image & template loaded into Ring 0
    libc/               ← Minimal freestanding libc for the hypervisor
    tests/              ← Spawn, crash, and memory-isolation tests
    vuln_testing/       ← Dirty COW, DoS, and buffer-overflow exploit tests

demo/                   ← Standalone proof-of-concept kernel (Ring 0)
    doom/               ← doomgeneric source port (full DOOM engine)
    doominclude/        ← Freestanding C headers for the bare-metal build
    boot.S              ← Multiboot entry point
    kernel.c            ← Shell, RAM filesystem, keyboard driver, DOOM launcher
    dg_platform.c       ← DOOM → bare-metal adaptation (VGA, keyboard, frame draw)
    doom_libc.c         ← Freestanding libc stubs (malloc, printf, fopen, soft-float)
    mini_py.c/.h        ← Tiny inline Python interpreter
    doom1.wad           ← Freedoom Phase‑1 IWAD (free, redistributable)

project.md              ← Full MVP specification (architecture, roadmap, syscall table)
GUARDIAN.md             ← Adaptive Heartbeat & Memory Governance System spec
README.md               ← This file
```

---

## The Hyperkernel (`tesseract-kernel/`)

The hyperkernel runs in Ring – 1 (below Ring 0) and manages multiple
isolated Ring 0 kernel instances.  It is built as a Multiboot i386
ELF binary (< 50 KB) that loads under GRUB.

### Architecture

```
┌─ HYPERKERNEL (Ring –1) ──────────────────────────────────────┐
│  Bootloader (GRUB) → GDT setup → paging → interrupt handlers │
│                                                               │
│  ┌─ Instance Manager ──┐   ┌─ Scheduler ──────────────────┐  │
│  │  kspawn / kdestroy   │   │  Round‑robin across kernels  │  │
│  │  Memory isolation    │   │  Tick-based context switch    │  │
│  └──────────────────────┘   └──────────────────────────────┘  │
│                                                               │
│  ┌─ Heartbeat ──────────┐   ┌─ Guardian (Escalation) ─────┐  │
│  │  Per-tick ping       │   │  FLAGGED → DEPRECATED → DEAD │  │
│  │  Dynamic timeout     │   │  Auto‑spawn replacement      │  │
│  └──────────────────────┘   └──────────────────────────────┘  │
│                                                               │
│  ┌─ Syscall Table (Ring 0 → Ring –1) ──────────────────────┐  │
│  │  kspawn, kdestroy, kping, klog, kget_status, kschedule  │  │
│  └──────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────┘
```

### Syscall Table

| Syscall | Purpose |
|---|---|
| `kspawn(kernel_image_id)` | Allocate a new kernel instance |
| `kdestroy(kernel_id)` | Destroy a kernel instance |
| `kping(kernel_id)` | Heartbeat — prove the kernel is alive |
| `klog(kernel_id, message)` | Append to the immutable audit log |
| `kget_status(kernel_id)` | Return the kernel's current state |
| `kschedule(kernel_id)` | Yield the CPU to another kernel |

### Heartbeat & Guardian

The hyperkernel implements the **Three Strikes** escalation ladder
directly in its scheduling loop:

| Stage | Trigger | Action |
|---|---|---|
| **Healthy** | Heartbeat on time, memory reasonable | Full budget, normal priority |
| **Flagged** | Miss 1 heartbeat or memory > 50 % | Logged, memory denied |
| **Deprecated** | Miss 2 heartbeats | Memory revoked, kernel frozen |
| **Executed** | Miss 3 heartbeats | Memory wiped, ID freed, auto‑spawn |

The full specification is in [`GUARDIAN.md`](GUARDIAN.md).

### Building the Hyperkernel

```bash
cd tesseract-kernel/
make            # builds hyperkernel.elf + base_kernel.img
make qemu       # builds ISO and runs in QEMU
```

Requirements: `gcc`, `ld`, `objcopy`, `grub-mkrescue`, `xorriso`, `qemu-system-i386`.

---

## Demo Kernel (`demo/`)

The `demo/` directory is a **self-contained, bootable i386 kernel** that
showcases what a Tesseract user-kernel instance could look like.  It is
completely independent of the hyperkernel and runs under any Multiboot‑
compliant bootloader.

### Features

- Shell with RAM filesystem (`ls`, `cat`, `touch`, `echo`, `mkdir`, `cd`, `rm`, `clear`, `help`)
- Mini Python interpreter (`py <statement>`)
- **DOOM** — launches directly on the bare metal in VGA mode 13h (320 × 200, 256 colours)

### Building & Running

```bash
cd demo/
make clean && make -j4 && make iso
make qemu   # or: qemu-system-i386 -m 256 -cdrom demo.iso -serial stdio
```

To see DOOM's VGA output:

```bash
qemu-system-i386 -m 256 -cdrom demo.iso -vnc :0   # connect VNC to :5900
```

Type `doom` at the shell prompt to launch the game.

### Why a Separate Demo?

The demo is not the hyperkernel — it is a **proof‑of‑concept** of a
single Ring‑0 kernel that might run under the hypervisor.  Keeping it
separate lets you build, run, and debug it without the hypervisor layer,
and serves as a portable reference that boots on real hardware or any
Multiboot‑capable emulator.  See [`demo/README.md`](demo/README.md) for
full details.

---

## Roadmap

| Phase | Milestones | Status |
|---|---|---|
| **0: Foundation** | Cross‑compiler, Makefile, QEMU, bootloader, terminal output|
| **1: Single Spawn** | Hyperkernel loads and jumps to a base kernel|
| **2: Dual Spawn** | Two kernels in memory, context switching|
| **3: Memory Isolation** | Separate page tables, fault on cross‑access|
| **4: Heartbeat** | `kping`, zombie detection, auto‑kill|
| **5: Syscalls** | Full syscall table, keyboard forwarding|
| **6: Userland** | Filesystem, shell, multi‑user login|

The full roadmap with 23 milestones (M0–M23) is in [`project.md`](project.md).

---

## Project Documents

| File | Contents |
|---|---|
| [`project.md`](project.md) | Full MVP specification, architecture, roadmap, syscall table |
| [`GUARDIAN.md`](GUARDIAN.md) | Adaptive Heartbeat & Memory Governance System specification |
| [`demo/README.md`](demo/README.md) | Demo kernel build/run guide, DOOM technical notes |

---

## License

The Tesseract hyperkernel and demo platform code are provided under the
project's license.  `doomgeneric` is distributed under the Doom Source
License.  Freedoom is distributed under BSD‑3‑Clause.
