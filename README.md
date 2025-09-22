# "Das U-Boot" for MediaTek MT6572
# Devices that this was tested with
* JTY D101
* ...

## Status
### What works
* Basic booting and relocation, poweroff, reset
* UART

### What doesn't
* Everything else

## Installation/running
### Flashing to the eMMC
1. Build U-Boot with `mt6572_defconfig`:
```
ARCH=arm CROSS_COMPILE=arm-none-eabi- make mt6572_defconfig
ARCH=arm CROSS_COMPILE=arm-none-eabi- make -j$(nproc)
```
2. Use `mtkclient` to flash `u-boot-mtk.bin` to the `UBOOT` partition:
```
mtk w uboot u-boot-mtk.bin
```

### Booting as DA
Instead of flashing U-Boot to the eMMC of your device, you can also boot it as DA using `mtkclient`.
1. Build U-Boot with `mt6572_da_defconfig`:
```
ARCH=arm CROSS_COMPILE=arm-none-eabi- make mt6572_da_defconfig
ARCH=arm CROSS_COMPILE=arm-none-eabi- make -j$(nproc)
```
2. Use `u-boot.bin` (without the LK header) with [this fork](https://github.com/mt6572-mainline/mtkclient) of `mtkclient`:
```
./mtk.py plstage --preloader u-boot.bin
```
