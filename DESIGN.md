# Directories

Every choice in a build is a `_phi.js` file next to the sources and headers it describes. Scanned together, those files are the database the configurator reads. `make config` installs the configurator with `npm ci` and opens it. The window writes `build/`, and `make` compiles from there with a single include path, `build/include`.

```
.
├── Makefile                  make config, make (= make -C build), make flash, make clean, make doc
├── build/                    generated, not versioned
│   ├── config.js             the selection the window reopens
│   ├── Makefile              compile and link lines for that selection
│   └── include/              the only -I
│       ├── autoconf.h        PHI_BUILD_*, SUPPORT_* defines and header settings
│       ├── pinout.h          one PHI_<NAME> options initializer per device
│       ├── mp.h  config.h  list.h  ...   -> ../../common/*.h
│       ├── task/task.h       -> ../../../common/task/task.h
│       ├── ina219/INA219.h   -> ../../../drivers/sensors/ina219/INA219.h
│       └── arch/internal.h   -> ../../../arch/<arch>/include/internal.h
│
├── config/                   Electron app, never compiled into the firmware
│   ├── package.json          npm start builds the renderer, then opens the window
│   ├── main.js               IPC: evaluate, write build/, run make, save a board
│   ├── preload.cjs           window.phi bridge
│   ├── catalog.js            scans and loads every _phi.js
│   ├── resolve.js            dependencies, settings, devices, pin and gate checks
│   ├── prepare.js            runs the prepare(ctx) hooks
│   ├── generate.js           writes the build/ files, saves boards
│   ├── renderer/             React UI, bundled by Vite into config/dist/
│   └── assets/               logo.svg, icon.png
│
├── common/
│   ├── _phi.js               kind kernel: kernel.c state.c printk.c utils.c mp.c
│   ├── mp.h                  the API every source includes
│   ├── config.h              includes autoconf.h, then size defaults
│   ├── list.h  utils.h  state.h  printk.h
│   ├── task/                 required module
│   │   ├── _phi.js
│   │   ├── task.c
│   │   └── task.h
│   ├── mem/  circular/  serial/  pinout/  sensor/  regmaster/  hci/
│   ├── quaternion/  kalman/  optional modules, same shape
│   ├── input/       touch, mouse and keyboard gestures, read by the TUI
│   ├── device/      board devices by class, so an app names no driver
│   └── tui/         Things UI, for small screens; fonts/ holds the TTF files
│
├── arch/
│   └── msp430/
│       ├── _phi.js           buses, toolchains, prepare
│       ├── adc.c  gpio.c  uart.c  spi.c  i2c.c  ...
│       ├── include/          arch headers, linked as build/include/arch/
│       ├── f5xx_f6xx/
│       │   ├── _phi.js       kind series: MCUs, pins, gates, reserved, clocks
│       │   └── clock.c       boot
│   │   └── ld/
│   │       ├── msp430f5438.cmd
│   │       └── msp430f5438a.cmd
│   └── stm32/
│       ├── _phi.js           buses, arm-none-eabi-gcc toolchain, prepare
│       ├── stm32.c           machine init, MCU name, die temperature and VDDA sensors
│       ├── adc.c  gpio.c  uart.c  spi.c  i2c.c  timer.c  flash.c  gate.c  interrupt.c
│       ├── syscalls.c        newlib hooks, _sbrk bounded by HEAP_SIZE
│       ├── include/          arch headers, linked as build/include/arch/
│       └── f1/
│           ├── _phi.js       kind series: STM32F103T8 and STM32F103ZE, pins, gates, clock
│           ├── startup.c     vector table, Reset_Handler        (boot)
│           ├── clock.c       HSE + PLL, SysTick 1 kHz, WFI      (boot)
│           ├── include/series.h   F1 registers and IRQ numbers, as build/include/arch/series.h
│           └── ld/           stm32f103x8.ld (64 KB), stm32f103xe.ld (512 KB)
│
├── drivers/
│   ├── led/                  _phi.js + led.c + led.h
│   ├── button/
│   ├── lcd/nokia3310/
│   ├── rf/nRF8001/           _phi.js, nRF8001.c cmds.c evts.c, nRF8001.h aci*.h
│   └── sensors/
│       ├── ina219/  tmp006/  ads1115/  ads124x/  lsm9ds0/  mpl3115a2/
│
├── app/
│   └── sysinfo/
│       ├── _phi.js           kind app: modules, drivers, peripherals and settings it needs
│       ├── main.c            main(): banner, version, build, CPU, then a periodic report
│       ├── screen.c          the display: open, close, console keys
│       └── pages/            one directory per home page; index.c is the list
│           ├── accelerometer/index.c
│           └── system/       index.c, tasks.c and devices.c are its sub-pages
│
└── boards/                   one directory per vendor, then one per product
    ├── olimex/
    │   ├── msp430-5438-stk/
    │   │   └── _phi.js       kind board: target, app, modules, drivers, devices
    │   └── stm32-lcd/
    │       └── _phi.js       STM32F103ZE, console on UEXT USART1, TFT not driven yet
    └── artekit/
        ├── ak-stm32-lkit/
        │   └── _phi.js       kind board, requires the ak-link probe
        └── ak-link/
            ├── _phi.js       kind probe: make flash, prepare checks openocd and the USB device
            ├── ak-link.cfg   OpenOCD interface for the FT2232D
            └── flash.sh      openocd program, verify, reset
```

