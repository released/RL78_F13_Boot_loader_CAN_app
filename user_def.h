/******************************************************************************
*
* @file    $Name$
* @version $Revision$
* @date    $LastChangedDate$
*
* @Copyright 2015 - JIM.
*  This material is protected by copyright law. It is unlawful
*  to copy it.
*
* This document contains confidential information. It is not to be
* disclosed or used except in accordance with applicable contracts
* or agreements. This document must be rendered illegible when
* being discarded.
* 
* @remarks
* Modification History
* -------------------------
* See Version Control System History.
*
* @file This file contains the template class.
*
******************************************************************************/

#ifndef __USER_DEF_H__
#define __USER_DEF_H__

#include "can_boot_config.h"
#include "image_metadata_config.h"
#include "boot_app_handoff.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
/*                              Include File                                 */
/*****************************************************************************/


/*****************************************************************************/
/*                      Global Macro or Enum Definition	                     */
/*****************************************************************************/

#define bool                                    _Bool
#define FALSE	                                (0)
#define TRUE	                                (1)
#define false                                   (0)
#define true                                    (1)

#define BYTE0_Msk                               (0x000000FFUL)                   ///< Mask to get bit0~bit7 from a 32 bit integer
#define BYTE1_Msk                               (0x0000FF00UL)                   ///< Mask to get bit8~bit15 from a 32 bit integer
#define BYTE2_Msk                               (0x00FF0000UL)                   ///< Mask to get bit16~bit23 from a 32 bit integer
#define BYTE3_Msk                               (0xFF000000UL)                   ///< Mask to get bit24~bit31 from a 32 bit integer

#define GET_BYTE0(u32Param)    (((u32Param) & BYTE0_Msk)      ) /*!< Extract Byte 0 (Bit  0~ 7) from parameter u32Param */
#define GET_BYTE1(u32Param)    (((u32Param) & BYTE1_Msk) >>  8) /*!< Extract Byte 1 (Bit  8~15) from parameter u32Param */
#define GET_BYTE2(u32Param)    (((u32Param) & BYTE2_Msk) >> 16) /*!< Extract Byte 2 (Bit 16~23) from parameter u32Param */
#define GET_BYTE3(u32Param)    (((u32Param) & BYTE3_Msk) >> 24) /*!< Extract Byte 3 (Bit 24~31) from parameter u32Param */


#ifndef BIT0
#define BIT0                                    (0x00000001UL)       ///< Bit 0 mask of an 32 bit integer
#endif

#ifndef BIT1
#define BIT1                                    (0x00000002UL)       ///< Bit 1 mask of an 32 bit integer
#endif

#ifndef BIT2
#define BIT2                                    (0x00000004UL)       ///< Bit 2 mask of an 32 bit integer
#endif

#ifndef BIT3
#define BIT3                                    (0x00000008UL)       ///< Bit 3 mask of an 32 bit integer
#endif

#ifndef BIT4
#define BIT4                                    (0x00000010UL)       ///< Bit 4 mask of an 32 bit integer
#endif

#ifndef BIT5
#define BIT5                                    (0x00000020UL)       ///< Bit 5 mask of an 32 bit integer
#endif

#ifndef BIT6
#define BIT6                                    (0x00000040UL)       ///< Bit 6 mask of an 32 bit integer
#endif

#ifndef BIT7
#define BIT7                                    (0x00000080UL)       ///< Bit 7 mask of an 32 bit integer
#endif


