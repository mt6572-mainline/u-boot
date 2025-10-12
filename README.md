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
Instead of flashing U-Boot to the eMMC of your device, you can also boot it as the Download Agent (DA).
1. Build U-Boot with `mt6572_defconfig` and `mt6572_da.config` config fragment:
```
ARCH=arm CROSS_COMPILE=arm-none-eabi- make mt6572_defconfig mt6572_da.config
ARCH=arm CROSS_COMPILE=arm-none-eabi- make -j$(nproc)
```
2. Use [our Rust tool](https://github.com/mt6572-mainline/u-boot-da):
```
cargo r --release -- /path/to/u-boot.bin
```
