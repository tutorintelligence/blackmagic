# Tutorduino — Black Magic Debug Probe

The tutorduino is an in-house BMP clone built around an STM32F103C8T6 (or
compatible GD32F103 variant).  It is a fixed-pinout design with no runtime
hardware-version detection; the pin mapping below is the only one supported.

## Hardware overview

| MCU | STM32F103C8T6 (or GD32F103 clone) |
|-----|-----------------------------------|
| Flash | 128 KiB (chip may report 64 KiB — see flashing notes) |
| RAM | 20 KiB |
| USB | Full-speed, software pull-up on PA8 |
| Target supply | Soft-start PWM via TIM1 CH3N (PB1 open-drain) |

## Pin mapping

| Signal | Pin | Notes |
|--------|-----|-------|
| TDI | PA3 | |
| TMS / SWDIO | PA4 | Direction controlled by PA1 |
| TCK / SWCLK | PA5 | |
| TDO / TRACESWO | PA6 | Also TIM3 CH1 (Manchester SWO) |
| nRST | PA2 | Active-high NPN pull-down |
| nRST sense | PA7 | Pull-up input |
| nTRST / PWR_BR | PB1 | Open-drain; also controls target power |
| TPWR sense | PB0 | ADC1 CH8 |
| UART TX | PA9 | USART1 |
| UART RX | PA10 | USART1 |
| LED (UART / running) | PB2 | Yellow |
| LED (idle) | PB10 | Orange |
| LED (error) | PB11 | Red |
| USB D+ | PA12 | |
| USB D− | PA11 | |
| USB pull-up | PA8 | Output, active-high |
| USB VBUS sense | PB13 | EXTI, both edges |
| DFU button | PB12 | Hold low at power-on to stay in bootloader |

**SWO note:** UART SWO is not available because DMA channel 5 is used by
USART1 RX.  Only Manchester-encoded SWO (via TIM3 on PA6/TDO) is supported.

## Build targets

The cross-file enables only the targets needed for the primary use-case
(STM32G0B1 and other Cortex-M/STM32 devices):

| Option | Value | Reason |
|--------|-------|--------|
| `targets` | `cortexm,stm` | Fits in 128 KiB; covers all STM32 and generic Cortex-M |
| `rtt_support` | `false` | Reduces size |
| `trace_protocol` | `1` (Manchester) | UART SWO unavailable on this hardware |

## Building

### Prerequisites

| Tool | Install |
|------|---------|
| `arm-none-eabi-gcc` | `sudo apt install gcc-arm-none-eabi` (Ubuntu) |
| `meson` | `pip install meson` |
| `ninja` | `pip install ninja` (or `sudo apt install ninja-build`) |

> **libopencm3 wrap note:** `deps/libopencm3.wrap` must use `revision = main`.
> The GitHub repo's default branch (`HEAD`) points to an empty `archive` branch;
> meson will silently clone an almost-empty directory and then fail with
> "Subproject exists but has no meson.build file" if the revision is wrong.

```sh
# From the repository root:
meson setup build-tutorduino --cross-file cross-file/tutorduino.ini
meson compile -C build-tutorduino          # firmware
meson compile -C build-tutorduino boot-bin # bootloader
```

Outputs:

| File | Flash address | Purpose |
|------|---------------|---------|
| `build-tutorduino/blackmagic_tutorduino_bootloader.bin` | `0x08000000` | DFU bootloader (≤ 8 KiB) |
| `build-tutorduino/blackmagic_tutorduino_firmware.bin` | `0x08002000` | BMP firmware |

## Flashing via ST-Link

A convenience script handles the complete flash sequence:

```sh
scripts/flash-tutorduino.sh
```

Or manually with OpenOCD (an ST-Link v2 must be connected to the target's SWD
header):

```sh
# The chip may auto-detect as 64 KiB; FLASH_SIZE overrides this.
openocd \
    -f interface/stlink.cfg \
    -c "set FLASH_SIZE 0x20000" \
    -f target/stm32f1x.cfg \
    -c "program build-tutorduino/blackmagic_tutorduino_bootloader.bin 0x08000000 verify" \
    -c "program build-tutorduino/blackmagic_tutorduino_firmware.bin   0x08002000 verify reset exit"
```

**Flash-size quirk:** The STM32F103C8T6 and most GD32F103 clones report only
64 KiB in their device ID register, but physically have 128 KiB.  Without the
`set FLASH_SIZE 0x20000` line, OpenOCD will fail when it tries to write the
firmware above address `0x08010000`.

### Alternative: STM32_Programmer_CLI (no OpenOCD required)

If OpenOCD is unavailable, STM32CubeProgrammer's CLI tool works too and does
not require the flash-size override — it writes past the reported 64 KiB limit
without complaint:

```sh
STM32_Programmer_CLI \
    --connect port=SWD freq=4000 reset=HWrst \
    --erase all \
    --write build-tutorduino/blackmagic_tutorduino_bootloader.bin 0x08000000 \
    --write build-tutorduino/blackmagic_tutorduino_firmware.bin   0x08002000 \
    --verify \
    --go 0x08000000
```

After a few seconds the probe enumerates as two CDC-ACM ports (VID/PID
`1d50:6018`).

### Subsequent updates via DFU

Once the BMP bootloader is installed, future firmware updates can be done over
USB without the ST-Link:

```sh
# Enter DFU mode: hold the PB12 button while plugging in USB, then:
dfu-util -d 1d50:6018,:6017 -s 0x08002000:leave \
    -D build-tutorduino/blackmagic_tutorduino_firmware.bin
```

## Using the probe

After flashing, two CDC-ACM ports appear:

| Port | Purpose |
|------|---------|
| `/dev/ttyACM0` (if00) | GDB server |
| `/dev/ttyACM1` (if02) | UART pass-through to target |

Connect to the GDB server and scan for targets:

```sh
arm-none-eabi-gdb -q
(gdb) target extended-remote /dev/ttyACM0
(gdb) monitor tpwr enable    # optional: power the target from the probe
(gdb) monitor swdp_scan
Available Targets:
No. Att Driver
 1      STM32G0B/C M0+
(gdb) attach 1
(gdb) load your_firmware.elf
```
