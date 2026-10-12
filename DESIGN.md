# Directories

Four choices, four places. Each choice is a vanilla JS module next to the sources it describes. The configurator in `tools/` loads those modules, runs `prepare`, and writes `.config`. The Makefile includes that file and does not guess.

```
.
├── .config                   active choice, written by the configurator, not versioned
├── boards/
│   └── olimex/
│       └── _phi.js           optional preset: selection, pins, prepare
│
├── tools/
│   └── configurator/         Electron app, not part of the firmware
│       ├── main.js           loads the modules, runs prepare, writes .config
│       ├── index.html        the single window
│       └── renderer.js       the form, no Node, no filesystem
│
├── include/                  API visible from everywhere
│   ├── mp.h
│   ├── config.h              sizes only (heap, tasks, states)
│   ├── autoconf.h            generated: SUPPORT_* and MCU macros
│   ├── common/
│   └── drivers/
│
├── common/                   portable kernel
│   ├── kernel.c              always compiled, no module
│   ├── state.c
│   ├── printk.c
│   ├── task/
│   │   ├── _phi.js           required: the form cannot turn it off
│   │   └── task.c
│   └── quaternion/
│       ├── _phi.js           optional
│       └── quaternion.c
├── app/
│   └── main.c                firmware: onBoot, states, pin wiring
│
├── arch/
│   ├── msp430/
│   │   ├── _phi.js           toolchain, arch define, prepare
│   │   ├── gpio.c  uart.c  spi.c  i2c.c  ...
│   │   ├── include/          arch headers, added to -I from ARCH
│   │   ├── f5xx_f6xx/
│   │   │   ├── series.js     boot files, prepare
│   │   │   └── clock.c       series boot and clock
│   │   └── ld/
│   │       ├── msp430f5438.cmd
│   │       └── msp430f5438a.cmd
│   └── stm32/
│       ├── _phi.js
│       ├── gpio.c  uart.c  ...
│       ├── include/
│       ├── f4/
│       │   ├── series.js
│       │   ├── startup.s     boot: .data, .bss, SystemInit, main
│       │   └── clock.c
│       └── ld/
│           └── stm32f407vg.ld
│
└── drivers/
    ├── led/
    │   ├── _phi.js           pins the configurator can assign
    │   └── led.c
    ├── button/
    │   ├── _phi.js
    │   └── button.c
    ├── lcd/nokia3310/
    │   ├── _phi.js
    │   └── nokia3310.c
    ├── rf/nRF8001/
    │   ├── _phi.js
    │   ├── cmds.c
    │   └── evts.c
    └── sensors/ina219/
        ├── _phi.js
        └── ina219.c
```

`build/` is generated and not versioned.

## What lives where

| Directory | Role | Who selects it |
|---|---|---|
| `arch/<arch>/_phi.js` | Toolchain and sources shared by every series of that arch | the configurator, copied into `.config` |
| `arch/<arch>/<series>/series.js` | Boot and clock. Both paths end in `main` | copied into `BOOT=` |
| `arch/<arch>/ld/` | Memory map of one MCU | copied into `LDSCRIPT=` |
| `drivers/<name>/_phi.js` | One driver. Declares its sources and its pins | copied into `DRIVERS=` and `SRC_<NAME>=`. Pin assignments go in the preset |
| `common/<name>/_phi.js` | One kernel module. `required: true` stays on (task). `required: false` is a checkbox (quaternion) | copied into `FEATURES=` |
| `common/*.c` without a module | Kernel core: `kernel.c`, `state.c`, `printk.c` | always compiled |
| `boards/<name>/_phi.js` | Optional named preset | `make BOARD=olimex`, after the configurator exports it |
| `tools/configurator/` | The window. Loads the modules above, never compiled into the firmware | |
| `.config` | The personal selection, including one made without a board | `make` with no `BOARD=` |
| `app/` | The product. The kernel does not know the board | always compiled |