/*
 * RL78/F13 R5F10BMG code-flash layout
 * ====================================
 *
 * Device capacity : 128 KiB, address 0x00000 through 0x1FFFF
 * Erase block     : 1 KiB (0x400 bytes)
 * Block numbers   : 0 through 127
 * FSL write unit  : 4 bytes
 *
 * Current allocation:
 *
 *   Address range       Blocks      Size       Owner
 *   ------------------  ----------  ---------  -----------------------------
 *   0x00000-0x01FFF     0-7         8 KiB      Boot loader and FSL
 *   0x02000-0x020FF     8           256 bytes  Application jump table
 *   0x02100-0x021FF     8           256 bytes  Metadata region (format6)
 *   0x02100-0x0217F     8           128 bytes  Core plus common update policy
 *   0x02180-0x021FF     8           128 bytes  Metadata extensions (0xFF)
 *   0x02200-actual end  8-126       Variable    Application code/padding
 *   final four bytes    8-126       4 bytes     Application CRC32
 *   after image end     8-126       Variable    Outside the image
 *   0x1FC00-0x1FFFF     127         1 KiB      Protected debug-monitor block
 *
 * FLASH_AREA is both the first application address and the first address that
 * the CAN updater may erase/write.  FLASH_TABLE is the relocated reset and
 * interrupt jump-table base.  The complete CAN update file covers FLASH_AREA
 * through its declared image end, including the four CRC bytes.  The maximum
 * update capacity is FLASH_UPDATE_MAX_SIZE, but the update file does not need
 * to fill that capacity.
 *
 * Variable-length update image rules
 * ----------------------------------
 * - The update BIN starts at APP_FLASH_START and is a multiple of four bytes.
 * - It contains the jump table, manifest, reserved extension area, application
 *   code/padding, and the CRC32 in its final four bytes.
 * - The manifest image-size field is the exact BIN length, not the capacity.
 * - The CRC covers every BIN byte except the final four stored-CRC bytes.
 * - APP_IMAGE_MIN_SIZE is the smallest structurally valid image.  Production
 *   applications normally exceed it because code starts at APP_CODE_START.
 * - FLASH_UPDATE_MAX_SIZE is the largest accepted BIN and is advertised by
 *   HELLO as capacity in KiB.  START carries the actual BIN length.
 * - Bytes after the declared image end are outside the validated application.
 *   They are neither executed by the boot handoff nor included in its CRC.
 *   The partially used final block is erased before it is rewritten; later
 *   complete blocks may retain older data but are unreachable by a valid image.
 *
 * IMPORTANT: the boot/application boundary must always be aligned to the
 * 1-KiB erase-block size.  Do not move only one define or only one linker
 * setting.  The boot loader, application, vector table, generated update image
 * and device metadata must all use the same boundary and capacity.  The actual
 * image size remains variable and is patched by the app post-build step.
 *
 * Changing the boot-loader allocation
 * -----------------------------------
 * Example: if boot grows from 8 KiB to 12 KiB, reserve blocks 0-11 and change
 * APP_FLASH_START from 0x02000 to 0x03000.  Then perform every step below.
 *
 * 1. In image_metadata_config.h, change IMAGE_APPLICATION_START,
 *    IMAGE_APPLICATION_CAPACITY, and IMAGE_RESERVED_SIZE as required.
 *    APP_FLASH_START, BLOCK_APP_BEGIN, and all image-size expressions below
 *    are derived from those authoritative values.
 *
 * 2. Boot project, CS+:
 *      Project Properties -> Build Tool -> Link Options -> Section -> Start
 *    Keep the boot sections starting at 0x000D8, rebuild, and verify in
 *    DefaultBuild/RL78_F13_Boot_loader_CAN.map that every boot/FSL section
 *    ends below APP_FLASH_START.
 *
 *    Also change:
 *      Build Tool -> Hex Output Options -> Divide output file
 *    from "boot0000_1FFF.hex=0000-1FFF" to the new inclusive boot range.
 *    For a 12-KiB boot area this becomes:
 *      boot0000_2FFF.hex=0000-2FFF
 *    Update generate_update_image.ps1 if the boot HEX filename changes.
 *
 * 3. Application project, CS+:
 *      Project Properties -> Build Tool -> Link Options -> Section -> Start
 *    Change the application section start currently shown as:
 *      .const,.text,...,.init_array/02200
 *    The code start is APP_FLASH_START + 0x200 because the first 0x100 bytes
 *    are the jump table and the next 0x100 bytes contain the manifest and
 *    reserved extension bytes.  For a 12-KiB boot area, use /03200.
 *
 *    Change FLASH_TABLE in ftable.inc in BOTH projects; the application
 *    ftable_RL78_F13.asm uses that constant to place the jump table and derives
 *    the manifest address from it. The fixed IRQ ABI currently requires
 *    APP_FLASH_START=0x2000. Repartitioning is a new boot ABI, not an app-only
 *    change. Regenerate f13_boot_vectors.asm; never add competing -VECTN
 *    options. All normal handlers, including INTTRJ0, are RETI ISRs.
 *
 *    Finally change:
 *      Build Tool -> Hex Output Options -> Divide output file
 *    so its lower address is the new APP_FLASH_START.
 *
 * 4. Update the application manifest values in app/ftable.inc and the CS+
 *    divided HEX input name/range used by generate_update_image.ps1.  Keep
 *    target/layout/format aligned with image_metadata_config.h and CAN
 *    settings with can_boot_config.h. Rebuild boot then app. Confirm the
 *    update BIN is no larger than:
 *      FLASH_UPDATE_MAX_SIZE
 *    and that its manifest size equals its actual length.  Independently
 *    confirm the CRC32 stored in the final four bytes.
 *
 * 5. Update CAN_BOOT_PROTOCOL.md.  The PC tool reads the start, capacity,
 *    actual size, MCU identity, CAN settings, and IDs from image/device
 *    metadata; do not add a target-size constant to the UI.  The F24 host
 *    forwards opaque CAN frames and does not contain the application layout.
 */
