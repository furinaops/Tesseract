# Tesseract Demo — DOOM on a Bare-Metal Ring‑0 Kernel

This directory builds a **standalone, bootable i386 ISO** that is **not** the Tesseract
Hyperkernel itself.  It is a **demonstration payload** — a minimal bare‑metal kernel
that boots directly into a shell, lets you explore a RAM filesystem, run a miniature
Python interpreter, and launch **DOOM** rendered directly to VGA mode 13h with no
operating system underneath.

---

## Project Structure

```
demo/
├── README.md              ← this file
├── Makefile               ← build:  make qemu  boots the whole thing
├── linker.ld              ← links the kernel at 1 MB (Multiboot convention)
├── boot.S                 ← Multiboot header + entry point (sets up stack, calls kernel_main)
├── kernel.c               ← shell, RAM filesystem, keyboard driver, DOOM launcher
│
├── dg_platform.c          ← DOOM → bare‑metal adaptation layer (VGA, keyboard, frame draw)
├── doom_libc.c            ← freestanding libc stubs (malloc, printf, fopen, soft‑float, …)
│
├── mini_py.c / mini_py.h  ← tiny inline Python interpreter
│
├── doom1.wad              ← Freedoom Phase‑1 IWAD (free, redistributable)
├── doom1_wad.o            ← WAD embedded as an ELF .rodata object via objcopy
│
├── doom/                  ← doomgeneric source tree (full DOOM engine)
│   ├── d_main.c           ← D_DoomMain, game loop, doomgeneric_Tick
│   ├── g_game.c           ← game state, G_InitNew, level loading
│   ├── r_*.c              ← software renderer
│   ├── p_*.c              ← gameplay / play simulation
│   ├── w_wad.c / w_file.c ← WAD reading
│   ├── st_stuff.c         ← status bar
│   ├── hu_stuff.c         ← heads‑up display
│   └── … (~70 files)      ← the rest of the DOOM engine
│
├── doominclude/           ← freestanding C headers for the bare‑metal build
│   ├── stdio.h, stdlib.h, string.h, math.h, …
│   └── sys/ (stat.h, time.h, …)
│
├── demo.elf               ← linked kernel binary
└── demo.iso               ← bootable ISO (GRUB + demo.elf)
```

---

## How it Works

1. **GRUB** loads `demo.elf` via Multiboot.
2. **`boot.S`** sets up a stack and calls `kernel_main()` in C.
3. **`kernel.c`** initialises a RAM filesystem, prints a welcome banner, and
   drops into a shell (`/home *>`).
4. Type `doom` — the shell switches VGA to mode 13h (320×200, 256 colours),
   loads the embedded Freedoom WAD, and runs the DOOM engine.
5. DOOM renders every frame into a software buffer; `DG_DrawFrame()` copies
   it to the VGA framebuffer at `0xA0000`.
6. Keyboard input comes from the PS/2 controller (`0x60`/`0x64`) which
   `DG_GetKey()` reads and maps to DOOM keycodes.
7. Type `doom` again or press **Escape** to quit back to the shell
   (`vga_restore_text_mode()`).

---

## Why a Separate Demo ISO?

The **Tesseract Hyperkernel** (at the repository root in `tesseract-kernel/`)
is a Ring – 1 hypervisor that spawns and isolates multiple Ring‑0 kernels.
The `demo/` directory is **not** the hypervisor — it is a **self‑contained
proof‑of‑concept** of what a single user‑kernel instance running under the
hypervisor might look like.

Keeping it separate simplifies development:
- You can build, run, and debug the demo without the hypervisor layer.
- The hypervisor team can test the demo as a payload without being coupled
  to the hypervisor build system.
- The demo serves as a portable reference: it runs under any Multiboot‑
  compliant bootloader, on real hardware, or in QEMU.

---

## Requirements

