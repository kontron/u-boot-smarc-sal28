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
is also a failsafe image of it. See "Flash Map" below.

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

## SPI flash memory map

| Start      | End        | Size     | WP |Contents                            |
| ---------- | ---------- | --------:| -- | -------------------------------    |
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
update by the user.

## Configuration Store

The board can be configured with different reset configuration words. To
make it easy for the user to switch between them, there is an extra
partition where all factory provided RCWs are stored. At the beginning of
this partition, there is a script which can be run by the user to list and
install the RCWs. The u-boot provides a handy environment variable
`install_rcw` which can be executed with `run install_rcw`.
