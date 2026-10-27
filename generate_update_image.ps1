[CmdletBinding()]
param(
    [string]$InputHex = "DefaultBuild\flash2000_1FFFF.hex",
    [string]$MapFile = "DefaultBuild\RL78_F13_Boot_loader_CAN_app.map"
)

# The update image does not have a configured 32/60/80/110/120-KiB build size.
# The linker map is the source of the highest loaded application address.  The
# script aligns the following CRC address to four bytes, patches the resulting
# exact image length into the manifest, and stores CRC32 in those final four
# bytes.  APP_REGION_SIZE is only the maximum permitted capacity for this MCU
# and boot/app partition.  If that partition changes, update ftable.inc, the
# paired boot configuration, linker section addresses, and CS+ divided-output
# range together; never change only the PC tool or only this script.

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0
. (Join-Path $PSScriptRoot '..\RL78_F13_Boot_loader_CAN\app_config.ps1')
. (Join-Path $PSScriptRoot '..\RL78_F13_Boot_loader_CAN\app_build_report.ps1')

# Reject stale/tampered generated inputs before using any old linker output.
& (Join-Path $PSScriptRoot '..\RL78_F13_Boot_loader_CAN\generate_irq_contract.ps1') `
    -Mode App -AppProject $PSScriptRoot -CheckOnly

$tableConfigPath = Join-Path $PSScriptRoot 'ftable.inc'
$tableConfigText = [IO.File]::ReadAllText($tableConfigPath)
function Get-EquValue {
    param([Parameter(Mandatory = $true)][string]$Name)
    $match = [Regex]::Match(
        $tableConfigText,
        ('(?m)^\s*{0}\s+\.EQU\s+(0x[0-9A-Fa-f]+|[0-9]+)\s*(?:;.*)?$' -f [Regex]::Escape($Name)))
    if (-not $match.Success) {
        throw "Missing .EQU metadata value: $Name"
    }
    $raw = $match.Groups[1].Value
    if ($raw.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToInt64($raw.Substring(2), 16)
    }
    return [Convert]::ToInt64($raw, 10)
}

$AppStart = Get-EquValue 'FLASH_TABLE'
$MaximumAppLength = Get-EquValue 'APP_REGION_SIZE'
$FlashEndExclusive = Get-EquValue 'CODE_FLASH_SIZE'
$AppEndExclusive = $AppStart + $MaximumAppLength
$MinimumAppLength = Get-EquValue 'MINIMUM_IMAGE_SIZE'
$ManifestAddress = $AppStart + 0x100
$ExpectedManifestMagic = [uint32](Get-EquValue 'IMAGE_MANIFEST_MAGIC')
$ExpectedManifestFormat = 6
$ExpectedManifestLength = 128
$MetadataRegionSize = Get-EquValue 'IMAGE_METADATA_REGION_SIZE'
if ($MetadataRegionSize -ne 256) { throw 'Metadata Flash region must be256 bytes.' }
$ExpectedTargetId = (Get-EquValue 'IMAGE_MANIFEST_TARGET') -band 0xFFFF

function Read-IntelHex {
    param([Parameter(Mandatory = $true)][string]$Path)

    $memory = @{}
    [long]$baseAddress = 0

    foreach ($rawLine in [IO.File]::ReadAllLines($Path)) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0) { continue }
        if (($line[0] -ne ':') -or ($line.Length -lt 11)) {
            throw "Invalid Intel HEX record in ${Path}: $line"
        }

        $count = [Convert]::ToByte($line.Substring(1, 2), 16)
        $address = [Convert]::ToUInt16($line.Substring(3, 4), 16)
        $type = [Convert]::ToByte($line.Substring(7, 2), 16)
        if ($line.Length -ne (11 + (2 * $count))) {
            throw "Invalid Intel HEX record length in ${Path}: $line"
        }

        [int]$sum = $count + (($address -shr 8) -band 0xFF) + ($address -band 0xFF) + $type
        $data = New-Object byte[] $count
        for ($i = 0; $i -lt $count; $i++) {
            $data[$i] = [Convert]::ToByte($line.Substring(9 + (2 * $i), 2), 16)
            $sum += $data[$i]
        }
        $recordChecksum = [Convert]::ToByte($line.Substring(9 + (2 * $count), 2), 16)
        if ((($sum + $recordChecksum) -band 0xFF) -ne 0) {
            throw "Intel HEX checksum error in ${Path}: $line"
        }

        switch ($type) {
            0 {
                for ($i = 0; $i -lt $count; $i++) {
                    $memory[$baseAddress + $address + $i] = $data[$i]
                }
            }
            1 { return $memory }
            2 {
                $baseAddress = ([long]((([int]$data[0]) -shl 8) -bor $data[1])) -shl 4
            }
            4 {
                $baseAddress = ([long]((([int]$data[0]) -shl 8) -bor $data[1])) -shl 16
            }
            default { }
        }
    }

    return $memory
}

function Write-IntelHexRecord {
    param(
        [Parameter(Mandatory = $true)][IO.StreamWriter]$Writer,
        [Parameter(Mandatory = $true)][byte]$Type,
        [Parameter(Mandatory = $true)][uint16]$Address,
        [Parameter(Mandatory = $true)][AllowEmptyCollection()][byte[]]$Data
    )

    [int]$sum = $Data.Length + (($Address -shr 8) -band 0xFF) + ($Address -band 0xFF) + $Type
    $dataText = New-Object Text.StringBuilder
    foreach ($value in $Data) {
        [void]$dataText.AppendFormat("{0:X2}", $value)
        $sum += $value
    }
    $checksum = (-$sum) -band 0xFF
    $Writer.WriteLine(":{0:X2}{1:X4}{2:X2}{3}{4:X2}", $Data.Length, $Address, $Type, $dataText.ToString(), $checksum)
}

function Write-IntelHex {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$StartAddress,
        [Parameter(Mandatory = $true)][byte[]]$Data
    )

    $writer = New-Object IO.StreamWriter($Path, $false, [Text.Encoding]::ASCII)
    try {
        $offset = 0
        $activeUpper = -1
        while ($offset -lt $Data.Length) {
            $absolute = $StartAddress + $offset
            $upper = ($absolute -shr 16) -band 0xFFFF
            if ($upper -ne $activeUpper) {
                [byte[]]$upperData = @([byte](($upper -shr 8) -band 0xFF), [byte]($upper -band 0xFF))
                Write-IntelHexRecord -Writer $writer -Type 4 -Address 0 -Data $upperData
                $activeUpper = $upper
            }

            $address = $absolute -band 0xFFFF
            $recordLength = [Math]::Min(16, $Data.Length - $offset)
            $recordLength = [Math]::Min($recordLength, 0x10000 - $address)
            $record = New-Object byte[] $recordLength
            [Array]::Copy($Data, $offset, $record, 0, $recordLength)
            Write-IntelHexRecord -Writer $writer -Type 0 -Address ([uint16]$address) -Data $record
            $offset += $recordLength
        }
        Write-IntelHexRecord -Writer $writer -Type 1 -Address 0 -Data ([byte[]]@())
    }
    finally {
        $writer.Dispose()
    }
}

function Get-Crc32 {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Data,
        [Parameter(Mandatory = $true)][int]$Count
    )

    [uint64]$crc = 4294967295
    for ($i = 0; $i -lt $Count; $i++) {
        $crc = ($crc -bxor $Data[$i]) -band 4294967295
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = (($crc -shr 1) -bxor 3988292384) -band 4294967295
            }
            else {
                $crc = ($crc -shr 1) -band 4294967295
            }
        }
    }
    return [uint32](($crc -bxor 4294967295) -band 4294967295)
}

function Get-ApplicationLinkedEnd {
    param([Parameter(Mandatory = $true)][string]$Path)

    $loadedSectionEndSymbols = @(
        ('jtext_AT{0:X}' -f $AppStart),
        ('appmeta_AT{0:X}' -f $ManifestAddress),
        "const",
        "text",
        "RLIB",
        "SLIB",
        "textf",
        "constf",
        "data",
        "sdata",
        "init_array"
    )
    $mapText = [IO.File]::ReadAllText($Path)
    [long]$linkedEnd = 0
    foreach ($symbol in $loadedSectionEndSymbols) {
        $escaped = [Regex]::Escape($symbol)
        $pattern = "(?m)^\s*__e\.${escaped}\s*\r?\n\s*([0-9A-Fa-f]{8})\b"
        $matches = [Regex]::Matches($mapText, $pattern)
        foreach ($match in $matches) {
            $value = [Convert]::ToInt64($match.Groups[1].Value, 16)
            if ($value -gt $linkedEnd) {
                $linkedEnd = $value
            }
        }
    }
    if ($linkedEnd -eq 0) {
        throw "No application load-section end symbols were found in map file: $Path"
    }
    return $linkedEnd
}

function Write-U32LittleEndian {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Data,
        [Parameter(Mandatory = $true)][int]$Offset,
        [Parameter(Mandatory = $true)][uint32]$Value
    )

    $Data[$Offset] = [byte]($Value -band 0xFF)
    $Data[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Data[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Data[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

$projectDirectory = $PSScriptRoot
$inputPath = Join-Path $projectDirectory $InputHex
if (-not (Test-Path -LiteralPath $inputPath)) {
    throw "Application HEX file not found: $inputPath"
}
$mapPath = Join-Path $projectDirectory $MapFile
if (-not (Test-Path -LiteralPath $mapPath)) {
    throw "Application map file not found: $mapPath"
}

$linkedEnd = Get-ApplicationLinkedEnd -Path $mapPath
if (($linkedEnd -lt ($AppStart + $MinimumAppLength - 4)) -or
    ($linkedEnd -gt ($AppEndExclusive - 4))) {
    throw ("Application linked end 0x{0:X5} is outside the supported range." -f $linkedEnd)
}
$CrcAddress = [long](($linkedEnd + 3) -band -4)
$AppLength = $CrcAddress + 4 - $AppStart
if (($AppLength -lt $MinimumAppLength) -or
    ($AppLength -gt $MaximumAppLength) -or
    (($AppLength -band 3) -ne 0)) {
    throw ("Derived image length 0x{0:X8} is invalid; supported range is 0x{1:X8}-0x{2:X8}." -f `
        $AppLength, $MinimumAppLength, $MaximumAppLength)
}

