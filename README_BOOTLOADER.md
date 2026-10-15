# Boot-loader image note

- Paired boot loader: `..\RL78_F13_Boot_loader_CAN`
- App jump table: `0x2000-0x20FF`
- Image manifest: `0x2100-0x215F` (96-byte format 3, target `0xF13B`)
- Reserved manifest extension: `0x2160-0x21FF`
- App code start: `0x2200`
- CRC32: final four bytes of the aligned variable-length image, little-endian
- CAN update image:
  `DefaultBuild\RL78_F13_Boot_loader_CAN_app_update.bin`
- Current R5F10BMG layout: app start `0x2000`, capacity `0x1E000`, code flash
  `0x20000`; these are metadata values, not fixed PC-tool limits
- Protocol: `..\RL78_F13_Boot_loader_CAN\CAN_BOOT_PROTOCOL.md`
