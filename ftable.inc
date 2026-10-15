FLASH_TABLE        .EQU    0x2000
INTERRUPT_OFFSET   .EQU    0x100
APP_MANIFEST_TABLE .EQU    FLASH_TABLE+INTERRUPT_OFFSET

; Common CAN Upgrade v5 metadata. Keep it synchronized with can_boot_config.h.
IMAGE_MANIFEST_MAGIC       .EQU    0x334E4143 ; Little-endian bytes "CAN3"
IMAGE_MANIFEST_FORMAT_INFO .EQU    0x00038006 ; format6, core128/region256, common update policy
IMAGE_METADATA_REGION_SIZE .EQU    0x100
IMAGE_MANIFEST_PROTOCOL    .EQU    0x01050001 ; protocol ID 1, v5, CRC32
IMAGE_MANIFEST_TARGET      .EQU    0x0001F13B ; target 0xF13B, Classical CAN
APP_IMAGE_VERSION          .EQU    0x00010500 ; Semantic version1.5.0
APP_IMAGE_BUILD            .EQU    20261015
; Do not replace IMAGE_SIZE_POST_BUILD with a planned 60K/80K/110K size.
; generate_update_image.ps1 derives the actual image end from the linker map,
; patches this placeholder, and stores CRC32 in the final four BIN bytes.
IMAGE_SIZE_POST_BUILD      .EQU    0xFFFFFFFF ; Patched from the linker map
; APP_REGION_SIZE is capacity, not the required update BIN length.  FLASH_TABLE,
; capacity, flash/boot/erase geometry, CAN rates and IDs form one metadata
; contract with the boot project and must be changed and rebuilt together.
APP_REGION_SIZE            .EQU    0x0001E000
CODE_FLASH_SIZE            .EQU    0x00020000
BOOT_RESERVED_SIZE         .EQU    0x00002000
FLASH_ERASE_BLOCK_SIZE     .EQU    0x00000400
CAN_NOMINAL_BITRATE_MASK   .EQU    0x00000004 ; 500 kbit/s
CAN_DATA_BITRATE_MASK      .EQU    0x00000000
CAN_REQUEST_ID             .EQU    0x00000700
CAN_RESPONSE_ID            .EQU    0x00000701
MCU_PART_0                 .EQU    0x31463552 ; "R5F1"
MCU_PART_1                 .EQU    0x474D4230 ; "0BMG"
MCU_FAMILY_0               .EQU    0x38374C52 ; "RL78"
MCU_FAMILY_1               .EQU    0x3331462F ; "/F13"
MINIMUM_IMAGE_SIZE         .EQU    0x00000204
