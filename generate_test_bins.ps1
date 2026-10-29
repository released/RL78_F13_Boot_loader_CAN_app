[CmdletBinding()]
param(
    [string]$ProjectRoot = '',
    [string]$BootProject = '',
    [string]$InputBin = '',
    [int[]]$FlashBudgetKiB = @(16,32,64,128,256,512,1024),
    [switch]$Automatic,
    [switch]$Pause
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0

function Assert-TestBinPlainPath([string]$Path) {
    $cursor=[IO.Path]::GetFullPath($Path)
    while($cursor){
        if(Test-Path -LiteralPath $cursor){
            if(((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0){
                throw 'Test BIN paths must not traverse a junction or symbolic link.'
            }
        }
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
}

try {
    if(!$ProjectRoot){$ProjectRoot=$PSScriptRoot}
    $project=(Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
    Assert-TestBinPlainPath $project
    $stem=Split-Path -Leaf $project
    if(!$BootProject){$BootProject=$project}
    $boot=(Resolve-Path -LiteralPath $BootProject).Path
    $reader=Join-Path $boot 'app_config.ps1'
    if(!(Test-Path -LiteralPath $reader)){ $reader=Join-Path $boot 'tools\app_config.ps1' }
    if(!(Test-Path -LiteralPath $reader)){throw 'The project-local C-header reader app_config.ps1 is required.'}
    . $reader
    if(!$InputBin){$InputBin=Join-Path $project ('DefaultBuild\'+$stem+'_update.bin')}
    $source=(Resolve-Path -LiteralPath $InputBin).Path
    $outputRoot=Join-Path $project 'test_bins'
    Assert-TestBinPlainPath $source
    Assert-TestBinPlainPath $outputRoot
    $sourceHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $original=[IO.File]::ReadAllBytes($source)

    # .NET is part of Windows PowerShell; no MCU compiler/Python is invoked.
    if(-not ('DeviceFfTestBin' -as [type])){
        Add-Type -TypeDefinition @'
using System;
public static class DeviceFfTestBin {
    static void Need(bool ok,string reason) { if(!ok) throw new ArgumentException(reason); }
    public static uint Read32(byte[] b,int at) { return BitConverter.ToUInt32(b,at); }
    static void Put(byte[] b,int at,uint value) { Array.Copy(BitConverter.GetBytes(value),0,b,at,4); }
    public static uint Crc(byte[] b,int count) {
        uint crc=0xffffffffU;
        for(int i=0;i<count;i++) {
            crc^=b[i];
            for(int bit=0;bit<8;bit++) crc=(crc&1U)!=0U?(crc>>1)^0xedb88320U:crc>>1;
        }
        return crc^0xffffffffU;
    }
    static void Ascii(byte[] b,int at,int length) {
        bool ended=false,any=false;
        for(int i=at;i<at+length;i++) {
            if(b[i]==0) { ended=true;continue; }
            Need(!ended && b[i]>=32 && b[i]<=126,"Invalid MCU ASCII metadata.");any=true;
        }
        Need(any,"Missing MCU ASCII metadata.");
    }
    public static void Validate(byte[] b) {
        Need(b!=null && b.Length>=516 && (b.Length&3)==0,"Invalid aligned App BIN length.");
        bool can=Read32(b,0x100)==0x004e4143U;
        uint protocol=Read32(b,0x108);
        Need((can || Read32(b,0x100)==0x00505349U) && Read32(b,0x104)==0x00038006U &&
             (can?protocol==0x01050001U:protocol==0x01010055U || protocol==0x01010049U || protocol==0x01010058U),
             "Expected format6 CAN/UART/I2C/Xmodem CRC32 App BIN.");
        Need(BitConverter.ToUInt16(b,0x10c)!=0 && Read32(b,0x110)<=0xffffffU &&
             Read32(b,0x114)!=0xffffffffU,"Invalid App identity.");
        Need(Read32(b,0x118)==b.Length,"Metadata image_size differs from actual BIN.");
        uint start=Read32(b,0x11c),capacity=Read32(b,0x120),flash=Read32(b,0x124);
        uint boot=Read32(b,0x128),erase=Read32(b,0x12c),minimum=Read32(b,0x15c);
        Need(capacity>=b.Length && start<flash && (ulong)start+capacity<=flash && boot<=start &&
             erase!=0 && (erase&(erase-1))==0 && start%erase==0 && minimum>=516 && b.Length>=minimum,
             "Invalid App Flash layout or capacity.");
        Need(!can || ((b[0x10e]==1 || b[0x10e]==2) && b[0x10f]<=1 && Read32(b,0x130)!=0 &&
             (b[0x10e]!=2 || Read32(b,0x134)!=0)),"Invalid CAN mode or rate metadata.");
        uint idLimit=b[0x10f]==0?0x7ffU:0x1fffffffU;
        Need(!can || (Read32(b,0x138)<=idLimit && Read32(b,0x13c)<=idLimit &&
             Read32(b,0x138)!=Read32(b,0x13c)),"Invalid CAN IDs.");
        Ascii(b,0x140,16);Ascii(b,0x150,12);
        Need(Read32(b,0x160)==0x31445055U && b[0x164]==3 && b[0x165]==32 &&
             b[0x166]==1 && b[0x167]==(can?1:protocol==0x01010049U?2:4) &&
             Read32(b,0x168)!=0 && Read32(b,0x16c)<=0xffffffU && (can?Read32(b,0x170)==0:
             protocol==0x01010049U?Read32(b,0x170)==0 && (Read32(b,0x130)==100000 || Read32(b,0x130)==400000) &&
             Read32(b,0x134)>=8 && Read32(b,0x134)<=0x77 && Read32(b,0x138)==0 && Read32(b,0x13c)==0:
             Read32(b,0x170)>=300 && Read32(b,0x170)<=2000000 && Read32(b,0x130)==0 &&
             Read32(b,0x134)==0 && Read32(b,0x138)==0 && Read32(b,0x13c)==0) &&
             Read32(b,0x174)<=1 && Read32(b,0x178)==0 && Read32(b,0x17c)==0,
             "Invalid DEVICE_APP update policy or communication settings.");
        for(int i=0x180;i<0x200;i++) Need(b[i]==255,"Reserved metadata must be FF.");
        Need(Crc(b,b.Length-4)==Read32(b,b.Length-4),"Source/output CRC32 mismatch.");
        if(Read32(b,0x168)==0x31563852U) {
            Need(b.Length>=4100,"RH850 App requires its complete exception/IRQ tables.");
            Branch(b,0,start,start+4096,(ulong)start+(uint)b.Length-4);
            for(int at=0x200;at<0x400;at+=16) Branch(b,at,start+(uint)at,start+4096,(ulong)start+(uint)b.Length-4);
            for(int at=0x400;at<0xc00;at+=4) {
                uint destination=Read32(b,at);
                Need((destination&1)==0 && destination>=start+4096 && destination<(ulong)start+(uint)b.Length-4,
                     "Invalid RH850 App IRQ pointer.");
            }
            return;
        }
        for(int slot=0;slot<64;slot++) {
            int at=slot*4;
            if(Read32(b,at)==0xffffffffU && slot==1) continue;
            uint destination=Read32(b,at)>>8;
            Need(b[at]==0xec && destination>=(ulong)start+512 &&
                 destination<(ulong)start+(uint)b.Length-4,"Invalid App branch/vector destination.");
        }
    }
    static void Branch(byte[] b,int at,uint address,uint low,ulong end) {
        if(b[at]==0x1f && b[at+1]==0) { at+=2;address+=2; }
        uint destination=unchecked(address+Read32(b,at+2));
        Need(b[at]==0xe0 && b[at+1]==2 && (destination&1)==0 && destination>=low && destination<end,
             "Invalid RH850 App branch.");
    }
    public static byte[] Build(byte[] original,int length) {
        Validate(original);
        Need(length>=original.Length && (length&3)==0 && (uint)length<=Read32(original,0x120),
             "Test size would truncate the executable or exceed App capacity.");
        byte[] output=new byte[length];
        for(int i=0;i<length;i++) output[i]=255;
        Array.Copy(original,0,output,0,original.Length-4);
        Put(output,0x118,(uint)length);
        Put(output,length-4,Crc(output,length-4));
        Validate(output);
        for(int i=0;i<original.Length-4;i++) {
            if(i>=0x118 && i<0x11c) continue;
            Need(output[i]==original[i],"Source code/vector/metadata preservation failed.");
        }
        for(int i=original.Length-4;i<length-4;i++) Need(output[i]==255,"Padding is not FF.");
        return output;
    }
}
'@
    }
    [DeviceFfTestBin]::Validate($original)
    . (Join-Path $PSScriptRoot 'device_test_bins_output.ps1')
    $contractHeader=Join-Path $boot 'image_metadata_config.h'
    $contract=[IO.File]::ReadAllText($contractHeader)
    function Image-Literal([string]$Key) {
        $match=[regex]::Match($contract,'(?m)^#define\s+'+[regex]::Escape($Key)+'\s+(\(?(?:0[xX][0-9A-Fa-f]+|[0-9]+)[uUlL]*\)?)\s*(?:/\*.*)?$')
        if(!$match.Success){throw ('Missing literal project image contract: '+$Key)}
        return ConvertFrom-AppUnsignedLiteral $match.Groups[1].Value
    }
    foreach($field in @(@(0x100,'IMAGE_MANIFEST_MAGIC'),@(0x11c,'IMAGE_APPLICATION_START'),
        @(0x120,'IMAGE_APPLICATION_CAPACITY'),@(0x124,'IMAGE_CODE_FLASH_SIZE'),@(0x128,'IMAGE_RESERVED_SIZE'),
        @(0x12c,'IMAGE_ERASE_BLOCK_SIZE'),@(0x15c,'IMAGE_MINIMUM_IMAGE_SIZE'),@(0x168,'IMAGE_IRQ_ABI'),
        @(0x16c,'IMAGE_BOOT_VERSION'))) {
        if([DeviceFfTestBin]::Read32($original,$field[0]) -ne (Image-Literal $field[1])) {
            throw ('Source BIN differs from the selected project contract: '+$field[1])
        }
    }
    if([BitConverter]::ToUInt16($original,0x10c) -ne (Image-Literal IMAGE_TARGET_ID)){throw 'Source target differs from the selected project contract.'}
    $isCan=[DeviceFfTestBin]::Read32($original,0x100) -eq 0x004e4143
    $protocol=[DeviceFfTestBin]::Read32($original,0x108)
    if($isCan) {
        $canHeader=Join-Path (Split-Path -Parent $contractHeader) 'can_boot_config.h'
        $canText=[IO.File]::ReadAllText($canHeader)
        if([DeviceFfTestBin]::Read32($original,0x168) -eq 0x31563852) {
            [xml]$buildXml=[IO.File]::ReadAllText((Join-Path $project ($stem+'.mtpj')))
            $definitions=$buildXml.SelectSingleNode('//COptionD-0').InnerText
            $expectedMode=if($definitions -match '(?m)^RH850_CAN_FD\s*=\s*1\s*$'){2}else{1}
        } else {
            $mode=[regex]::Match($canText,'(?m)^#define\s+CAN_BOOT_CAN_MODE_(?:MASK|CLASSICAL)\s+(\([^\r\n)]+\))')
            if(!$mode.Success){throw 'Missing literal CAN mode in the selected project contract.'}
            $expectedMode=ConvertFrom-AppUnsignedLiteral $mode.Groups[1].Value
        }
        if($original[0x10e] -ne $expectedMode){throw 'Source CAN mode differs from this App project.'}
    } else {
        $setting=Join-Path $boot 'isp_transport_config.h'
        $text=[IO.File]::ReadAllText($setting)
        $match=[regex]::Match($text,'(?m)^#define\s+ISP_BOOT_PROTOCOL_ID\s+(\([^\r\n)]+\))')
        if(!$match.Success -or ($protocol -band 0xffff) -ne (ConvertFrom-AppUnsignedLiteral $match.Groups[1].Value)){
            throw 'Source ISP protocol differs from the selected project contract.'
        }
    }
    $transport=if($isCan){if($original[0x10e] -eq 2){'CAN FD'}else{'Classical CAN'}}
        elseif(($protocol -band 255) -eq 0x49){'I2C'}elseif(($protocol -band 255) -eq 0x58){'Xmodem'}else{'UART'}
    $app=Read-AppImageConfig $project ([BitConverter]::ToUInt16($original,0x10c)) ([uint32]$original[0x167])
    if($app.Version -ne [DeviceFfTestBin]::Read32($original,0x110) -or
       $app.Build -ne [DeviceFfTestBin]::Read32($original,0x114) -or
       $app.VersionControl -ne [DeviceFfTestBin]::Read32($original,0x174)){
        throw 'The source BIN differs from app_image_config.h; clean-build/package the App first.'
    }
    if(!$FlashBudgetKiB.Count -or @($FlashBudgetKiB | Where-Object {$_ -le 0 -or $_ -gt 16384}).Count){
        throw 'FlashBudgetKiB must contain positive sizes up to16384KiB.'
    }
    $bootBytes=[DeviceFfTestBin]::Read32($original,0x128)
    $capacity=[DeviceFfTestBin]::Read32($original,0x120)
    $flash=[DeviceFfTestBin]::Read32($original,0x124)
    $start=[DeviceFfTestBin]::Read32($original,0x11c)
    $jobs=@();$skipped=@()
    foreach($budget in @($FlashBudgetKiB | Sort-Object -Unique)){
        [long]$budgetBytes=[long]$budget*1024
        $reason=''
        if($budgetBytes -gt $flash){$reason='Exceeds physical Flash.'}
        elseif($budgetBytes -le $start){$reason='No App space after Boot.'}
        else{
            [int]$size=[Math]::Min(($budgetBytes-$start),$capacity)
            if($size -lt $original.Length){$reason='Existing App does not fit; executable is never truncated.'}
            elseif(($size -band 3) -ne 0){$reason='App size must be four-byte aligned.'}
        }
        if($reason){$skipped+=[pscustomobject]@{flash_budget_kib=$budget;reason=$reason};continue}
        if(@($jobs|Where-Object {$_.Size -eq $size}).Count){continue}
        $jobs+=[pscustomobject]@{Budget=$budget;Size=$size;Bytes=([DeviceFfTestBin]::Build($original,$size))}
    }
    if($Automatic -and !@($jobs|Where-Object {$_.Size -eq $capacity}).Count) {
        $jobs+=[pscustomobject]@{Budget=($flash/1024);Size=[int]$capacity;Bytes=([DeviceFfTestBin]::Build($original,[int]$capacity))}
    }
    if(!$jobs.Count){throw 'No requested size fits the source App and its declared Flash partition.'}
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $sourceHash){throw 'Source BIN changed during generation.'}

    $files=@{};$records=@()
    foreach($job in $jobs) {
        $name='{0}_test_FF_flash{1:000}KiB_app{2:000}KiB.bin' -f $stem,$job.Budget,($job.Size/1024)
        $files[$name]=[byte[]]$job.Bytes
        $records+=[pscustomobject][ordered]@{
            file=$name;flash_budget_kib=$job.Budget;app_kib=($job.Size/1024);input_bytes=$job.Size
            app_start=('0x{0:X8}' -f $start);app_end=('0x{0:X8}' -f ($start+$job.Size-1))
            crc32=('0x{0:X8}' -f [DeviceFfTestBin]::Read32($job.Bytes,$job.Size-4))
            sha256=(Get-TestBinHash $job.Bytes);source_bin=[IO.Path]::GetFileName($source)
            source_bytes=$original.Length;source_sha256=$sourceHash
            app_version=('0x{0:X8}' -f $app.Version);app_build=$app.Build;version_control=$app.VersionControl
            can_mode=$(if($isCan){$transport}else{''});transport=$transport;padding='0xFF';hardware='NOT RUN'
        }
    }
    $encoding=New-Object Text.UTF8Encoding($true)
    $files['manifest.csv']=$encoding.GetBytes((($records|ConvertTo-Csv -NoTypeInformation)-join [Environment]::NewLine))
    if($skipped.Count){$files['skipped.csv']=$encoding.GetBytes((($skipped|ConvertTo-Csv -NoTypeInformation)-join [Environment]::NewLine))}
    $note=@'
FF-filled App timing images; 1 KiB = 1024 bytes.
The packaged App executable, IRQ tables and metadata are preserved except image_size at BIN 0x118.
FF begins at the source CRC; the new final four bytes contain little-endian CRC32.
Names distinguish Boot+App Flash budget from actual App BIN length; protected tail is excluded.
Version, build, interface, IRQ ABI and policy remain unchanged. Normal Tool gates still apply.
Automatic builds maintain only test_bins/auto; manual batches remain independent.
Do not edit automatic outputs. Changed or unknown files stop replacement rather than being erased.
Use index.html or manifest.csv to select a BIN, then measure with the existing matching ISP workflow.
This generator does not update a board or measure programming speed.
'@
    $files['README.txt']=$encoding.GetBytes($note)
    $html=New-Object Text.StringBuilder
    [void]$html.Append('<!doctype html><html lang="en"><meta charset="utf-8"><title>App timing images</title><style>body{font:16px system-ui;margin:2em}table{border-collapse:collapse}td,th{border:1px solid #aaa;padding:.4em;overflow-wrap:anywhere}code{word-break:break-all}</style><h1>App timing images</h1><p>')
    [void]$html.Append([Net.WebUtility]::HtmlEncode($stem+' / '+$transport))
    [void]$html.Append('</p><p>Source SHA256: <code>'+[Net.WebUtility]::HtmlEncode($sourceHash)+'</code></p><p>FF-filled images for the matching ISP workflow. Actual update time comes from the Tool report.</p><table><tr><th>BIN</th><th>Bytes</th><th>CRC32</th><th>SHA256</th></tr>')
    foreach($row in $records) {
        [void]$html.Append('<tr><td><a href="'+[Net.WebUtility]::HtmlEncode($row.file)+'">'+[Net.WebUtility]::HtmlEncode($row.file)+'</a></td><td>'+$row.input_bytes+'</td><td>'+$row.crc32+'</td><td><code>'+$row.sha256+'</code></td></tr>')
    }
    [void]$html.Append('</table></html>')
    $files['index.html']=$encoding.GetBytes($html.ToString())
    if((Get-FileHash -LiteralPath $source).Hash -cne $sourceHash){throw 'Source BIN changed during generation.'}
    $batch=Publish-TestBinBatch $project $stem $files ([bool]$Automatic)
    Write-Host ('PASS: '+$records.Count+' FF-filled timing BINs; original BIN unchanged.')
    Write-Host ('Output: '+$batch)
    foreach($skip in $skipped){Write-Host ('SKIP '+$skip.flash_budget_kib+'KiB: '+$skip.reason)}
} catch {
    [Console]::Error.WriteLine($_.ToString())
    [Console]::Error.WriteLine($_.ScriptStackTrace)
    if($Pause){Read-Host 'Press Enter to close' | Out-Null}
    exit 1
}
if($Pause){Read-Host 'Press Enter to close' | Out-Null}
