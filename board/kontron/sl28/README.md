# SMARC-sAL28

## LS1028A SoC Overview

Please refer arch/arm/cpu/armv8/fsl-layerscape/doc/README.soc.

## Board Overview

The Kontron SMARC-sAL28 board is a TSN-enabled dual-core ARM A72 processor
module with an on-chip 6-port TSN switch and a 3D GPU.

## Boot Process

The LS1028A processors starts its boot process with a reset configuration
word. This RCW can also contain a small set of interpreted commands,
including conditions. (Usually) the last command of the RCW copies a
bootloader from a storage device the internal SRAM and jumps to its entry
point. It is also possible to skip the copying and do execute-in-place from
SPI flash, which is mapped to a memory window at `2000_0000h`.

On the SMARC-sAL28 board it is possible to load the RCW from SPI, I2C or SD
card. The bootloader payload is always read from the SPI flash, but there
is also a failsafe image of it. See the [flash map](#spi-flash-memory-map)
below.

By default the SMARC-sAL28 board loads the RCW from the I2C EEPROM. The RCW
code then figures out what the boot source was and loads the bootloader
payload appropriately. In case of the I2C EEPROM this is the SPI flash at
offset `21_0000h`. The user is able to update the I2C EEPROM as well as the
second half of the SPI flash.

If something went wrong during an update it should be possible to still
boot the board and recover it. For this, the SMARC-sAL28 board provides a
failsafe image, which can be activated by asserting the `FORCE_RECOV#` line
on the SMARC connector. In this case, the RCW source is the SPI flash (at
write-protected offset 0h). As before, the RCW figures out where it was
loaded from and loads the bootloader payload from (write-protected) SPI
flash at offset `1_0000h`.

As a last resort boot mechanism, the SMARC-sAL28 supports loading the RCW
from SD card. To use this, assert the `MODULE_TEST#` line. Please keep in
mind, that this also enables other (undocumented) test features of the
module. As a customer, you shouldn't use this boot method, nor should you
ever has to use it. But for the sake of completeness, it is described here.
After the RCW is loaded from SD card at offset `1000h` it loads the
bootloader payload from SD card at offset `10_0000h`.

The bootloader payload is a u-boot secondary program loader (SPL) with a
proper u-boot. The RCW actually only copies the SPL to internal SRAM and
jumps to its entry point. The SPL initialize the SDRAM, figues out where it
was bootet from and copies the proper u-boot from the SPI flash (either
from the failsafe or from the normal offset) or from SD card to SDRAM and
jumps to its entry point.

## One Binary To Rule Them All

Because the boot process uses the SPL in every case, one binary can be used
for all boot sources and any offset within these. Eg. the resulting u-boot
binary can be copied either to the failsafe SPI flash offset, the normal
boot SPI flash offset or to the SD card. Also, there is only one RCW binary
which can be used for any of these three storages.

## Environment

If the proper u-boot detects that it is loaded either from failsafe SPI
flash or from SD card it will only load the compiled-in default
environment. Otherwise, eg. in the normal SPI boot, the environment is
loaded from SPI flash offset `3F_0000h`.

## SPI Flash Memory Map

| Start      | End        | Size     | WP |Contents                            |
| ---------- | ---------- | --------:| -- | ---------------------------------- |
| `00_0000h` | `00_FFFFh` |   64 kiB | x  | reset configuration word           |
| `01_0000h` | `0F_FFFFh` |  960 kiB | x  | failsafe bootloader                |
| `10_0000h` | `13_FFFFh` |  256 kiB | x  | failsafe DP controller firmware    |
| `14_0000h` | `1D_FFFFh` |  640 kiB | x  | failsafe TF-A image                |
| `1E_0000h` | `1F_FFFFh` |  128 kiB | x  | *reserved*                         |
| `20_0000h` | `20_FFFFh` |   64 kiB |    | configuration store                |
| `21_0000h` | `2F_FFFFh` |  960 kiB |    | bootloader                         |
| `30_0000h` | `33_FFFFh` |  256 kiB |    | DP controller firmware             |
| `34_0000h` | `3D_FFFFh` |  640 kiB |    | TF-A image                         |
| `3E_0000h` | `3F_FFFFh' |  128 kiB |    | bootloader envrionment (redundant) |

The flash map is divided into two sections. Both sections have the same
layout. One is factory write protected while the other is inteded to be
updated by the user.

## Configuration Store

The board can be configured with different reset configuration words. To
make it easy for the user to switch between them, there is an extra
partition where all factory provided RCWs are stored. At the beginning of
this partition, there is a script which can be run by the user to list and
install the RCWs. The u-boot provides a handy environment variable
`install_rcw` which can be executed with `run install_rcw`.

## Failsafe Watchdog

Besides the two watchdogs of the CPU, the board has one extra watchdog
implemented in the CPLD. This watchdog has two modes: failsafe and normal.

After a reset or on power-up, the watchdog will be automatically started in
failsafe mode. After reset it has a timeout of 5 seconds. If it is not
kicked nor stopped, the board will reset and boot into failsafe mode.

The board-specifc code in u-boot will either stop it automatically late in
the boot process (default) or kick it again, so the user has additional 5
seconds to do reconfigure with the watchdog (see [environment
variables](#environment-variables). In either way it is up to the user to
enable it again, stop it, or change its mode using the `wdt` command in the
bootscript.

To have a complete supervised boot process, the following steps are
necessary:
1. set the environment variable `kick_failsafe_watchdog` to a non-zero
   value,
2. change the watchdog mode in the bootcmd or the bootscript to normal by
   using the `wdt start <timeout_ms>` command. Note that the flags are 0 in
   the command, which selectes the normal mode,
3. kick the watchdog in linux from your application.

In normal mode, the board will still be resetted, but won't start in
failsafe mode.

## Failsafe Mode

The board has a special failsafe mode, which ensures that at least the
bootloader will start and stop at its prompt. This mode will either be
entered if
* the failsafe watchdog expires, see [here](#failsafe-watchdog),
* the `FORCE_RECOV#` line is asserted, see [here](#boot-process).

In this mode the user can repair the board, ie by restoring the onboard
flash memories. A normal board reset can be used to boot into normal mode
again.

## Watchdog Command Flags

The `wdt start` as well as the `wdt expire` command take watchdog flags
argument.

| Bit | Description                   |
| --- | ----------------------------- |
|   0 | Enable failsafe mode          |
|   1 | Lock the control register     |
|   2 | Disable board reset           |
|   3 | Enable WDT_TIME_OUT# line     |

For example, you can use ´wdt expire 1` to issue a reset and boot into
failsafe mode.

## Environment Variables

The following custom variables exists, either set automatically be the
bootloader or can be set by the user to alter the boot process.

| Variable name          | Description                                           |
| ---------------------- | ----------------------------------------------------- |
| kick_failsafe_watchdog | If exists, watchdog will be kicked instead of stopped |
| install_rcw            | Script to run the RCW updater                         |
| variant                | Holds the board variant number, set automatically     |
| cpld_version           | Holds the CPLD version, set automatically             |
| rcw_filename           | Holds the current RCW filename, set automatically     |
| bootsource             | Holds the current boot source, set automatically      |

## Non-volatile Board Configuration Bits

The board has 16 configuration bits which are stored in the CPLD and are
non-volatile. These can be changed by the `sl28 nvm` command.

| Bit | Description                                                     |
| --- | --------------------------------------------------------------- |
|   0 | Power-on inhibit                                                |
|   1 | Enable eMMC boot                                                |
|   2 | Enable watchdog by default                                      |
|   3 | Disable failsafe watchdog by default                            |
|   4 | Clock generator selection bit 0                                 |
|   5 | Clock generator selection bit 1                                 |
|   6 | Disable CPU SerDes clock #2 and PCIe-A clock output             |
|   7 | Disable PCIe-B and PCIe-C clock output                          |
|   8 | Keep onboard PHYs in reset                                      |
|   9 | Keep USB hub in reset                                           |
|  10 | Keep eDP-to-LVDS converter in reset                             |
|  11 | Enable I2C stuck recovery on I2C PM and I2C GP busses           |
|  12 | reserved                                                        |
|  13 | reserved                                                        |
|  14 | Used by the RCW to determine boot source                        |
|  15 | Used by the RCW to determine boot source                        |

Please note, that if the board is in failsafe mode, the bits will have the
factory defaults, ie. all bits are off.

### Power-On Inhibit

If this is set, the board doesn't automatically turn on when power is
applied. Instead, the user has to either toggle the `PWR_BTN#` line or
use any other wake-up source such as RTC alarm or Wake-on-LAN.

### eMMC Boot

If this is set, the RCW will be fetched from the on-board eMMC at offset
1MiB. For further details, have a look at the [SMARC-sAL28 Reset
Configuration Word documentation][1].

### Watchdog

By default, the CPLD watchdog is enabled in failsafe mode. Using bits 2 and
3, the user can change its mode or disable it altogether.

| Bit 2 | Bit 3 | Description                     |
| ----- | ----- | ------------------------------- |
|     0 |     0 | Watchdog enabled, failsafe mode |
|     0 |     1 | Watchdog disabled               |
|     1 |     0 | Watchdog enabled, failsafe mode |
|     1 |     1 | Watchdog enabled, normal mode   |

### Clock Generator Select

### Clock Output Disable And Keep Devices In Reset

To safe power, the user might disable different devices and clock output of
the board.

[1]: https://github.com/kontron/rcw-smarc-sal28/blob/master/README.md