$outputDirectory = Split-Path -Parent $inputPath
$appMemory = Read-IntelHex -Path $inputPath
Write-Verbose ("Parsed {0} application HEX bytes; byte[0x2000] = 0x{1:X2}" -f $appMemory.Count, $appMemory[[long]$AppStart])
$appImage = New-Object byte[] $AppLength
for ($i = 0; $i -lt $appImage.Length; $i++) { $appImage[$i] = 0xFF }

foreach ($entry in $appMemory.GetEnumerator()) {
    $address = [long]$entry.Key
    if (($address -ge $AppStart) -and ($address -lt $CrcAddress)) {
        $appImage[$address - $AppStart] = [byte]$entry.Value
    }
    elseif (($address -ge $CrcAddress) -and
            ($address -lt $FlashEndExclusive) -and
            ([byte]$entry.Value -ne 0xFF)) {
        throw ("Non-blank application byte at 0x{0:X5} is beyond map-derived linked end 0x{1:X5}." -f `
            $address, $linkedEnd)
    }
}

if (($appImage[0] -eq 0xFF) -and ($appImage[1] -eq 0xFF) -and
    ($appImage[2] -eq 0xFF) -and ($appImage[3] -eq 0xFF)) {
    throw ("The application jump table is missing at address 0x{0:X}." -f $AppStart)
}

$manifestOffset = $ManifestAddress - $AppStart
for ($i=$ExpectedManifestLength; $i -lt $MetadataRegionSize; $i++) {
    if ($appImage[$manifestOffset+$i] -ne 255) { throw 'Reserved metadata extensions must be0xFF.' }
}
$manifestMagic = [BitConverter]::ToUInt32($appImage, $manifestOffset)
$manifestFormat = $appImage[$manifestOffset + 4]
$manifestLength = $appImage[$manifestOffset + 5]
$manifestProtocolId = [BitConverter]::ToUInt16($appImage, $manifestOffset + 8)
$manifestProtocolVersion = $appImage[$manifestOffset + 10]
$manifestChecksumType = $appImage[$manifestOffset + 11]
$manifestTargetId = [BitConverter]::ToUInt16($appImage, $manifestOffset + 12)
$manifestVersion = [BitConverter]::ToUInt32($appImage, $manifestOffset + 16)
$manifestBuild = [BitConverter]::ToUInt32($appImage, $manifestOffset + 20)
$manifestImageSize = [BitConverter]::ToUInt32($appImage, $manifestOffset + 24)
$manifestAppStart = [BitConverter]::ToUInt32($appImage, $manifestOffset + 28)
$manifestCapacity = [BitConverter]::ToUInt32($appImage, $manifestOffset + 32)
$manifestCodeFlash = [BitConverter]::ToUInt32($appImage, $manifestOffset + 36)
$manifestMinimumSize = [BitConverter]::ToUInt32($appImage, $manifestOffset + 92)
if ([BitConverter]::ToUInt32($appImage,$manifestOffset+96) -ne 0x31445055 -or
    [BitConverter]::ToUInt32($appImage,$manifestOffset+100) -ne (Get-EquValue 'IMAGE_POLICY_WORD') -or
    [BitConverter]::ToUInt32($appImage,$manifestOffset+104) -ne (Get-EquValue 'IMAGE_IRQ_ABI') -or
    [BitConverter]::ToUInt32($appImage,$manifestOffset+108) -ne (Get-EquValue 'IMAGE_MIN_BOOT_VERSION')) {
    throw 'Image policy must target DEVICE_APP via CAN with the fixed F13 IRQ ABI.'
}
if ([BitConverter]::ToUInt32($appImage,$manifestOffset+116) -ne (Get-EquValue 'APP_IMAGE_VERSION_CONTROL')) {
    throw 'Compiled version control differs from App header. Clean rebuild.'
}
for ($i=112; $i -lt 128; $i++) {
    if ($i -ge 116 -and $i -lt 120) { continue }
    if ($appImage[$manifestOffset+$i] -ne 0) { throw 'Unknown policy extension.' }
}

if ($manifestMagic -ne $ExpectedManifestMagic) {
    throw ("Invalid image manifest magic at 0x{0:X}: 0x{1:X8}" -f `
        $ManifestAddress, $manifestMagic)
}
if (($manifestFormat -ne $ExpectedManifestFormat) -or
    ($manifestLength -ne $ExpectedManifestLength)) {
    throw ("Unsupported image manifest format {0}, length {1}." -f $manifestFormat, $manifestLength)
}
if (($manifestProtocolId -ne 1) -or ($manifestProtocolVersion -ne 5) -or
    ($manifestChecksumType -ne 1)) {
    throw ("Unsupported update protocol/checksum metadata: id={0}, version={1}, checksum={2}." -f `
        $manifestProtocolId, $manifestProtocolVersion, $manifestChecksumType)
}
if ($manifestTargetId -ne $ExpectedTargetId) {
    throw ("Wrong image target 0x{0:X4}; expected 0x{1:X4}." -f $manifestTargetId, $ExpectedTargetId)
}
if (($manifestAppStart -ne $AppStart) -or
    ($manifestCapacity -ne $MaximumAppLength) -or
    ($manifestCodeFlash -ne $FlashEndExclusive) -or
    ($manifestMinimumSize -ne $MinimumAppLength)) {
    throw "Manifest flash-layout metadata does not match ftable.inc."
}
if (($manifestImageSize -ne [uint32]::MaxValue) -and
    ($manifestImageSize -ne $AppLength)) {
    throw ("Wrong manifest image size 0x{0:X8}; expected placeholder 0xFFFFFFFF or 0x{1:X8}." -f `
        $manifestImageSize, $AppLength)
}
if ($manifestVersion -eq [uint32]::MaxValue) {
    throw "The application image version is not initialized."
}
if ($manifestVersion -ne (Get-EquValue 'APP_IMAGE_VERSION') -or
    $manifestBuild -ne (Get-EquValue 'APP_IMAGE_BUILD')) {
    throw 'Compiled App metadata is stale relative to app_image_config.h. Clean rebuild the App.'
}

# Resolve the compiled table against this App's own header and linker symbols.
# This guard needs only the App map; no rebuilt Boot map is required.
$symbols = @{}
$mapText = [IO.File]::ReadAllText($mapPath)
foreach ($match in [regex]::Matches($mapText,
    '(?m)^  (\S+)\s*\r?\n\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]+)\s+(?:func|none|entry)\b')) {
    $symbols[$match.Groups[1].Value] = @(
        [Convert]::ToUInt32($match.Groups[2].Value,16),
        [Convert]::ToUInt32($match.Groups[3].Value,16))
}
$handlers = Read-AppIrqConfig $PSScriptRoot
$irqMap = @{}
foreach ($vector in $handlers.Keys) {
    $irqMap[$vector] = '_'+[string]$handlers[$vector]
}
for ($vector=0; $vector -lt 128; $vector+=2) {
    $offset = $vector*2
    if ($vector -eq 2) {
        if ([BitConverter]::ToUInt32($appImage,$offset) -ne [uint32]::MaxValue) {
            throw 'Reserved App vector slot 02 must stay FF.'
        }
        continue
    }
    $symbol = if ($vector -eq 0) {'_start'} elseif ($vector -eq 126) {'f13_app_default_brk'} `
        elseif ($irqMap.ContainsKey($vector)) {$irqMap[$vector]} else {'f13_app_default_isr'}
    if (!$symbols.ContainsKey($symbol)) { throw "Missing compiled App symbol: $symbol" }
    $address = [uint32]$symbols[$symbol][0]
    $branch = [uint32]$appImage[$offset+1] -bor ([uint32]$appImage[$offset+2] -shl 8) -bor ([uint32]$appImage[$offset+3] -shl 16)
    if ($appImage[$offset] -ne 0xEC -or $branch -ne $address -or
        $address -lt $AppStart+0x200 -or $address -ge $CrcAddress) {
        throw "Compiled App IRQ table is stale or invalid at vector $vector. Clean rebuild the App."
    }
    if ($irqMap.ContainsKey($vector)) {
        $size = [uint32]$symbols[$symbol][1]
        $last = $address-$AppStart+$size-2
        if ($size -lt 2 -or $last+1 -ge $appImage.Length -or
            $appImage[$last] -ne 0x61 -or $appImage[$last+1] -ne 0xFC) {
            throw "Compiled App handler must end in RETI: $symbol"
        }
    }
}

$crcOffset = $appImage.Length - 4
Write-U32LittleEndian -Data $appImage -Offset ($manifestOffset + 24) `
    -Value ([uint32]$AppLength)
$crc = Get-Crc32 -Data $appImage -Count $crcOffset
Write-U32LittleEndian -Data $appImage -Offset $crcOffset -Value $crc

$updateBinPath = Join-Path $outputDirectory "RL78_F13_Boot_loader_CAN_app_update.bin"
$updateHexPath = Join-Path $outputDirectory "flash_app_variable_CRC.hex"
$obsoleteOutputPaths = @(
    (Join-Path $outputDirectory "flash2000_1FFFF_CRC.hex"),
    (Join-Path $outputDirectory "flash2000_variable_CRC.hex"),
    (Join-Path $outputDirectory "RL78_F13_Boot_loader_app_update.bin")
)
foreach ($obsoleteOutputPath in $obsoleteOutputPaths) {
    if (Test-Path -LiteralPath $obsoleteOutputPath) {
        Remove-Item -LiteralPath $obsoleteOutputPath -Force
    }
}
[IO.File]::WriteAllBytes($updateBinPath, $appImage)
Write-IntelHex -Path $updateHexPath -StartAddress $AppStart -Data $appImage

$bootHexPath = Join-Path $projectDirectory "..\RL78_F13_Boot_loader_CAN\DefaultBuild\boot0000_1FFF.hex"
if (Test-Path -LiteralPath $bootHexPath) {
    # Programmer image stops before the protected final erase block.
    $combined = New-Object byte[] $AppEndExclusive
    for ($i = 0; $i -lt $combined.Length; $i++) { $combined[$i] = 0xFF }
    $bootMemory = Read-IntelHex -Path $bootHexPath
    foreach ($entry in $bootMemory.GetEnumerator()) {
        $address = [long]$entry.Key
        if (($address -ge 0) -and ($address -lt $AppStart)) {
            $combined[$address] = [byte]$entry.Value
        }
    }
    [Array]::Copy($appImage, 0, $combined, $AppStart, $appImage.Length)
    [IO.File]::WriteAllBytes((Join-Path $projectDirectory "boot_app.bin"), $combined)
    Write-IntelHex -Path (Join-Path $projectDirectory "boot_app.hex") -StartAddress 0 -Data $combined
}

Write-AppBuildReport -Project $PSScriptRoot -ImagePath $updateBinPath

Write-Host ("Generated {0} bytes; linked end = 0x{1:X5}; CRC address = 0x{2:X5}; target = 0x{3:X4}; version = 0x{4:X8}; build = {5}; app start = 0x{6:X}; capacity = 0x{7:X}; CRC32 = 0x{8:X8}" -f `
    $appImage.Length, $linkedEnd, $CrcAddress, $manifestTargetId,
    $manifestVersion, $manifestBuild, $manifestAppStart, $manifestCapacity, $crc)

# APP_TIMING_BINS_BEGIN
$LASTEXITCODE=0
& (Join-Path $PSScriptRoot 'generate_test_bins.ps1') -ProjectRoot $PSScriptRoot -Automatic
if($LASTEXITCODE -ne 0){throw 'App timing BIN generation failed.'}
# APP_TIMING_BINS_END
