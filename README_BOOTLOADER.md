# Boot-loader image note

- Paired boot loader: [RL78_F13_Boot_loader_CAN](https://github.com/released/RL78_F13_Boot_loader_CAN)
- App jump table: `0x2000-0x20FF`
- Image metadata region: `0x2100-0x21FF` (256 bytes, format6, target `0xF13B`)
- Defined core: `0x2100-0x217F` (128 bytes)
- Reserved metadata: `0x2180-0x21FF` (128 bytes, all `0xFF`)
- App code start: `0x2200`
- CRC32: final four bytes of the aligned variable-length image, little-endian
- CAN update image:
  `DefaultBuild\RL78_F13_Boot_loader_CAN_app_update.bin`
- Current R5F10BMG layout: app start `0x2000`, capacity `0x1E000`, code flash
  `0x20000`; these are metadata values, not fixed PC-tool limits
- Protocol: [CAN Boot protocol](https://github.com/released/RL78_F13_Boot_loader_CAN/blob/main/CAN_BOOT_PROTOCOL.md)
