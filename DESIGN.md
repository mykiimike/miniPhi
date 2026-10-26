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
│   ├── power/                required module: power levels, load governor, holds
│   ├── mem/  circular/  serial/  pinout/  sensor/  regmaster/  hci/  usb/
│   ├── quaternion/  kalman/  optional modules, same shape
│   ├── input/       touch, mouse and keyboard gestures, read by the TUI
│   ├── device/      board devices by class, so an app names no driver
│   ├── button/      a GPIO button: one pin, its active level, one MP_DEVICE_BUTTON
│   ├── crypto/      by family: hash/sha256/..., mac/hmac/, rng/hmac_drbg/; each family dir is its API; headers as <crypto/...>
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
│       ├── f1/
│           ├── _phi.js       kind series: STM32F103T8 and STM32F103ZE, pins, gates, clock
│           ├── startup.c     vector table, Reset_Handler        (boot)
│           ├── clock.c       power levels, tickless SysTick, WFI, Stop on the RTC  (boot)
│           ├── include/series.h   F1 registers and IRQ numbers, as build/include/arch/series.h
│           └── ld/           stm32f103x8.ld (64 KB), stm32f103xe.ld (512 KB)
│       └── f4/
│           ├── _phi.js       kind series: STM32F407ZG, pins, gates, 168 MHz clocks
│           ├── startup.c     vector table, Reset_Handler        (boot)
│           ├── clock.c       PLL, tickless SysTick, WFI         (boot)
│           ├── include/series.h   F4 registers and IRQ numbers, as build/include/arch/series.h
│           └── ld/stm32f407xg.ld  1 MB flash, 128 KB system SRAM
│
├── drivers/
│   ├── led/                  _phi.js + led.c + led.h
│   ├── lcd/nokia3310/
│   ├── rf/nRF8001/           _phi.js, nRF8001.c cmds.c evts.c, nRF8001.h aci*.h
│   └── sensors/
│       ├── ina219/  tmp006/  ads1115/  ads124x/  lsm9ds0/  mpl3115a2/
│
├── app/
│   ├── _phi.js               kind app, name sysinfo: modules, drivers, peripherals and settings it needs
│   ├── main.c                main(): banner, version, build, CPU, then a periodic report
│   ├── screen.c              the display: open, close, console keys
│   ├── sysinfo.h
│   └── pages/                one directory per home page; index.c is the list
│       ├── devices/          index.c, accel.c and camera.c: categories, then a device
│       ├── cryptography/index.c   home page: class, then algorithm, then acceleration
│       ├── power/index.c     home page: level, load, wake ups, holds, time per level
│       └── system/           index.c, tasks.c and drivers.c are its sub-pages
│
└── boards/                   one directory per vendor, then one per product
    ├── olimex/
    │   ├── arm-usb-ocd-h/
    │   │   ├── _phi.js       kind probe: make flash through OpenOCD
    │   │   └── flash.sh      program, verify, reset (STM32F1 and STM32F4)
    │   ├── msp430-jtag-tiny-v2/
    │   │   ├── _phi.js       kind probe: make flash through mspdebug
    │   │   ├── flash.sh      program the ELF; closing the session runs the core
    │   │   └── 60-msp430-jtag-tiny-v2.rules
    │   ├── msp430-5438-stk/
    │   │   └── _phi.js       kind board, requires the msp430-jtag-tiny-v2 probe
    │   ├── stm32-e407/
    │   │   └── _phi.js       STM32F407ZG, console on UEXT USART6, LED on PC13, ARM-USB-OCD-H
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
| `arch/<arch>/<series>/_phi.js` | Boot files, series headers, MCUs (pins, IRQ ports or lines, gates, memory, linker script, `cpu`), pins the kernel reserves, clock settings, crypto `accelerators` | `SRCS`, `include/arch/`, `LDSCRIPT`, `MCU`, `CPU`, `autoconf.h` |
| `common/_phi.js` | Kernel core, always built. Its headers sit at the root of `build/include` | `SRCS`, `include/`, `autoconf.h` |
| `common/<name>/_phi.js` | One module. `required: true` stays on. `requires` pulls other modules | `SRCS`, `include/<name>/`, `SUPPORT_COMMON_*` |
| `drivers/<name>/_phi.js` | One driver: its bus, its options, the modules it needs | `SRCS`, `include/<name>/`, `SUPPORT_DRV_*`, `pinout.h` |
| `app/_phi.js` | The application the kernel boots into. Its `name` is what a board selects. Its `main()`, the modules and drivers it needs, the peripherals it opens | `SRCS`, `PHI_BUILD_APP`, `autoconf.h` |
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

