# Directories

Every choice in a build is a `_phi.js` file next to the sources and headers it describes. Scanned together, those files are the database the configurator reads. `make config` installs the configurator with `npm ci` and opens it. The window writes `build/`, and `make` compiles from there with a single include path, `build/include`.

```
.
├── Makefile                  make config, make (= make -C build), make clean, make doc
├── build/                    generated, not versioned
│   ├── config.js             the selection the window reopens
│   ├── Makefile              compile and link lines for that selection
│   └── include/              the only -I
│       ├── autoconf.h        PHI_BUILD_*, SUPPORT_* defines and header settings
│       ├── pinout.h          one PHI_<NAME> options initializer per device
│       ├── mp.h  config.h  list.h  ...   -> ../../common/*.h
│       ├── task/task.h       -> ../../../common/task/task.h
│       ├── ina219/INA219.h   -> ../../../drivers/sensors/ina219/INA219.h
│       └── arch/internal.h   -> ../../../arch/msp430/include/internal.h
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
│   └── quaternion/  kalman/  optional modules, same shape
│
├── arch/
│   └── msp430/
│       ├── _phi.js           buses, toolchains, prepare
│       ├── adc.c  gpio.c  uart.c  spi.c  i2c.c  ...
│       ├── include/          arch headers, linked as build/include/arch/
│       ├── f5xx_f6xx/
│       │   ├── _phi.js       kind series: MCUs, pins, gates, reserved, clocks
│       │   └── clock.c       boot
│       └── ld/
│           ├── msp430f5438.cmd
│           └── msp430f5438a.cmd
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
│       └── main.c            main(): banner, version, build, CPU, then a periodic report
│
└── boards/
    └── olimex/
        └── _phi.js           preset: target, app, modules, drivers, devices
```

## What lives where

| File | Role | Ends up in |
|---|---|---|
| `arch/<arch>/_phi.js` | Arch sources and headers, the buses a device can sit on, the toolchains | `SRCS`, `include/arch/`, `CC`, `CFLAGS`, `LDFLAGS`, compile and link rules |
| `arch/<arch>/<series>/_phi.js` | Boot files, MCUs (pins, IRQ ports, gates, memory, linker script), pins the kernel reserves, clock settings | `SRCS`, `LDSCRIPT`, `MCU`, `autoconf.h` |
| `common/_phi.js` | Kernel core, always built. Its headers sit at the root of `build/include` | `SRCS`, `include/`, `autoconf.h` |
| `common/<name>/_phi.js` | One module. `required: true` stays on. `requires` pulls other modules | `SRCS`, `include/<name>/`, `SUPPORT_COMMON_*` |
| `drivers/<name>/_phi.js` | One driver: its bus, its options, the modules it needs | `SRCS`, `include/<name>/`, `SUPPORT_DRV_*`, `pinout.h` |
| `app/<name>/_phi.js` | The application the kernel boots into: its `main()`, the modules and drivers it needs, the peripherals it opens | `SRCS`, `include/<name>/`, `PHI_BUILD_APP`, `autoconf.h` |
| `boards/<name>/_phi.js` | A saved selection, naming one app. The configurator also starts from a blank one | `build/config.js` once picked |

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

A driver names its bus and options. The bus options (gate, SDA, SCL) come from the arch. `irq: true` only accepts pins on a port with an interrupt vector:

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

An app is the firmware's entry point. It pulls modules and drivers the same way, and declares the bus peripherals it opens. Picking the app adds the missing ones to the selection, where the pins and gate are chosen:

```js
// app/sysinfo/_phi.js
export const phi = {
	kind: "app",
	name: "sysinfo",
	label: "System info",
	sources: ["main.c"],
	requires: ["SERIAL", "SENSOR"],
	devices: [{ bus: "uart", name: "console", label: "Console" }],
	settings: [{ name: "SYSINFO_PERIOD", label: "Report period", type: "number", unit: "ms", default: 5000 }],
}
```

A board names it with `app: "sysinfo"`. `sources` on a board or in the window adds files next to the app, or provides `main()` when no app is picked. `autoconf.h` tells the firmware what it was built from: `PHI_BUILD_BOARD`, `PHI_BUILD_APP`, `PHI_BUILD_ARCH`, `PHI_BUILD_SERIES`, `PHI_BUILD_MCU`, `PHI_BUILD_TOOLCHAIN`, `PHI_BUILD_MODULES`, `PHI_BUILD_DRIVERS`, `PHI_BUILD_FLASH` and `PHI_BUILD_RAM`.

`prepare(ctx)` checks the machine. `ctx.requireTool(cmd, hint)`, `ctx.requireFile(path, hint)` (paths may use the toolchain variables, `$(CG_TOOL_ROOT)/bin/cl430`), `ctx.info`, `ctx.warn`, `ctx.error`. It also gets `ctx.selection`, `ctx.mcu`, `ctx.toolchain`, `ctx.vars`, and `ctx.setting(name)`.

## Checks

The configurator refuses to write `build/` while one of these holds:

- a pin or a gate has two owners, including the pins the series reserves (XT1, the tick timer);
- an `irq` option sits on a port without an interrupt vector;
- a gate is set to a mode it cannot run, or a bus pin differs from the pin the gate is wired to;
- a required option is empty, a setting is out of range, a source or header file is missing;
- the app needs a peripheral the selection does not define;
- two records link a header to the same `build/include` path;
- the declared RAM exceeds the MCU.

Failed `prepare` checks (compiler not found) do not block writing. They show in the window, and `make` fails.

## The mp.h seam

The selected arch is always linked as `build/include/arch/`, so `mp.h` does not name it:

```c
#include <arch/internal.h>
```

The `#ifdef __MSP430__` block around it in `mp.h` is still there, because it also pulls `msp430.h` and the C library headers. `common/` calls `mp_clock_schedule`, `mp_gpio_text_handle`, `mp_uart_open`. Arch state stays in the arch `.c` files.

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
