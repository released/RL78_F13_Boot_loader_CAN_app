# RL78/F13 CAN boot-loader application

Boot-aware application for RL78/F13 R5F10BMG, paired with
`..\RL78_F13_Boot_loader_CAN`.

App health INFO1B reports APP_READY only after initialization, clock/CAN and
an observed timer IRQ. It also reports Boot handoff/update provenance and RESF.
Tool checks live identity/version/build/size after END; this is distinct from
the Boot's full-image CRC. See [confirmation and diagnostics](https://released.github.io/slide_isp_tool_for_RL78/#update-confirmation).

Start with the [RL78 Boot/App porting guide (zh-TW)](https://released.github.io/slide_boot_loader_for_RL78/#boot-app-porting)
for the complete startup/linker/IRQ/build/installation workflow.

## Public examples and checkout layout

- [CAN Transfer benchmark](https://github.com/released/RL78_F13_CAN_Transfer).
- [Required sibling Boot project](https://github.com/released/RL78_F13_Boot_loader_CAN).
- [This App project](https://github.com/released/RL78_F13_Boot_loader_CAN_app).

Clone Boot and App into the same parent directory using the exact repository
names. For Download ZIP, remove the `-main` directory suffix. This App references
Boot's board sources, CAN configuration and IRQ generator; the App repository
alone is not a self-contained build. Build Boot before App for initial combined
HEX generation. Compiler executable paths below are installation examples,
not links to project sources.

## App-only IRQ extension

User-editable inputs are app_image_config.h (version/build plus fixed target/role/
transport) and app_irq_handlers.h (vector/handler). The pre-build generator
produces ftable.inc, app_identity.h and app_irq_table.inc. Do not hand-edit the
generated files. Missing or invalid C configuration headers stop generation.
The local image_metadata_config.h forwards the paired Boot's IMAGE_* contract
and generated App identity. Shared image format/MCU/Flash/policy belong to that
Boot header; CAN settings belong to can_boot_config.h, and no-init RAM markers
to boot_app_handoff.h. An App release does not duplicate those shared values.
App INFO reads metadata fields through the independently compiled
image_metadata.c/.h. The same module supplies Boot validation, with project
values in image_metadata_profile.c; decoding alone does not validate CRC.
Follow the [step-by-step F13/F24 device workshop](https://released.github.io/slide_boot_loader_for_RL78/#device-metadata-vector-workshop).

Post-build rejects stale linked metadata or IRQ tables after a C header change.
Regenerate and clean rebuild this App; do not merely rerun the packager on old
HEX/map output. The App-only metadata/BR/RETI check does not need a Boot map.

Use app_irq_handlers.h to map physical vectors to globally visible normal
ISRs with #pragma interrupt and no vect=. Each normal ISR returns RETI.
Pre-build generates app_irq_table.inc and
app_identity.h (version/build from this app's app_image_config.h). Register new sources
in the app MTPJ, initialize/clear/mask correctly before EI, and build/update
only app for compatible additions. Boot assembly does not depend on this map.
Keep unused IRQs masked; default RETI does not acknowledge peripheral causes.
Different app projects must preserve the fixed MCU/partition/RAM/startup/
metadata/CRC/CAN INFO-ENTER contract; see the boot project's IRQ guides.

## Memory layout

| Range | Purpose |
| --- | --- |
| `0x0000-0x1FFF` | Reserved boot loader |
| `0x2000-0x20FF` | Relocated application jump table |
| `0x2100-0x21FF` | 256-byte metadata region (format6) |
| `0x2100-0x217F` | 128-byte defined core fields |
| `0x2180-0x21FF` | 128 reserved metadata bytes,0xFF |
| `0x2200-linked end` | Application code and linker data |
| aligned linked end | Optional `0xFF` alignment padding |
| final four image bytes | CRC32, little-endian |
| after image end to `0x1FBFF` | Outside the update image |
| `0x1FC00-0x1FFFF` | Protected final 1-KiB erase block |

**Important: the default App limit is 119 KiB. Using the full 120 KiB
requires matching Boot/App capacity settings and Boot installation with
RFP only, not CS+ Debug download.** CS+ places a debug monitor in the tail;
overwriting it can cause App startup/communication failures even when END
CRC passes. See [Boot installation and Flash tail protection](https://released.github.io/slide_boot_loader_for_RL78/#device-debug-monitor).

The application table uses `0x2000 + original_vector * 2`. The bootloader owns
the hardware vectors and dispatches application entries through this table.
TMRJ0 is a normal interrupt ISR with a no-vect pragma and RETI, like other
app IRQs. It is not an ordinary C callback. Boot dispatches by the owner
marker at `0xFF710` and the fixed app slot.

## Runtime identity and boot provenance

`APP_RUNTIME_LOG_TAG` in `custom_func.h` is a short operator-visible image tag.
The current image prints `log(timer)-V2:...`; change this tag when preparing a
deliberately distinguishable update test build.

The app also prints two startup results:

- `app launch: boot-validated` means the bootloader validated the image and
  wrote the `0xFF714` handoff signature immediately before this launch.
- `CAN update: completed` means the previous bootloader session completed the
  CAN programming operation and wrote the `0xFF718` signature before reset.

Both signatures are consumed and cleared before interrupts are enabled. A log
tag proves which app binary is executing; the one-shot signatures provide the
separate evidence for the current boot handoff and update path.

## CAN boot entry

The app initializes CAN at 500 kbit/s on P1.0/CTXD0 and P1.1/CRXD0, using P1.2
as the active-low transceiver enable. CAN handling is polling-only.

The shared `../RL78_F13_Boot_loader_CAN/f13_board_support.c/.h` now selects
the proven Transfer board baseline: X1 4 MHz /2 x16 -> PLL/CPU32, MDIV0,
CAN16, BRP register1 and 16 TQ -> Classical500K (75% sample point).
IC2 TJA1050T requires EVDD=4.75-5.25 V; P12 is S/silent-mode control, LOW
for normal TX. MCU operation at 3.3 V does not validate CAN power.

The preserved include section of generated `r_systeminit.c` redirects its
R_CGC_Create call to F13_BoardClockPrepare. The old generated HOCO body is
inactive; do not remove this hook when regenerating. X1/PLL waits run in
hardware_init after main, are bounded and preserve a valid inherited boot PLL.
CAN_AppInit resets the inherited operating controller before configuring
timing/filter RAM. Failed clock/CAN initialization leaves S HIGH and disables
request processing. The app prints clock status, CAN result/IDs and S level.
UART and TMRJ0 retain their existing CPU32 divisors and shared vectors.

The image uses format6 / CAN v5. Initially program the generated
`boot_app.hex` to avoid combining the new boot with a previous HOCO-only app.
For analyzer discovery send standard `0x700`, DLC2, `07 00`; a running app
responds on `0x701`, DLC8, INFO page0 with state=application. Tool workflow is
`CAN Upgrade`, not `Custom Transfer`. Flash and shared-vector layout are unchanged.

When the app receives standard ID `0x700` with INFO command `0x07`, it returns
the running application state, target/protocol, application version, and build
without resetting. ENTER command `0x06` replies on `0x701` with
`[0x06, 0x00]`, writes `0xAA55AA55` to RAM address `0xFF700`, and resets. The
bootloader consumes and clears the signature. UART0 command `z` remains
available as a local test path.

The app includes the paired boot project's authoritative `can_boot_config.h`,
and its RS-CAN Lite acceptance rule receives only standard data frames on the
configured request ID. Change CAN IDs in the boot project header, then rebuild
both projects.

See the [CAN boot protocol](https://github.com/released/RL78_F13_Boot_loader_CAN/blob/main/CAN_BOOT_PROTOCOL.md)
for the update protocol.
For reset/interrupt relocation and maintenance, read:

- [English guide](https://github.com/released/RL78_F13_Boot_loader_CAN/blob/main/INTERRUPT_VECTOR_GUIDE_EN.md)
- [繁體中文指南](https://github.com/released/RL78_F13_Boot_loader_CAN/blob/main/INTERRUPT_VECTOR_GUIDE_zh-TW.md)

Before a release, update the application semantic version and build fields in
`app_image_config.h`, not generated `ftable.inc`. Layout/identity declarations
come from the paired Boot's `image_metadata_config.h`, CAN parameters from
`can_boot_config.h`; the post-build script reads the same
assembly constants and does not keep a separate expected target or image-size
constant. APP_IMAGE_VERSION_CONTROL defaults to0;1 enables installed-App
same/newer checks on ordinary updates. Boot and App versions/builds are
independent. See the [version policy and maintenance operation](https://released.github.io/slide_isp_tool_for_RL78/#force-old-version).

CRC provides integrity, not authenticity. Version ordering and the public
maintenance confirmation are not cryptographic authentication. The PC validates
target, format, size, manifest and CRC before
START; the boot loader validates the manifest and the full application CRC on
every boot and remains in boot mode when validation fails. A partial update or
power loss therefore cannot jump into an incomplete application.

## Build and artifacts

```powershell
& 'C:\Program Files (x86)\Renesas Electronics\CS+\CC\CubeSuite+.exe' `
  /bb '.\RL78_F13_Boot_loader_CAN_app.mtpj'
```

The post-build script validates Intel HEX checksums and the manifest at
`0x2100`, reads the loaded-section end from the linker map, aligns the CRC
address to four bytes, patches the exact image length into the manifest, and
stores CRC32 in the final four update-image bytes. It intentionally does not
infer length from the last non-`0xFF` byte because valid data may end in
`0xFF`. It generates:

No project-size define is required for a 60-KiB, 80-KiB, or 110-KiB build.
`IMAGE_SIZE_POST_BUILD` remains a placeholder and `APP_REGION_SIZE` remains the
maximum capacity. Each successful build derives its own exact update-image
length from the linker map. For example, if the highest loaded application
byte causes the aligned image to end at 60 KiB, CRC32 is stored at bytes
`60 KiB - 4` through `60 KiB - 1`; a later 110-KiB build stores CRC32 at the
last four bytes of that 110-KiB image. Both builds must still fit the metadata
capacity and agree with the boot project's flash geometry.

| File | Size | Purpose |
| --- | ---: | --- |
| `DefaultBuild\RL78_F13_Boot_loader_CAN_app_update.bin` | Metadata-defined, 4-byte aligned | CAN update image |
| `DefaultBuild\flash_app_variable_CRC.hex` | Metadata-defined app range | CRC-protected app HEX |
| `DefaultBuild\app_build_report.html` | Offline HTML | Actual BIN metadata/CRC/SHA-256, byte addresses and full App vector table |
| `boot_app.hex` | 128 KiB address image | Initial boot + app programming |
| `boot_app.bin` | 131,072 | Combined binary image |

Git distribution retains only the update BIN in DefaultBuild. Build report,
map and app-only HEX files are generated locally by a clean build. The Tool's
[publication workflow](https://github.com/released/simple_renesas_isp_tool/blob/main/docs/GITHUB_PUBLISH_zh-TW.md)
archives other DefaultBuild files; root boot_app.hex/bin remain available for
initial installation.

The generator removes known obsolete fixed-range output names so an old
122,880-byte format-2 BIN or format3 BIN cannot be mistaken for the current format6 image.

## FF-filled App test images

After a successful App build, double-click `generate_test_bins.bat`, or run
`powershell -NoProfile -ExecutionPolicy Bypass -File .\generate_test_bins.ps1`
from this App directory. The matching sibling Boot supplies `app_config.ps1`;
no Python, SRecord or MCU compiler invocation is needed.

The script reads the packaged `DefaultBuild/*_update.bin` and creates a new
`test_bins/<batch>/` with test BINs, `manifest.csv` and `skipped.csv`.
Default Flash budgets are 16/32/64/128/256/512 KiB, including Boot; the actual
BIN subtracts the Boot reserve and respects metadata capacity. With the current
App, the generated lengths are 24/56/119 KiB; an App that no longer fits a
budget is skipped rather than truncated. One KiB is 1024 bytes.

App code and vectors remain unchanged. FF padding starts at the original CRC
location; metadata image_size at BIN `0x118..0x11B` is updated, and the new final
four bytes contain little-endian CRC32. Version/build/policy remain unchanged.
The original BIN is never overwritten. Test outputs are ignored by Git and
can be regenerated after each App build; both generator scripts are shared.
Use the existing Tool CAN Upgrade workflow and keep its JSON operation report.
See the [test-image guide](https://released.github.io/slide_boot_loader_for_RL78/#device-test-images).

## Debugging with E2 Lite

The app project runs to `_main` and uses `-OCDTR`; normal `.dataR/.bss`
allocation starts at `0xFE500` to avoid the RL78 trace/self RAM region at
`0xFE300-0xFE4FF`.

Starting debug directly from this app project is a standalone-app workflow.
The ABS file contains a temporary reset/vector section at `0x0000`, so its
object download temporarily replaces the boot-owned vectors. This is suitable
for app-only debugging but does not test the production boot handoff or shared
interrupt dispatch.

For integrated testing, program `boot_app.hex`, retain the boot region, load
the app symbols without downloading the app vector object, set a breakpoint at
app `main()`, and run from reset through the boot loader. See the paired
interrupt-vector guide's debug section and the porting guide for the procedure.

Combined images use
`..\RL78_F13_Boot_loader_CAN\DefaultBuild\boot0000_1FFF.hex`.

### F24 SRecord reference and the F13 implementation

The older F24 CRC sample uses CS+ post-build command `boot_flash.bat` and
SRecord response files. In particular, `2generateChecksum.cmd` crops the app
range and applies `-crc32-l-e`, while `3generateCRCHex.cmd` writes the CRC into
the final HEX. The F24 `app_HW_CRC` batch comments those two CRC-generation
steps out, so it is not the reference for an embedded post-build CRC.

The F13 project keeps the same useful concept but uses
`generate_update_image.ps1` instead of copying the batch chain. The PowerShell
implementation additionally rejects malformed Intel HEX, duplicate addresses,
wrong target/manifest, out-of-range content, map/HEX inconsistency and a missing
EOF record. Do not replace it with the older batch unless those checks and the
map-derived variable length are retained.

In CS+, the hook is under **CC-RL Build Tool > Common Options > Build Events >
Commands executed after build**. The project setting is stored as
`PostBuildCommands-0` in `RL78_F13_Boot_loader_CAN_app.mtpj`.

## Verification requirements

- Confirm `.jtext_AT2000` remains exactly at `0x2000-0x20FF`.
- Confirm `.appmeta_AT2100` remains exactly at `0x2100-0x21FF`.
- Confirm all normal app sections start at or after `0x2200`.
- Confirm update BIN length equals the manifest size, is 4-byte aligned, and is
  within the capacity declared by its metadata. For the current R5F10BMG
  build, that capacity is `0x1DC00` bytes; it is not a PC-tool global limit.
- Confirm generated CRC32 equals an independent calculation.
- Run `scripts\verify_f13_artifacts.ps1` from `simple_renesas_isp_tool` to
  verify boot growth margin, app boundaries, identity/version, manifest/CRC,
  and shared-vector mappings together.
- After running CS+ Code Generator, recheck exported relocated handlers and
  ensure TMRJ0 was not restored as a separate interrupt vector.

Physical CAN update, application execution, reset and power-loss cases still
require board testing.