| Tool | Purpose |
|---|---|
| `gcc` (i386 target) | Compiles the kernel and DOOM sources |
| `grub-mkrescue` / `xorriso` | Builds the bootable ISO |
| `objcopy` (binutils) | Embeds the WAD file as an ELF object |
| `qemu-system-i386` | Runs the demo (recommended) |

On Debian/Ubuntu:

```bash
sudo apt install build-essential grub-pc-bin xorriso qemu-system-x86
```

---

## Building & Running

```bash
cd demo/
make clean && make -j4 && make iso
make qemu        # builds + runs in QEMU with serial console
```

Equivalent manual QEMU invocation:

```bash
qemu-system-i386 -m 256 -cdrom demo.iso -serial stdio
```

To see the VGA output (DOOM graphics):

```bash
qemu-system-i386 -m 256 -cdrom demo.iso -vnc :0
# connect a VNC viewer to localhost:5900
```

---

## Shell Commands

Once booted, you are at the shell prompt `/home *>`:

| Command | Description |
|---|---|
| `ls` | List files in the current directory |
| `cat <file>` | Print a file's contents |
| `touch <file>` | Create an empty file |
| `echo <text> [> <file>]` | Print text or write to a file |
| `mkdir <dir>` | Create a directory |
| `cd <dir>` | Change directory |
| `rm <file>` | Remove a file |
| `clear` | Clear the screen |
| `py <statement>` | Run a Python statement (mini interpreter) |
| `help` | Show available commands |
| **`doom`** | **Launch DOOM!** |

---

## What's Included in the ISO

| Component | Purpose |
|---|---|
| **Multiboot‑compliant kernel** | A minimal i386 ELF that boots under GRUB. |
| **Shell** | Simple command‑line interface with RAM filesystem. |
| **Mini Python interpreter** | Single‑file embedded Python for scripting. |
| **DOOM engine (doomgeneric)** | Full Doom source (renderer, gameplay, WAD I/O). |
| **Freedoom Phase‑1 IWAD** | Complete, free, redistributable game data (~28 MB). |
| **VGA mode 13h driver** | 320×200, 256‑colour linear framebuffer. |
| **PS/2 keyboard driver** | Scancode‑to‑DOOM‑key mapping. |
| **Freestanding libc** | All C library functions DOOM needs (malloc, printf, fopen, soft‑float, etc.). |

Freedoom is used instead of the shareware `doom1.wad` because the shareware
WAD is missing the `STBAR` lump that the status‑bar code expects.  Freedoom
is legally redistributable and contains all required lumps.

---

## Technical Notes

- **Timer**: `DG_GetTicksMs()` uses a simple counter incremented on every
  call.  This is enough to satisfy `TryRunTics()`, but the game runs
  "as fast as possible" rather than at a fixed 35 Hz.  The `I_Sleep(1)`
  busy‑loop adds a coarse throttle.
- **Palette**: The VGA DAC is programmed via ports `0x3C8`/`0x3C9`.  The
  DOOM palette (`PLAYPAL` lump) is written every frame when
  `palette_changed` is set.
- **WAD embedding**: `doom1.wad` is turned into an ELF `.rodata` section
  with `objcopy --rename-section .data=.rodata`.  Symbols
  `_binary_doom1_wad_start`, `_binary_doom1_wad_end`, and
  `_binary_doom1_wad_size` (an ABS symbol — use `&` to read its value)
  provide access from C.
- **Soft‑float**: The `-mno-80387` flag prevents the compiler from
  emitting x87 instructions.  DOOM uses `fixed_t` (integer) arithmetic
  for most calculations, but a few float operations remain; the linker
  pulls in `__*sf3` / `__*df2` routines from `doom_libc.c`.
- **Video mode**: The QEMU screendump command captures the VGA output
  at 640 × 400 (2 × upscaled from 320 × 200 mode 13h).

---

## License

The demo kernel and platform code are provided under the same license as the
Tesseract project.  `doomgeneric` is distributed under the Doom Source
License.  Freedoom is distributed under the BSD‑3‑Clause license.