A record names `vendor` when one company makes the part. The LED and the button are only a pin, so they leave the field out.

```js
// drivers/sensors/tmp006/_phi.js
export const phi = {
	kind: "driver",
	name: "TMP006",
	vendor: "Texas Instruments",
	define: "SUPPORT_DRV_TMP006",
	sources: ["TMP006.c"],
	headers: ["TMP006.h"],
	buses: ["i2c"],
	requires: ["DEVICE", "REGMASTER"],
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
// app/_phi.js
export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	sources: ["main.c"],
	requires: ["SERIAL", "DEVICE"],
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

## Power

`common/power` (module `POWER`, always on) picks how fast the chip runs and how deep it sleeps. There are four levels, `MP_POWER_SLEEP`, `LOW`, `MEDIUM` and `HIGH`; a higher one never consumes less. The kernel loop is: the task pass, the state tick, then `mp_power_schedule()`, which measures, decides, and calls the arch to sleep.

The governor reads the kernel loop itself. The load is the share of time the loop spent awake over a window of `MP_POWER_WINDOW` ms, smoothed by keeping half of the previous window each time. The frequency it asks for is that share of the clock that just ran, divided by `MP_POWER_TARGET` percent, so a load of that percent is content where it is. The target level is the slowest one whose `mp_clock_level_hz()` still reaches that frequency, and when two levels share a frequency it is the lower of them. The level moves up to the target at once. It moves down, in one step, only after the target has stayed below the governed level for `MP_POWER_DOWN_DELAY` ms. Any move up clears that wait, because changing the clock costs a PLL relock. `MP_POWER_PINNED` starts with the level fixed at HIGH, and `mp_power_fix(kernel, level)` pins one at run time. `MP_POWER_AUTO` gives it back. A hold can still force a higher level than the one the governor chose.

A hold is a floor or a veto. `mp_power_hold(kernel, &hold, MP_POWER_LOW, "Serial")` keeps the level at LOW or more until `mp_power_release()`, and `mp_power_hold_for()` lets go by itself after some ms. `mp_power_nodeep()` forbids a deep sleep while the frequency stays free. Holds are interrupt safe and the hold struct belongs to the caller. The serial line holds LOW for `MP_SERIAL_AWAKE` ms after each byte received. USB does not take part: it is not finished, and a role does not hold a clock. The Lepton holds HIGH while a frame is shifting. A dimmed ILI932x backlight vetoes deep sleep, because that PWM stops in Stop. `mp_power_deep()` is YES only at SLEEP, with no floor above it and no veto. The power struct also counts the wake ups per second, the level changes, the deep sleeps and the ms spent at each level. sysinfo shows them on its Power page, under the Governor switch, and a veto is named there as "Backlight no deep".

Tasks give the arch their deadline. `mp_task_idle()` is the number of ticks before the next task is due, 0 when one is pending, `MP_TASK_FOREVER` when every task sleeps. `mp_task_wake()` makes a task pending from an interrupt, and `mp_task_delay()` sets a new period from now. A task that has nothing to do should sleep, or set a long delay, rather than run to find out: every wake up costs a full pass. The drivers do so: the TUI only runs while something is dirty or an input arrives, the touch panel slows down without a finger, the ADC task and the RNG sleep when nobody waits on them.

The arch provides `mp_clock_level(kernel, level)`, which returns `FALSE` when the switch has to wait (a transfer in flight), `mp_clock_level_hz()`, `mp_clock_us()`, `mp_clock_schedule(kernel, limit)` and `mp_clock_deep()`, a name for its deepest sleep. `mp_clock_schedule()` sleeps until an interrupt or the next deadline, `limit` ticks at most. `mp_clock_blocker()` names the bus that refused the last switch; the Power page shows it beside the count of refusals. What a level can drive is the arch's business: a bus adapts itself to the clock it is given rather than asking the governor for a level.

- STM32: HIGH, MEDIUM and LOW are `MP_CLOCK_HE_FREQ`, `MP_CLOCK_ME_FREQ` and `MP_CLOCK_LE_FREQ` (the crystal itself when one of them equals it, otherwise the PLL), SLEEP is the HSI at 8 MHz. On the F1, MEDIUM defaults to 48 MHz. A switch retimes SysTick, the UARTs, I2C, SPI, the timers (duty cycles keep their share), the FSMC, the ADC prescaler and, on the F1, the USB prescaler (the PLL undivided at 48 MHz, divided by 1.5 at 72 MHz). SysTick is tickless: its reload stretches over the idle ticks, and an early interrupt counts what went by. Sleep is WFI. Stop is not used: on the STM32-LCD it is followed by the board losing power, and a press cannot wake it. `MP_CLOCK_LSE` still runs the RTC on the 32.768 kHz crystal. I2C serves every byte from an interrupt, so SCL never runs faster than 9 bits per 720 core cycles, 100 kHz at 8 MHz, whatever the device asked for; a late STOP at a slow clock lets one more byte in and leaves the F1 block BUSY. A START resets a bus left that way, and `mp_i2c_watch()`, called before each sleep, resets a bus that saw no event for 100 ms and ends its transfer as a NACK, so a hung bus cannot refuse every switch.
- MSP430: the DCO stays at `MP_CLOCK_HE_FREQ`; the levels only decide LPM0 or LPM3. TA1 runs on ACLK in continuous mode and its compare moves past the idle ticks, so one tick is 1/1024 s. LPM3 needs every UART on ACLK (9600 baud or less). Every interrupt leaves the LPM on its way out, and `mp_interrupt_state()` reads GIE, so a safe section inside an interrupt stays masked.

## TUI

TUI is Things UI, a user interface for small screens such as the 320×240 of the STM32-LCD. When a screen starts, `mp_tui_step()` shows a splash before anything else: the word miniPhi, the logo from `config/assets/logo.svg`, `MP_KERNEL_VERSION`, all at once. `common/tui` (module `TUI`) then draws a tree of things on any display. A display is one `write(display, x, y, w, h, pixels)` callback taking RGB565, plus an optional `flush`. There is no framebuffer: changes mark up to `MP_TUI_DIRTY_MAX` dirty rectangles, and `mp_tui_step()` renders one band of the current one into a buffer the caller gives to `mp_tui_init()`, at least one display row. Every `mp_tui_t` carries its own state, so several screens run side by side; `mp_tui_attach()` gives one a kernel task that draws a band per tick.

After the splash the screen is a home. `mp_tui_app()` declares an application (a name and a function that fills a page). The home lists them. Opening one shows that page, which scrolls when the content is taller than the screen, and a Back button returns to the home. The home lists Accelerometer, Cryptography, Power and System. Cryptography lists the classes in the firmware (hash, mac, cipher, kdf, rng, sign). A class lists its algorithms, and opening one says whether the chip accelerates it; HMAC also lists the hashes it runs over. A signature shows its key and signature sizes and checks itself: Ed25519 and ECDSA P-256 sign and verify a fixed message, RSA verifies a signature stored in the firmware. The modulus is `MP_RSA_BITS`, at least 3072. System shows the logo, about, version, copyright, license, and the board, CPU, clock, memory, build, uptime and tasks. It opens Task manager, the kernel tasks, Devices, the board devices from `common/device` with their class and whether they are open, and USB, the PHY ports with the role each one is locked to and the role it runs now.

Things are caller-allocated structs that start with an `mp_tui_widget_t`: box, label (`mp_tui_label_wrap()` breaks a line that does not fit and moves the next widget down), bar, table (a header and caller-owned cells, at most 8 columns), window (children sit below the title), button, switch (a label and an on/off track that a click flips), columns, and a view whose children scroll. Columns are laid out when the page is built: side by side when each gets its minimum width, one under the other otherwise, so the same page works in landscape and in portrait. Positions are relative to the parent's client area, and drawing is clipped to it. A custom thing passes its own `mp_tui_class_t` to `mp_tui_widget_init()`; its optional `event` callback receives click, drop and wheel events, which go up to the parents until one returns non-zero. `mp_tui_widget_name()` with `mp_tui_set_text()` and `mp_tui_set_value()` updates by name, and setters only redraw when the value changes.

The TUI task runs while something is dirty or an input arrives: an invalidation and an input push wake it, and it sleeps otherwise. It calls `mp_tui_poll()`, which reads every input of the kernel (see Input) and acts on the widgets. A down highlights the button under it, a click runs it. A drag moves the nearest widget flagged `MP_TUI_DRAGGABLE`, raised above its siblings, and its drop sends `MP_TUI_EVENT_DROP` with the dragged widget as `source` to what lies under the drop point. Otherwise a drag scrolls the view under it, like the wheel. A finger that starts on a button and moves does the same once the page is taller than the screen, so a list of buttons still scrolls; a short press still clicks. A widget with its own event, a switch, gets the same slack: the finger may wander 12 px past it and still click it on release, and a pull of 20 px or more scrolls instead. Arrows, Tab, Enter, Space, Escape, Backspace and Page Up / Down drive the home and the page through `mp_tui_key()`. A mouse also gets an arrow cursor, drawn over the bands like any dirty rectangle. Events are dropped while the splash is up.

A display may also give `backlight(display, percent)` and `power(display, on)`. With either, the TUI has a screen saver: it dims to `MP_TUI_DIM_LEVEL` % after `MP_TUI_DIM_AFTER` s without an input, and, when `mp_tui_hibernate()` is on, turns the display off after `MP_TUI_OFF_AFTER` s. 0 turns a step off. While hibernate is off the saver stays at the dimmed backlight, and turning it off while the panel is dark brings the backlight back to that dim. The STM32-LCD sets the off step to 0: turning the panel off is when the board loses power. Off, the backlight goes to `MP_TUI_OFF_LEVEL` % (0, dark). While the screen is off nothing is drawn, accelerometers sleep, and the inputs doze. The next input wakes it, and the press that did so is swallowed until its release, so it does not click what lies under the finger. `mp_tui_hibernate()` wakes the TUI task, so the saver follows on the next pass. `mp_tui_awake()` says whether it is lit, `mp_tui_wake()` lights it.

`MP_TUI_FONT` picks a TrueType file of `common/tui/fonts/` (each with its license), `MP_TUI_FONT_SIZE`, `MP_TUI_FONT_BPP` (4 antialiased, 1 for pixel fonts) and `MP_TUI_FONT_CHARSET` (ASCII or Latin-1). `config/font.js` rasterizes it at write time into `include/gen/tui_font_default.h`, which is `mp_tui_font_default`, in flash. Text is UTF-8; a character the font lacks shows as a box. Other fonts compiled the same way can be given to `mp_tui_label_font()`.

## Input

`common/input` (module `INPUT`) turns what drivers feed into gestures. An `mp_input_t` is listed on `kernel->inputs` by `mp_input_init()` with its capabilities: touch (absolute), mouse (relative, a cursor), wheel, keys. A driver feeds it from a task or an interrupt, one producer per input: `mp_input_touch(x, y, down)`, `mp_input_motion(dx, dy)` and `mp_input_button(down)`, `mp_input_wheel(steps)`, `mp_input_key(key)`, or `mp_input_terminal(byte)` for a VT100 terminal (arrows, Home, End, Page Up / Down, Delete, a lone Escape after 50 ms). Transitions go in a queue of `MP_INPUT_QUEUE` entries, and motion is only the last position, so a fast pointer never fills it.

`mp_input_read()` gives the gestures: move, down, click, drag start once the pointer leaves the press point by more than `MP_INPUT_DRAG_THRESHOLD` pixels, drag with its delta, drop, wheel and key. A press and a release between two reads still give the drag and the drop. `mp_input_bind()` keeps an input for one reader, a screen for instance, when there are several; `mp_input_next()` lists those a reader may take. Keys are their character, or `MP_INPUT_KEY_*` from 0x100. sysinfo makes its console a keyboard this way. The STM32-LCD touch panel is a resistive input on the same list.

A push wakes `input->reader`, the task reading the input (the TUI sets it), so the reader can sleep. `mp_input_waiting()` says when the reader has to come back: something queued, or the ms left before a lone Escape. `mp_input_doze(input, 1)` tells the driver nobody looks, and when it ends `input->task`, the driver task, is woken.

## USB

`common/usb` (module `USB`) lists the PHY ports of the chip at boot, before any of them is device or host. `caps` is what the silicon can do. `fixed` is a lock: `MP_USB_ROLE_NONE` leaves the choice open, `DEVICE` or `HOST` is the only role `mp_usb_set_role()` accepts. A free port stays at `role` `NONE`. A lock is armed in that role at start, and a device lock raises the pull-up, so a host reset moves the link from idle to connected. `mp_usb_lock()` can lock a free port once; a silicon lock cannot be widened. Turning a port off with `MP_USB_ROLE_NONE` is always allowed. Pins are taken only while a role is armed. There are no endpoints and no classes yet.

The arch publishes the ports from `mp_usb_arch_register()`. The F103 has one controller, named `usb`, locked to device. On the STM32-LCD, PD3 drives the D+ pull-up transistor and is held low while the port is armed. A host reset moves the link to connected, and a USB link change wakes the screen the way a touch does. The firmware carries no endpoint, so a host that stops the start of frames has left: the link returns to idle, and the pull-up stays so the next plug is seen. The connector does not bring VBUS to a pin. The F407 lists `otg-fs` and `otg-hs`, both free; the HS port uses the on-chip full-speed PHY on PB14 and PB15. The MSP430 F5438 has none. `mp_usb_connect()` is the pull-up in device role and the core port power in host role. The board VBUS switch is not bound yet. When the chip has at least one port, one task, named USB, applies connect, disconnect, reset, suspend, resume and fault. A clock change parks an armed PHY and does not report a link event. The pull-up comes back when that tree still has a 48 MHz USB clock, and `mp_usb_set_role()` refuses when it does not (a 48 or 72 MHz PLL on the F1, a PLL whose VCO is a multiple of 48 MHz on the F4). On the F1, USBPRE leaves a 48 MHz PLL undivided and divides a 72 MHz PLL by 1.5. The port does not hold a clock level. sysinfo does not require the module; with it, System opens a USB page. The console report prints one line per port.

## Flash store

`common/kflash` (module `KFLASH`) keeps small values across power cycles in the last two pages of the internal flash, which `make flash` does not touch. A key is a name such as `sysinfo.accelerometers`, stored as its FNV-1a hash. The module that owns a key declares its size and its default with `mp_kflash_listen()`, and gets the stored value at once. Its callback runs on later changes. `mp_kflash_set()` from anywhere stores a value and tells every listener of that key, and writes nothing when the value is the same. `_bool` helpers wrap a one-byte `mp_bool_t`. A value is at most `MP_KFLASH_VALUE_MAX` bytes.

Records are appended: the hash, the size, a Fletcher-16, then the value, with the check programmed last so a record cut by a reset is skipped. When the page is full, the latest record of each key moves to the other page, which becomes active once its header is written. A page is erased once every few hundred writes. The CPU stalls while the flash is programmed, so `mp_kflash_set()` runs from a task, never from an interrupt. The arch provides `mp_flash_program()`, `mp_flash_image_end()`, `MP_FLASH_ORIGIN` and `MP_FLASH_PAGE_SIZE`; only the STM32 does for now. If the image grows into the two pages, the store stays closed and listeners keep their defaults.

## Crypto

`common/crypto` groups the crypto modules by family: `hash/` holds `blake2s/` (module `BLAKE2S`), `sha1/` (`SHA1`), `sha256/` (`SHA256`), `sha384/` (`SHA384`) and `sha512/` (`SHA512`). SHA-384 requires SHA-512 and reuses its compression. Each algorithm is its own module, so a build only carries what an app `requires`. They all set `headerDir: "crypto"`, so code includes `<crypto/sha256.h>`, and two of them cannot publish a header of the same name.

A digest runs through a context the caller owns: `mp_sha256_init()`, `mp_sha256_update()`, `mp_sha256_final()`, which wipes the context, or `mp_sha256()` in one call. SHA-1, SHA-384, SHA-512 and BLAKE2s have the same calls. BLAKE2s adds `mp_blake2s_init_key()` for a key of up to 32 bytes. The software version sits in the module's `.c` and always builds. The schedule is a ring of 16 words, so the stack stays small on MSP430.

`common/crypto/hash` itself is the module `HASH`, one API over every hash in the build, picked by name:

```c
mp_hash_t hash;
uint8_t digest[MP_HASH_SIZE_MAX];