## What lives where

| File | Role | Ends up in |
|---|---|---|
| `arch/<arch>/_phi.js` | Arch sources and headers, the buses a device can sit on, the toolchains | `SRCS`, `include/arch/`, `CC`, `CFLAGS`, `LDFLAGS`, compile and link rules |
| `arch/<arch>/<series>/_phi.js` | Boot files, series headers, MCUs (pins, IRQ ports or lines, gates, memory, linker script, `cpu`), pins the kernel reserves, clock settings | `SRCS`, `include/arch/`, `LDSCRIPT`, `MCU`, `CPU`, `autoconf.h` |
| `common/_phi.js` | Kernel core, always built. Its headers sit at the root of `build/include` | `SRCS`, `include/`, `autoconf.h` |
| `common/<name>/_phi.js` | One module. `required: true` stays on. `requires` pulls other modules | `SRCS`, `include/<name>/`, `SUPPORT_COMMON_*` |
| `drivers/<name>/_phi.js` | One driver: its bus, its options, the modules it needs | `SRCS`, `include/<name>/`, `SUPPORT_DRV_*`, `pinout.h` |
| `app/<name>/_phi.js` | The application the kernel boots into: its `main()`, the modules and drivers it needs, the peripherals it opens | `SRCS`, `include/<name>/`, `PHI_BUILD_APP`, `autoconf.h` |
| `boards/<vendor>/<board>/_phi.js` | A saved selection, naming one app and the probes it `requires`. The configurator also starts from a blank one | `build/config.js` once picked |
| `boards/<vendor>/<probe>/_phi.js` | A programmer or debugger: the make targets it adds, the files they run | Rules in `build/Makefile` (`make flash`) |

## _phi.js records

Each file exports `phi`, a plain object, and may export `prepare(ctx)`. Paths are relative to the file.

```js
// common/mem/_phi.js
export const phi = {
	kind: "module",
	name: "MEM",
	label: "Memory pool",
	define: "SUPPORT_COMMON_MEM",
	sources: ["mem.c"],
	headers: ["mem.h"],
	settings: [
		{ name: "MP_MEM_SIZE", label: "Pool size", type: "number", unit: "bytes", default: 1024, min: 128, max: 12288, ram: true },
	],
}
```

`headers` are linked into `build/include/<headerDir>/<file>` when the record is part of the build. `headerDir` defaults to the record directory in lowercase (`mem`, `ina219`, `nrf8001`). The kernel sets `headerDir: ""` so `<mp.h>` stays at the root, and the arch sets `headerDir: "arch"`. The links are relative symlinks, and two records cannot claim the same path. `build/include` only holds the headers of what is selected, so `mp.h` includes each module header under its `SUPPORT_*` define.

Pins are `p<port>.<bit>` on numbered ports (`p10.6` on MSP430) and `p<letter><bit>` on lettered ones (`pa9` on STM32). A driver names its bus and options. The bus options (gate, SDA, SCL) come from the arch. `irq: true` only accepts pins on a port listed in the MCU's `irqPorts`. An MCU with `irqLines: true` (STM32 EXTI) takes any pin, but two interrupt pins cannot share a bit number:

```js
// drivers/sensors/tmp006/_phi.js
export const phi = {
	kind: "driver",
	name: "TMP006",
	define: "SUPPORT_DRV_TMP006",
	sources: ["TMP006.c"],
	headers: ["TMP006.h"],
	buses: ["i2c"],
	requires: ["SENSOR", "REGMASTER"],
	options: [{ name: "drdy", label: "Data ready", type: "pin", irq: true }],
}
```

Option and setting types: `pin`, `gate`, `enum`, `bool`, `number`, `text`. Settings go to `autoconf.h` unless they say `emit: "make"`, in which case they become Makefile variables (stack, heap). `ram: true` counts them in the RAM budget.

A board lists devices. A device with `driver` belongs to that driver. A device with `bus` is a peripheral the application opens itself:

```js
devices: [
	{ driver: "LED", name: "red", options: { port: "p10.6" } },
	{ driver: "INA219", name: "ina219", options: { gate: "USCI_B3", sda: "p10.1", clk: "p10.2" } },
	{ bus: "uart", name: "console", options: { gate: "USCI_A3", txd: "p10.4", rxd: "p10.5", baudRate: "9600" } },
]
```

They become `PHI_LED_RED`, `PHI_INA219_INA219`, and `PHI_CONSOLE` in `build/pinout.h`:

```c
mp_options_t options[] = PHI_LED_RED;
```

An app that should run on any board does not take those macros: it goes through `common/device` (module `DEVICE`). A driver takes part with a `device` entry naming the struct the configurator allocates and its `mp_device_driver_t`:

```js
// drivers/sensors/smb380/_phi.js
device: { struct: "mp_drv_smb380_t", ops: "mp_drv_smb380_device" },
```

The driver fills `mp_drv_smb380_device` with its class (`MP_DEVICE_DISPLAY`, `MP_DEVICE_TOUCH`, `MP_DEVICE_ACCELEROMETER`, `MP_DEVICE_LED`), `open` and `close` around its own init and fini, and optionally `face`, the common interface of the class (the `mp_tui_display_t` of a display, the `mp_input_t` of a touch panel), and `set` for an output. When `build/` is written, `common/device` generates `include/gen/devices.h`: one storage and one flash copy of the options per board device of such a driver, in board order. The app finds one with `mp_device_find(class, rank)` or `mp_device_named(name)`, then calls `mp_device_open()`, `mp_device_face()`, `mp_device_set()` and `mp_device_close()`. The board name of the device is its `who`. sysinfo uses that and names no driver.

An app is the firmware's entry point. It pulls modules and drivers the same way, and declares the bus peripherals it opens. Picking the app adds the missing ones to the selection, where the pins and gate are chosen:

```js
// app/sysinfo/_phi.js
export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	sources: ["main.c"],
	requires: ["SERIAL", "SENSOR", "DEVICE"],
	devices: [{ bus: "uart", name: "console", label: "Console" }],
	settings: [{ name: "SYSINFO_PERIOD", label: "Report period", type: "number", unit: "ms", default: 5000 }],
}
```

A board names it with `app: "sysinfo"`. `sources` on a board or in the window adds files next to the app, or provides `main()` when no app is picked. `autoconf.h` tells the firmware what it was built from: `PHI_BUILD_BOARD`, `PHI_BUILD_APP`, `PHI_BUILD_ARCH`, `PHI_BUILD_SERIES`, `PHI_BUILD_MCU`, `PHI_BUILD_TOOLCHAIN`, `PHI_BUILD_MODULES`, `PHI_BUILD_DRIVERS`, `PHI_BUILD_FLASH` and `PHI_BUILD_RAM`.

An app stays free of the architecture. It reads the CPU from `kernel->mcuVendor` and `kernel->mcuName`, the clock from `mp_clock_get_speed()`, and the tick rate from `mp_clock_tick_hz()`, which the arch implements. Delays in the app are milliseconds and get converted with that rate. `mp_serial_open()` takes the `PHI_CONSOLE` options and lets the arch own the port. `mp_machine_state_set()` is where the arch publishes the sensors that belong to the chip.

Boards and probes sit under their vendor and carry its name in `vendor`, which groups them in the window. The record name is the product name, so `PHI_BUILD_BOARD` is `"msp430-5438-stk"`. A board names the probes it is flashed with in `requires`. A probe adds make targets that run one of its scripts once `$(TARGET)` is built:

```js
// boards/artekit/ak-link/_phi.js
export const phi = {
	kind: "probe",
	name: "ak-link",
	vendor: "Artekit",
	label: "Artekit AK-LINK",
	files: ["ak-link.cfg"],
	targets: [{ name: "flash", label: "Flash", script: "flash.sh", args: "$(MCU) $(TARGET)" }],
}
```

```make
flash: $(TARGET)
	$(ROOT)/boards/artekit/ak-link/flash.sh $(MCU) $(TARGET)
```

`make flash` at the root runs it. The script maps `$(MCU)` to what its tool needs (`stm32f1*` to OpenOCD's `target/stm32f1x.cfg`), and the probe's `prepare` refuses an MCU the script does not know. Two probes cannot declare the same target.

`prepare(ctx)` checks the machine. `ctx.requireTool(cmd, hint)`, `ctx.requireFile(path, hint)` (paths may use the toolchain variables, `$(CG_TOOL_ROOT)/bin/cl430`), `ctx.info`, `ctx.warn`, `ctx.error`. It also gets `ctx.selection`, `ctx.mcu`, `ctx.toolchain`, `ctx.vars`, and `ctx.setting(name)`.

`generate(ctx)` produces files when `build/` is written, not on every evaluation. It returns `{ "include/gen/<file>": content }`, gets `ctx.root`, `ctx.selection`, `ctx.setting(name)`, `ctx.devices` (the board devices with their `macro` and the `device` entry of their driver) and `ctx.compileFont(file, options)`, and two records cannot generate the same path. Objects are rebuilt when a file of `include/gen/` changes, and when a header they include changes. A setting with `emit: "none"` only feeds `generate` and stays out of `autoconf.h`.

## TUI

TUI is Things UI, a user interface for small screens such as the 320×240 of the STM32-LCD. When a screen starts, `mp_tui_step()` shows a splash before anything else: the word miniPhi, the logo from `config/assets/logo.svg`, `MP_KERNEL_VERSION`, all at once. `common/tui` (module `TUI`) then draws a tree of things on any display. A display is one `write(display, x, y, w, h, pixels)` callback taking RGB565, plus an optional `flush`. There is no framebuffer: changes mark up to `MP_TUI_DIRTY_MAX` dirty rectangles, and `mp_tui_step()` renders one band of the current one into a buffer the caller gives to `mp_tui_init()`, at least one display row. Every `mp_tui_t` carries its own state, so several screens run side by side; `mp_tui_attach()` gives one a kernel task that draws a band per tick.

After the splash the screen is a home. `mp_tui_app()` declares an application (a name and a function that fills a page). The home lists them. Opening one shows that page, which scrolls when the content is taller than the screen, and a Back button returns to the home. The first application is System: the logo, about, version, copyright, license, and the board, CPU, clock, memory, build, uptime and tasks. It opens Task manager, the kernel tasks, and Devices, the board devices from `common/device` with their class and whether they are open.

Things are caller-allocated structs that start with an `mp_tui_widget_t`: box, label, bar, table (a header and caller-owned cells, at most 8 columns), window (children sit below the title), button, switch (a label and an on/off track that a click flips), columns, and a view whose children scroll. Columns are laid out when the page is built: side by side when each gets its minimum width, one under the other otherwise, so the same page works in landscape and in portrait. Positions are relative to the parent's client area, and drawing is clipped to it. A custom thing passes its own `mp_tui_class_t` to `mp_tui_widget_init()`; its optional `event` callback receives click, drop and wheel events, which go up to the parents until one returns non-zero. `mp_tui_widget_name()` with `mp_tui_set_text()` and `mp_tui_set_value()` updates by name, and setters only redraw when the value changes.

On every tick the TUI task calls `mp_tui_poll()`, which reads every input of the kernel (see Input) and acts on the widgets. A down highlights the button under it, a click runs it. A drag moves the nearest widget flagged `MP_TUI_DRAGGABLE`, raised above its siblings, and its drop sends `MP_TUI_EVENT_DROP` with the dragged widget as `source` to what lies under the drop point. Otherwise a drag scrolls the view under it, like the wheel. Arrows, Tab, Enter, Space, Escape, Backspace and Page Up / Down drive the home and the page through `mp_tui_key()`. A mouse also gets an arrow cursor, drawn over the bands like any dirty rectangle. Events are dropped while the splash is up.

`MP_TUI_FONT` picks a TrueType file of `common/tui/fonts/` (each with its license), `MP_TUI_FONT_SIZE`, `MP_TUI_FONT_BPP` (4 antialiased, 1 for pixel fonts) and `MP_TUI_FONT_CHARSET` (ASCII or Latin-1). `config/font.js` rasterizes it at write time into `include/gen/tui_font_default.h`, which is `mp_tui_font_default`, in flash. Text is UTF-8; a character the font lacks shows as a box. Other fonts compiled the same way can be given to `mp_tui_label_font()`.

## Input

`common/input` (module `INPUT`) turns what drivers feed into gestures. An `mp_input_t` is listed on `kernel->inputs` by `mp_input_init()` with its capabilities: touch (absolute), mouse (relative, a cursor), wheel, keys. A driver feeds it from a task or an interrupt, one producer per input: `mp_input_touch(x, y, down)`, `mp_input_motion(dx, dy)` and `mp_input_button(down)`, `mp_input_wheel(steps)`, `mp_input_key(key)`, or `mp_input_terminal(byte)` for a VT100 terminal (arrows, Home, End, Page Up / Down, Delete, a lone Escape after 50 ms). Transitions go in a queue of `MP_INPUT_QUEUE` entries, and motion is only the last position, so a fast pointer never fills it.

`mp_input_read()` gives the gestures: move, down, click, drag start once the pointer leaves the press point by more than `MP_INPUT_DRAG_THRESHOLD` pixels, drag with its delta, drop, wheel and key. A press and a release between two reads still give the drag and the drop. `mp_input_bind()` keeps an input for one reader, a screen for instance, when there are several; `mp_input_next()` lists those a reader may take. Keys are their character, or `MP_INPUT_KEY_*` from 0x100. sysinfo makes its console a keyboard this way. The STM32-LCD touch panel is a resistive input on the same list.

## Flash store

`common/kflash` (module `KFLASH`) keeps small values across power cycles in the last two pages of the internal flash, which `make flash` does not touch. A key is a name such as `sysinfo.accelerometers`, stored as its FNV-1a hash. The module that owns a key declares its size and its default with `mp_kflash_listen()`, and gets the stored value at once. Its callback runs on later changes. `mp_kflash_set()` from anywhere stores a value and tells every listener of that key, and writes nothing when the value is the same. `_bool` helpers wrap a one-byte `mp_bool_t`. A value is at most `MP_KFLASH_VALUE_MAX` bytes.

Records are appended: the hash, the size, a Fletcher-16, then the value, with the check programmed last so a record cut by a reset is skipped. When the page is full, the latest record of each key moves to the other page, which becomes active once its header is written. A page is erased once every few hundred writes. The CPU stalls while the flash is programmed, so `mp_kflash_set()` runs from a task, never from an interrupt. The arch provides `mp_flash_program()`, `mp_flash_image_end()`, `MP_FLASH_ORIGIN` and `MP_FLASH_PAGE_SIZE`; only the STM32 does for now. If the image grows into the two pages, the store stays closed and listeners keep their defaults.

## Checks

The configurator refuses to write `build/` while one of these holds:

- a pin or a gate has two owners, including the pins reserved by the series (XT1, the tick timer, JTAG), the MCU or the board (a crystal, a wired USB or SD socket). A reserved entry with `when: "NAME"` only holds while that setting is on, `when: "!NAME"` while it is off;
- an `irq` option sits on a port without an interrupt vector, or on an EXTI line another interrupt pin already uses;
- a gate is set to a mode it cannot run, or a bus pin differs from the pin the gate is wired to;
- a required option is empty, a setting is out of range, a source or header file is missing;
- the app needs a peripheral the selection does not define;
- two records link a header to the same `build/include` path;
- a board requires a probe that has no `_phi.js`, a probe file is missing, or two probes declare the same make target;
- the declared RAM exceeds the MCU.

Failed `prepare` checks (compiler not found) do not block writing. They show in the window, and `make` fails.

## The mp.h seam

The selected arch is always linked as `build/include/arch/`, so `mp.h` does not name it:

```c
#include <arch/internal.h>
```

`mp.h` pulls the C library headers itself, and each `internal.h` pulls its own vendor header (`msp430.h`, or the series registers in `arch/series.h`). `internal.h` must define `mp_arch_t`, which the kernel embeds as `kernel->arch`: the MSP430 keeps its scheduler state and die temperature there, the STM32 its clock, MCU name and VDDA. `common/` calls `mp_clock_schedule`, `mp_gpio_text_handle`, `mp_uart_open`. Everything else stays in the arch `.c` files.

## STM32

The arch copies the MSP430 API: same functions, same options, same `mp_uart_t` or `mp_i2c_t` fields the drivers touch. What differs:

- One tick is one millisecond (SysTick at 1 kHz). `mp_clock_schedule()` runs WFI when no task is pending, so SysTick or any interrupt wakes the loop.
- Every peripheral vector of `startup.c` goes to `mp_interrupt_dispatch()`, which looks up the active line in the table `mp_interrupt_set()` fills. `mp_arch_fault[]` holds the last fault (vector, CFSR, HFSR, BFAR) for the debugger.
- GPIO interrupts use EXTI: one line per bit number, shared vectors for 5 to 9 and 10 to 15.
- The I2C block wants an explicit SB / ADDR handshake. `i2c.c` keeps a phase (idle, address, data) so the USCI-style calls regMaster makes (`mp_i2c_txStart` on every ASR pass, `mp_i2c_txStop` before the last byte) turn into the F1 register sequence.
- `mp_clock_low_energy()` keeps the clock: the bus dividers are computed once at boot.
- The MCU name comes from DBGMCU_IDCODE, which the F1 only returns while a debugger is attached. Without one, sysinfo prints the flash size only.

The F1 series reserves the JTAG pins (PA13, PA14, PA15, PB3, PB4). `_SWD_ONLY` gives PA15, PB3 and PB4 back, but the firmware then turns JTAG off at boot, and the AK-LINK, which has no SWD, has to connect with SRST held (`reset_config srst_only connect_assert_srst`).

`STM32F10X_HD` (the STM32F103ZE) extends the same code: 60 interrupt lines instead of 43, GPIO ports A to G, USART3, UART4, UART5, SPI2, SPI3, I2C2 and TIM5 to TIM8, and 2 KB flash pages. The medium-density build keeps the shorter vector table. On the 144-pin package the crystal has its own pins and PD0, PD1 are FSMC D2, D3.

The `fsmc` bus drives an 8080 LCD on bank 1 in 16 bits. The `FSMC` gate claims D0 to D15, NOE and NWE through `claims` in the series, so no other device can take them; `cs` (an NE pin) and `rs` (any address line) are device options. `mp_fsmc_write_cmd()` writes with RS low, `mp_fsmc_write_data()` with RS high. The STM32-LCD wires an ILI9320 on NE1 with RS on A19 (PE3), its reset on PE2 and the backlight on PD13, active low; `drivers/lcd/ili932x` turns it into an `mp_tui_display_t` for the TUI.

The same board's touch panel is a 4-wire resistive sheet, not a controller: YD is PC0, YU is PC1, XL is PC2 and XR is PC3, all ADC inputs. `drivers/touch/resistive` drives one plate and measures the other with `mp_adc_convert()`, which borrows ADC1 between the temperature samples, then feeds an `mp_input_t`. A press is the two plates meeting. Landscape uses the same corner mapping as the ILI932x.

## Code Composer files

The linker scripts stay. They are the memory map. The Eclipse project files are gone: they repeated the selection, and they still pointed at the old tree.

| Path | Action | Where its content went |
|---|---|---|
| `lnk_msp430f5438.cmd` | Moved to `arch/msp430/ld/msp430f5438.cmd` | `ld` of the MCU in the series `_phi.js` |
| `lnk_msp430f5438a.cmd` | Moved to `arch/msp430/ld/msp430f5438a.cmd` | same, for the A part |
| `.cproject` | Deleted | Compiler and linker flags in the `ti` toolchain of `arch/msp430/_phi.js`. Defines in the arch, the MCU, and the board |
| `.ccsproject` | Deleted | The MCU entry in the series `_phi.js` |
| `.project` | Deleted | Nothing |
| `.settings/` | Deleted | Nothing |
| `.launches/` | Deleted | The probe attaches to the output of `make` |
