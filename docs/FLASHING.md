# Flashing NetGuard release firmware

## Supported image

`release/netguard-v0.1.0-esp32s3-16mb.bin` is a merged full-flash image for an ESP32-S3 with 16 MB flash. It contains the bootloader, partition table, and application.

## Simple flashing-tool workflow

1. Download the release `.bin` from the GitHub Releases page.
2. Connect the ESP32-S3 using a data-capable USB cable.
3. In an ESP32 flashing tool, choose the serial port and this `.bin` file.
4. Set flash address to `0x0`, then flash.
5. Hold **BOOT** while starting if the board cannot enter download mode.
6. Reboot and join `NetGuard-Mashatir Setup` for provisioning.

Verify the setup network appears, test DNS directly with `nslookup`, then change router DHCP DNS only after a blocked and permitted domain both behave correctly.