if(mp_hash_init(&hash, "SHA256") == TRUE) {
	mp_hash_update(&hash, data, size);
	mp_hash_digest(&hash, digest);   /* returns the size, wipes the context */
}
```

The name is the module name or the label, in any case (`"SHA256"`, `"sha-256"`). `mp_hash_init()` returns `FALSE` for an algorithm the build does not carry, so an app pulls the ones it wants with `requires`. `mp_hash_t` is a union of the contexts in the build, owned by the caller; nothing is allocated, and `mp_hash_free()` only wipes a hash that will not reach `mp_hash_digest()`. `mp_hash_count()`, `mp_hash_at()` and `mp_hash_find()` list them with their digest and block sizes. A new algorithm adds its wrappers and a line to the table in `index.c`, and a member to the union in `hash.h`.

`mac/hmac/` is the module `HMAC` (RFC 2104), which requires `HASH` and runs over any hash of the build: `mp_hmac_init(&hmac, "SHA256", key, keylen)`, `mp_hmac_update()`, `mp_hmac_final()`, which returns the tag size and wipes the context, or `mp_hmac()` in one call. A key longer than the block is hashed first. The context keeps the inner hash and the key XOR opad, about 350 bytes when SHA-512 is in the build.

`common/crypto/mac` itself is the module `MAC`, the same kind of API for MACs: `mp_mac_init(&mac, "HMAC-SHA256", key, keylen)`, `mp_mac_update()`, `mp_mac_final()`, `mp_mac_free()`. `HMAC-` takes any hash name of the build, and `BLAKE2S` is the keyed BLAKE2s (1 to 32 bytes of key). `mp_mac_equal()` compares two tags in a time that does not depend on where they differ; checking a tag with `memcmp()` tells an attacker how much of a forgery was right. CMAC and GMAC wait for AES under `cipher/`, and Poly1305 comes with ChaCha20.

`rng/hmac_drbg/` is the module `HMAC_DRBG` (NIST SP 800-90A), which requires `HMAC`. It is a generator, so it lives under `rng/` and not next to HMAC: the MAC is the primitive it consumes. `mp_hmac_drbg_init(&drbg, "SHA256", entropy, entropylen, nonce, noncelen, personal, personlen)` seeds it, `mp_hmac_drbg_generate()` draws at most `MP_HMAC_DRBG_MAX` bytes, and `mp_hmac_drbg_reseed()` mixes new entropy in. After `MP_HMAC_DRBG_RESEED` draws, generate refuses until a reseed. The context holds the HMAC key and the chaining value, both the digest size, and the caller owns it. The entropy is the caller's: the module reads no device. NIST approves SHA-1, SHA-256, SHA-384 and SHA-512 for this construction; another hash of the build runs the same steps. The seed material is fed to HMAC in pieces, so a long seed never needs its own buffer.

`common/crypto/rng` itself is the module `RNG`. The kernel calls `mp_rng_start()` once the task pool exists, and `mp_rng_stop()` wipes the generator on the way down. One task, named RNG, reseeds HMAC-DRBG after `MP_RNG_RESEED` draws, or on the first draw once the seed is `MP_RNG_PERIOD` ms old (a minute unless the build says otherwise), and sleeps while nobody draws. The noise is the low byte of the die temperature and of the internal reference; the clock and a counter ride along so two reseeds differ. `mp_rng_bytes()` draws from that generator and returns `FALSE` until the first sample has seeded it. SHA-256 is the hash when the build carries it.

`bn/` is the module `BN`, the unsigned arithmetic the signatures share. A number is at most `MP_BN_MAX` little-endian words of 32 bits: the RSA modulus when RSA is in the build, otherwise 512 bits. It adds, multiplies, divides, exponentiates modulo an odd modulus (Montgomery) and tests primes with Miller-Rabin. One set of buffers is the scratch, so these calls are not reentrant: the kernel does not preempt, and an interrupt must not enter them. The directory is `common/crypto/bn`, three path components, so `BN` is not a class on the Cryptography page.

`sign/ed25519/` is the module `ED25519` (RFC 8032). A private key is the 32-byte seed, the public key is 32 bytes and the signature is 64. Signing is deterministic: SHA-512 makes the nonce. `sign/ecdsa_p256/` is `ECDSA_P256`, ECDSA on NIST P-256 with SHA-256 and an RFC 6979 nonce. The private key is 32 bytes, the public key is x and y (64 bytes, no `0x04` prefix) and the signature is r and s (64). `sign/rsa/` is `RSA`, PKCS#1 v1.5 SHA-256 with the exponent 65537. `MP_RSA_BITS` is the modulus, at least 3072 and a multiple of 64, and the two primes are half of that. The public key is the modulus and the exponent, the private key is the CRT form and the signature is one modulus. `mp_rsa_keypair()` expands a seed of at least 32 bytes with HMAC-DRBG. That is far too slow for this screen, so the page only verifies a signature stored in the firmware, and only for a 3072-bit modulus. Signing uses the CRT. Each of these keeps its scratch in the module, so one signature operation runs at a time.

`common/crypto/sign` itself is the module `SIGN`, the same kind of API: `mp_sign_keypair(name, seed, seedlen, priv, pub)`, `mp_sign()`, `mp_sign_verify()`, and `mp_sign_count()`, `mp_sign_at()`, `mp_sign_find()`. The name is the module name or the label, in any case (`"ED25519"`, `"ed25519"`). The caller owns the key and signature buffers. `MP_SIGN_PRIV_MAX`, `MP_SIGN_PUB_MAX` and `MP_SIGN_SIG_MAX` follow the RSA modulus. A new algorithm adds one line to the table in `index.c`.

A chip with a hash block declares it on its series, or on one MCU, with the module name as the key:

```js
// arch/stm32/<series>/_phi.js
accelerators: { SHA256: { sources: ["hash.c"], headers: ["include/sha256.h"] } },
```

When that module is in the build, its sources join `SRCS`, its headers are linked under the series `headerDir` (`<arch/sha256.h>`, which defines `mp_arch_sha256_t`) and `MP_ARCH_SHA256` goes to `autoconf.h`. An MCU entry replaces the series one, and `SHA256: null` removes it. The arch then implements `mp_arch_sha256_init()`, `_update()` and `_final()`, declared in `sha256.h`. `init` returns `NO` when the block is busy or off, and that context runs in software. The API stays the same either way.

Writing `build/` also writes `include/gen/crypto.h`: every `common/crypto` module linked in the firmware, and every block the series or the MCU declares, at most `MP_CRYPTO_MAX` (24). The directory under `common/crypto/` is the class (`hash`, `mac`, `cipher`, `kdf`, `rng`, `sign`), and `hardware` is set when the chip has the block. Nothing linked and no block gives `MP_CRYPTO_COUNT 0`. The API modules, `HASH`, `MAC`, `RNG` and `SIGN`, sit in the class directory itself and are not listed, and neither is `BN`. sysinfo requires BLAKE2S, SHA1, SHA256, SHA384, SHA512, HMAC, HMAC_DRBG, RNG, ED25519, ECDSA_P256, RSA, SIGN, CIPHER, AES, MODE, CBC, CTR, GCM, CHACHA20, MAC, POLY1305, CHACHA20_POLY1305 and CCM, so Cryptography shows Hash with those five, MAC with HMAC and Poly1305, Cipher with AES and ChaCha20, Mode with CBC, CTR, GCM, CCM and ChaCha20-Poly1305, RNG with HMAC-DRBG, and Signature with Ed25519, ECDSA P-256 and RSA. Opening one says the STM32F103 does not accelerate it. HMAC and HMAC-DRBG also list the hashes they run over and whether the chip accelerates each of them. The RNG class page first names the generator the task reseeds (HMAC-DRBG / SHA-256), then shows 12 bytes from `mp_rng_bytes()` in hex, redrawn every 500 ms while the page is open, or "Seeding..." until the first sample, and lists the algorithms below. Opening a signature shows the private, public and signature sizes, then a self-test of the message "miniPhi". Ed25519 and ECDSA P-256 sign and verify from a fixed seed and show the time. RSA verifies a public key and a signature stored in the firmware when the modulus is 3072 bits, and says there is no vector otherwise; the page does not generate a key and does not sign.

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

`mp.h` pulls the C library headers itself, and each `internal.h` pulls its own vendor header (`msp430.h`, or the series registers in `arch/series.h`). `internal.h` must define `mp_arch_t`, which the kernel embeds as `kernel->arch`: the MSP430 keeps its die temperature there, the STM32 its clock, MCU name and VDDA. `common/` calls `mp_clock_schedule`, `mp_gpio_text_handle`, `mp_uart_open`. Everything else stays in the arch `.c` files.

## STM32

The arch copies the MSP430 API: same functions, same options, same `mp_uart_t` or `mp_i2c_t` fields the drivers touch. What differs:

- One tick is one millisecond (SysTick at 1 kHz, tickless while idle). `mp_clock_schedule()` sleeps in WFI or Stop until the next task or an interrupt, see Power. `mp_clock_delay()` runs WFI between the ticks.
- Every peripheral vector of `startup.c` goes to `mp_interrupt_dispatch()`, which looks up the active line in the table `mp_interrupt_set()` fills. `mp_arch_fault[]` holds the last fault (vector, CFSR, HFSR, BFAR) for the debugger.
- GPIO interrupts use EXTI: one line per bit number, shared vectors for 5 to 9 and 10 to 15.
- The I2C block wants an explicit SB / ADDR handshake. `i2c.c` keeps a phase (idle, address, data) so the USCI-style calls regMaster makes (`mp_i2c_txStart` on every ASR pass, `mp_i2c_txStop` before the last byte) turn into the F1 register sequence.
- The clock changes at run time with the power level. The bus dividers follow, and a peripheral that keeps its clock settings registers itself so it can be retimed. A switch waits while I2C, SPI or a UART transmits.
- Unused pins are analog inputs from boot, the lowest leakage, except the debug ones; a released pin goes back to analog. The ADC is only powered while a conversion runs.
- The MCU name comes from DBGMCU_IDCODE, which the F1 only returns while a debugger is attached. Without one, sysinfo prints the flash size only.

The F4 series records the STM32F407ZG (1 MB flash, 128 KB system SRAM, 64 KB CCM left out of the linker script) and the gates at their reset alternate. Its clock runs HIGH, MEDIUM and LOW from the PLL (or the HSE crystal when the level matches it) and SLEEP from the 16 MHz HSI, with tickless WFI. GPIO, EXTI, USART, SPI, I2C, the basic timers and ADC1 share the F1 drivers and branch where the registers differ: MODER instead of CRL, SYSCFG instead of AFIO, no ADC calibration, sector erase. Flash programming, SPI and I2C alternate functions, and timer alternate functions compile; there is no CCM, Ethernet, SD, HASH or CRYP driver. USB is the PHY port list in `arch/stm32/usb.c`, not a class stack. The Olimex STM32-E407 selects this series, puts the console on USART6 (PC6 TX, PC7 RX), the green LED on PC13, and the WKUP button on PA0, active high. It reserves the crystal, Ethernet, USB and microSD pins.

The F1 series reserves the JTAG pins (PA13, PA14, PA15, PB3, PB4). `_SWD_ONLY` gives PA15, PB3 and PB4 back, but the firmware then turns JTAG off at boot, and the AK-LINK, which has no SWD, has to connect with SRST held (`reset_config srst_only connect_assert_srst`).

`STM32F10X_HD` (the STM32F103ZE) extends the same code: 60 interrupt lines instead of 43, GPIO ports A to G, USART3, UART4, UART5, SPI2, SPI3, I2C2 and TIM5 to TIM8, and 2 KB flash pages. The medium-density build keeps the shorter vector table. On the 144-pin package the crystal has its own pins and PD0, PD1 are FSMC D2, D3.

The `fsmc` bus drives an 8080 LCD on bank 1 in 16 bits. The `FSMC` gate claims D0 to D15, NOE and NWE through `claims` in the series, so no other device can take them; `cs` (an NE pin) and `rs` (any address line) are device options. `mp_fsmc_write_cmd()` writes with RS low, `mp_fsmc_write_data()` with RS high. The STM32-LCD wires an ILI9320 on NE1 with RS on A19 (PE3), its reset on PE2 and the backlight on PD13, active low; `drivers/lcd/ili932x` turns it into an `mp_tui_display_t` for the TUI. PD13 is TIM4 channel 2 on the full remap, so with `backlightTimer: "TIM4"` the backlight is a 1 kHz PWM the screen saver dims. Off, the panel goes to standby (STB in R10) after the display is turned off, and its power registers are written again when it comes back; the TUI redraws the whole screen.

The same board's touch panel is a 4-wire resistive sheet, not a controller. `boards/olimex/stm32-lcd/touch.c` names the glass: YD is PC0, YU is PC1, XL is PC2, XR is PC3, 240 by 320, landscape with the rows mirrored, raw window 250 to 3850. `common/resistive` drives one plate and measures the other with `mp_adc_convert()` (`mp_adc_channel()` turns the pin into an ADC1 channel), then registers an `MP_DEVICE_TOUCH`. A press is the two plates meeting. The screen subscribes when it opens, which starts the sampling: every 20 ms with a finger, every 100 ms without, and every 50 ms while the input dozes, long enough for Stop in between. Between samples YD, pulled up with XL low, waits on a falling edge, which a firm press triggers at once.

A GPIO button is the same kind of split. `common/button` takes one pin and the active level the board stored on `face.active`, arms the interrupt on the opposite edge of the level it just read, and registers an `MP_DEVICE_BUTTON`. The first subscriber starts it; the task reports the raw level through `mp_device_button_level`. The Olimex MSP430-5438-STK names five active-low buttons, `button0.gpio.left` through `button4.gpio.down` on P1.6, P1.5, P1.4, P1.3 and P1.2. The STM32-E407 names WKUP, `button0.gpio.wkup` on PA0, active high. Press patterns stay above that contract.

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