#define FLASH_BYTE_PER_BLOCK                    (1024UL)
/*
 * Board communication baseline is shared in f13_board_support.h/.c:
 * X1 4 MHz -> PLL32 -> CPU32/CAN16, Classical500K, UART115200, TMRJ0 1 ms.
 * Both projects must retain the startup hook and share this implementation.
 * QB-R5F10BMG-TB TJA1050T requires EVDD 4.75..5.25 V; P12 is its S pin.
 * Supply voltage is a hardware prerequisite, not image metadata or an MCU
 * register measurement. A valid checksum does not prove transceiver power.
 * CAN clock/timing changes must agree with manifest/INFO bitrate declarations.
 * FSL_Init still declares CPU32; never use the F24 flash library here.
 */
#define FLASH_BLOCK_SHIFT                       (10U)       /* 2^10 = 1024 */
#define FLASH_TOTAL_SIZE                        IMAGE_CODE_FLASH_SIZE
#define FLASH_TOTAL_BLOCKS                      (FLASH_TOTAL_SIZE / FLASH_BYTE_PER_BLOCK)
#define FLASH_END_ADDRESS                       (IMAGE_APPLICATION_START + IMAGE_APPLICATION_CAPACITY - 1UL)

#define APP_FLASH_START                         IMAGE_APPLICATION_START
#define APP_JUMP_TABLE_SIZE                     (0x00100UL)
#define APP_EXTENSION_TABLE_SIZE                (0x00100UL)
#define APP_MANIFEST_ADDRESS                    (APP_FLASH_START + IMAGE_MANIFEST_OFFSET)
#define APP_CODE_START                          (APP_FLASH_START + APP_JUMP_TABLE_SIZE + APP_EXTENSION_TABLE_SIZE)
#define APP_CRC_SIZE                            (4UL)
#define APP_IMAGE_MIN_SIZE                      IMAGE_MINIMUM_IMAGE_SIZE
#define FLASH_UPDATE_MAX_SIZE                   IMAGE_APPLICATION_CAPACITY

/*
 * Relocated application entries are four bytes wide while the physical RL78
 * vector table uses two-byte slots.  Pass the physical vector address, not an
 * interrupt ordinal, to APP_VECTOR_ENTRY_ADDRESS().
 */
#define APP_VECTOR_ENTRY_ADDRESS(vector_address) \
    (APP_FLASH_START + ((unsigned long)(vector_address) * 2UL))

#define BLOCK_APP_BEGIN                         (APP_FLASH_START / FLASH_BYTE_PER_BLOCK)
#define BLOCK_APP_END                           (FLASH_END_ADDRESS / FLASH_BYTE_PER_BLOCK)

/* Compatibility names used by the existing boot code and FSL wrapper. */
#define FLASH_END_BLOCK                         FLASH_TOTAL_BLOCKS
#define FLASH_AREA                              APP_FLASH_START
#define FLASH_TABLE                             APP_FLASH_START
#define SAMPLE_VALUE_U08_SHIFT_ADDR_TO_BLOCK_CF FLASH_BLOCK_SHIFT

#if ((APP_FLASH_START % FLASH_BYTE_PER_BLOCK) != 0UL)
#error APP_FLASH_START must be aligned to the 1-KiB FSL erase block.
#endif

#if ((APP_FLASH_START < FLASH_BYTE_PER_BLOCK) || \
     ((APP_FLASH_START + APP_IMAGE_MIN_SIZE) > FLASH_TOTAL_SIZE))
#error APP_FLASH_START leaves no valid application update area.
#endif

#if ((APP_FLASH_START + FLASH_UPDATE_MAX_SIZE) > FLASH_TOTAL_SIZE)
#error The configured application update region exceeds code flash.
#endif

#if ((FLASH_UPDATE_MAX_SIZE % FLASH_BYTE_PER_BLOCK) != 0UL)
#error The application capacity must preserve complete FSL erase blocks.
#endif

// #define TARGET_DEVICE_NOA                       ( 1 )       // Number of recordable areas (user area in code flash, data flash)

// #define TARGET_DEVICE_TYPE                      ( 0x02 )    // 0x02(RA2/RA4), 0x03(RA6)

// #define BOOT_FIRMWARE_MAJOR_VERSION             ( 1 )

// #define BOOT_FIRMWARE_MINOR_VERSION             ( 0 )

// #define MAX_CODE_FLASH_WRITE_DATA_SIZE          (256)

// #define MAX_FLASH_READ_DATA_SIZE                (256)

/*****************************************************************************/
/*                   Global Structure or Union Definition     		         */
/*****************************************************************************/


/*****************************************************************************/
/*                       Global Variable Declaration                         */
/*****************************************************************************/


/*****************************************************************************/
/*                         Configuration Checking                            */
/*****************************************************************************/


/*****************************************************************************/
/*                       Global Function Declaration                         */
/*****************************************************************************/

#endif /* __TEMPLATE_H */
/* EOF */
