/* Project/API reading notes: Editable App release identity.
 * Version/build identify this App, target/role/transports must match the installed Boot contract,
 * and version control is disabled by default.
 * Generated identity/metadata outputs are rebuilt from these inputs; do not edit the BIN by hand.
 */
#ifndef APP_IMAGE_CONFIG_H
#define APP_IMAGE_CONFIG_H

/* User-editable image identity; clean rebuild App after any change.
 * Use integer literals, optional U/UL and one pair of parentheses.
 * Image format, MCU/Flash, IRQ ABI and minimum Boot: image_metadata_config.h.
 * CAN routing/rates: can_boot_config.h; RAM handoff: boot_app_handoff.h.
 * Generated ftable.inc/app_identity.h are outputs, not editing inputs.
 * Full field/address/build guide:
 * https://released.github.io/slide_boot_loader_for_RL78/#device-app-config
 */

/* Change for an App release: 0x00MMmmpp = major.minor.patch (one byte each).
 * Example: 0x00010601UL means 1.6.1; not decimal digits or a build date.
 * Independent of IMAGE_BOOT_VERSION: App 2.1.0 can use Boot 1.7.0.
 * Only major/minor/patch are compared for downgrade checks, not the build.
 * Valid range: 0x00000000..0x00FFFFFF. Use VERSION_CONTROL to select policy.
 */
#define APP_IMAGE_VERSION (0x00010601UL)

/* Change for build identification; YYYYMMDD is a convention, not a clock.
 * Example: 20261018UL identifies another build of the same App version.
 * Range: 0..4294967294; this is not compared with IMAGE_BOOT_BUILD.
 * A later build date does not make a lower App version a permitted upgrade.
 */
#define APP_IMAGE_BUILD (20261017UL)

/* Identity check, not MCU selection: keep equal to paired IMAGE_TARGET_ID.
 * Example: 0xF13BU selects this contract; 0xF33BU fails pre-build validation.
 * Changing a literal does not port the project to another MCU. A real port
 * also supplies the matching Boot, linker/Flash/board/IRQ and Tool contract.
 */
#define APP_IMAGE_TARGET_ID (0xF13BU)

/* Update destination role: 1U=DEVICE_APP, 2U=HOST_BRIDGE_APP in the protocol.
 * This project requires 1U. Setting 2U or undefined 5U fails pre-build;
 * a role mismatch is also rejected by Tool/Boot before any Flash erase.
 */
#define APP_IMAGE_ROLE (1U) /* DEVICE_APP */

/* Allowed update transport bitmask, not App runtime peripheral usage:
 * 0x01U=CAN, 0x02U=I2C, 0x04U=UART; protocol mask 0x03U means CAN|I2C.
 * This CAN project implements/requires 0x01U. Examples 0x02U, 0x04U, 0x03U,
 * zero or unknown bits fail pre-build; they do not enable another updater.
 * Additional transports require matching Boot/Tool implementations/contract.
 */
#define APP_IMAGE_UPDATE_TRANSPORTS (0x01U) /* CAN */

/* Installed-App version policy at metadata+0x74 (Flash 0x2174), LE32:
 * 0U=disabled (default): any version; 1U=enabled: same/newer App version only.
 * Example: installed 1.6.0/1U allows 1.6.0 and 1.7.0, rejects 1.5.0.
 * The incoming BIN's setting takes effect only AFTER a successful update;
 * setting a lower BIN to 0U cannot bypass the currently installed policy.
 * Tool maintenance action "force upgrade old version firmware" can explicitly
 * bypass only the version comparison for that one update, not CRC/identity.
 * No persistent floor: invalid-App recovery has no installed version policy.
 * Values other than 0U/1U fail generation. Clean rebuild after changing this.
 */
#define APP_IMAGE_VERSION_CONTROL (0U)

#endif
