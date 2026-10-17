$INCLUDE "ftable.inc"

$INCLUDE "app_irq_table.inc"

; Fixed256-byte metadata region:128-byte core plus128 reserved0xFF bytes.
; The full region is covered by the application CRC. Code remains at offset200h.
.appmeta .CSEG AT APP_MANIFEST_TABLE
         .DB4   IMAGE_MANIFEST_MAGIC
         .DB4   IMAGE_MANIFEST_FORMAT_INFO
         .DB4   IMAGE_MANIFEST_PROTOCOL
         .DB4   IMAGE_MANIFEST_TARGET
         .DB4   APP_IMAGE_VERSION
         .DB4   APP_IMAGE_BUILD
         .DB4   IMAGE_SIZE_POST_BUILD
         .DB4   FLASH_TABLE
         .DB4   APP_REGION_SIZE
         .DB4   CODE_FLASH_SIZE
         .DB4   BOOT_RESERVED_SIZE
         .DB4   FLASH_ERASE_BLOCK_SIZE
         .DB4   CAN_NOMINAL_BITRATE_MASK
         .DB4   CAN_DATA_BITRATE_MASK
         .DB4   CAN_REQUEST_ID
         .DB4   CAN_RESPONSE_ID
         .DB4   MCU_PART_0
         .DB4   MCU_PART_1
         .DB4   MCU_PART_2
         .DB4   MCU_PART_3
         .DB4   MCU_FAMILY_0
         .DB4   MCU_FAMILY_1
         .DB4   MCU_FAMILY_2
         .DB4   MINIMUM_IMAGE_SIZE
; Common UPD1 policy: version3/size32/DEVICE_APP/CAN; version control at +0x74.
         .DB4   0x31445055
         .DB4   IMAGE_POLICY_WORD
         .DB4   IMAGE_IRQ_ABI
         .DB4   IMAGE_MIN_BOOT_VERSION
         .DB4   0x00000000
         .DB4   APP_IMAGE_VERSION_CONTROL
         .DB4   0x00000000
         .DB4   0x00000000
; Remaining128 bytes must stay erased.
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
         .DB4   0xFFFFFFFF