`include/common` and `include/drivers` stay the API. `mp.h` only holds the seam:

```c
#if defined(MP_ARCH_MSP430)
	#include "msp430/internal.h"
#elif defined(MP_ARCH_STM32)
	#include "stm32/internal.h"
#endif
```

The arch `-I` (`arch/msp430/include`) makes that path real. `common/` calls `mp_clock_schedule`, `mp_gpio_text_handle`, `mp_uart_open`. Arch state (scheduler, registers) stays in the arch `.c` files, out of `mp_kernel_t`.

## Modules

The file is always named `_phi.js`, so each module has its own directory. The configurator scans `common/` and `drivers/` for that name.

A common module:

```js
// common/task/_phi.js
export const mod = {
  name: "TASK",
  required: true,
  sources: ["task.c"],
}

// common/quaternion/_phi.js
export const mod = {
  name: "QUATERNION",
  required: false,
  sources: ["quaternion.c"],
}
```

A driver module adds the pins the firmware opens with `mp_options_t`. The module names the roles. The preset assigns the ports:

```js
// drivers/led/_phi.js
export const mod = {
  name: "LED",
  required: false,
  sources: ["led.c"],
  pins: [{ name: "port", dir: "out" }],
}

// drivers/rf/nRF8001/_phi.js
export const mod = {
  name: "NRF8001",
  required: false,
  sources: ["cmds.c", "evts.c"],
  pins: [
    { name: "simo", dir: "out" },
    { name: "somi", dir: "in" },
    { name: "clk", dir: "out" },
    { name: "reqn", dir: "out" },
    { name: "rdyn", dir: "in" },
    { name: "reset", dir: "out" },
  ],
}
```

The form shows one row per pin. The assignment (`port = p10.7`) is stored in the preset, not in the driver. `required: true` is checked and locked.

`kernel.c`, `state.c`, and `printk.c` stay at the root of `common/`. They have no `_phi.js`.

## From the current tree

| Today | Becomes |
|---|---|
| `msp430/*.c` | `arch/msp430/*.c` |
| `msp430/F5xx_F6xx/clock.c` | `arch/msp430/f5xx_f6xx/clock.c` |
| `include/msp430/` | `arch/msp430/include/` |
| `main.c` | `app/main.c` |
| `common/task.c`, `common/quaternion.c`, … | `common/<name>/` plus `_phi.js`. Core files stay put |
| `drivers/led.c`, `drivers/sensors/*.c`, `drivers/rf/nRF8001/` | `drivers/<name>/` plus `_phi.js` |
| `include/config.h` (the `SUPPORT_*` list) | one `_phi.js` per module. `config.h` keeps the sizes only |

## Code Composer files

The linker scripts stay. They are the memory map. The Eclipse project files go: they repeat the preset, and they still point at the old tree.

| Path | Action | Where its content goes |
|---|---|---|
| `lnk_msp430f5438.cmd` | Move to `arch/msp430/ld/msp430f5438.cmd` | `LDSCRIPT=` in `boards/olimex/_phi.js` |
| `lnk_msp430f5438a.cmd` | Move to `arch/msp430/ld/msp430f5438a.cmd` | `LDSCRIPT=` when the preset picks that MCU |
| `.cproject` | Delete | Defines (`__MSP430F5438__`, `__OLIMEX_PROTOBOARD__`), source list, and linker choice move into the JS modules and `.config` |
| `.ccsproject` | Delete | Device MSP430F5438 and `libc` are the `mcu` line. The USB probe is no longer a build setting |
| `.project` | Delete | CCS and CDT natures. Nothing replaces them |
| `.settings/` | Delete | Indexer and editor preferences. They stay on the machine that created them |
| `.launches/` | Delete | Debug session. It embeds a Windows path to `miniphi.out`. The probe attaches with `mspdebug` to the `.elf` from `make` |
