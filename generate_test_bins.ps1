[CmdletBinding()]
param(
    [string]$ProjectRoot = '',
    [string]$BootProject = '',
    [string]$InputBin = '',
    [int[]]$FlashBudgetKiB = @(16,32,64,128,256,512),
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
    $stem=Split-Path -Leaf $project
    if(!$BootProject){
        if(!$stem.EndsWith('_app',[StringComparison]::OrdinalIgnoreCase)){
            throw 'Specify -BootProject for an App folder without the _app suffix.'
        }
        $BootProject=Join-Path (Split-Path -Parent $project) $stem.Substring(0,$stem.Length-4)
    }
    $boot=(Resolve-Path -LiteralPath $BootProject).Path
    $reader=Join-Path $boot 'app_config.ps1'
    if(!(Test-Path -LiteralPath $reader)){ $reader=Join-Path $boot 'tools\app_config.ps1' }
    if(!(Test-Path -LiteralPath $reader)){throw 'The paired Boot C-header reader app_config.ps1 is required.'}
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
        Need(Read32(b,0x100)==0x004e4143U && Read32(b,0x104)==0x00038006U &&
             Read32(b,0x108)==0x01050001U,"Expected format6 CAN protocol5 CRC32 App BIN.");
        Need(BitConverter.ToUInt16(b,0x10c)!=0 && Read32(b,0x110)<=0xffffffU &&
             Read32(b,0x114)!=0xffffffffU,"Invalid App identity.");
        Need(Read32(b,0x118)==b.Length,"Metadata image_size differs from actual BIN.");
        uint start=Read32(b,0x11c),capacity=Read32(b,0x120),flash=Read32(b,0x124);
        uint boot=Read32(b,0x128),erase=Read32(b,0x12c),minimum=Read32(b,0x15c);
        Need(capacity>=b.Length && start<flash && (ulong)start+capacity<=flash && boot<=start &&
             erase!=0 && (erase&(erase-1))==0 && start%erase==0 && minimum>=516 && b.Length>=minimum,
             "Invalid App Flash layout or capacity.");
        Need((b[0x10e]==1 || b[0x10e]==2) && b[0x10f]<=1 && Read32(b,0x130)!=0 &&
             (b[0x10e]!=2 || Read32(b,0x134)!=0),"Invalid CAN mode or rate metadata.");
        uint idLimit=b[0x10f]==0?0x7ffU:0x1fffffffU;
        Need(Read32(b,0x138)<=idLimit && Read32(b,0x13c)<=idLimit &&
             Read32(b,0x138)!=Read32(b,0x13c),"Invalid CAN IDs.");
        Ascii(b,0x140,16);Ascii(b,0x150,12);
        Need(Read32(b,0x160)==0x31445055U && b[0x164]==3 && b[0x165]==32 &&
             b[0x166]==1 && b[0x167]!=0 && (b[0x167]&~7)==0 && (b[0x167]&1)!=0 &&
             Read32(b,0x168)!=0 && Read32(b,0x16c)<=0xffffffU && Read32(b,0x170)==0 &&
             Read32(b,0x174)<=1 && Read32(b,0x178)==0 && Read32(b,0x17c)==0,
             "Invalid CAN DEVICE_APP update policy.");
        for(int i=0x180;i<0x200;i++) Need(b[i]==255,"Reserved metadata must be FF.");
        Need(Crc(b,b.Length-4)==Read32(b,b.Length-4),"Source/output CRC32 mismatch.");
        for(int slot=0;slot<64;slot++) {
            int at=slot*4;
            if(slot==1 && BitConverter.ToUInt16(b,0x10c)==0xf13b && Read32(b,at)==0xffffffffU) continue;
            uint destination=Read32(b,at)>>8;
            Need(b[at]==0xec && destination>=(ulong)start+512 &&
                 destination<(ulong)start+(uint)b.Length-4,"Invalid App branch/vector destination.");
        }
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
    $app=Read-AppImageConfig $project ([BitConverter]::ToUInt16($original,0x10c))
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
        elseif($budgetBytes -le $bootBytes){$reason='No App space after Boot.'}
        else{
            [int]$size=[Math]::Min(($budgetBytes-$bootBytes),$capacity)
            if($size -lt $original.Length){$reason='Existing App does not fit; executable is never truncated.'}
            elseif(($size -band 3) -ne 0){$reason='App size must be four-byte aligned.'}
        }
        if($reason){$skipped+=[pscustomobject]@{flash_budget_kib=$budget;reason=$reason};continue}
        $jobs+=[pscustomobject]@{Budget=$budget;Size=$size;Bytes=([DeviceFfTestBin]::Build($original,$size))}
    }
    if(!$jobs.Count){throw 'No requested size fits the source App and its declared Flash partition.'}
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $sourceHash){throw 'Source BIN changed during generation.'}

    # Every batch is new; existing formal/test BINs are never overwritten.
    $batch=Join-Path $outputRoot ((Get-Date -Format 'yyyyMMdd_HHmmss')+'_'+[guid]::NewGuid().ToString('N').Substring(0,8))
    Assert-TestBinPlainPath $batch
    if(Test-Path -LiteralPath $batch){throw 'Output batch already exists.'}
    foreach($job in $jobs){
        $name='{0}_test_FF_flash{1:000}KiB_app{2:000}KiB.bin' -f $stem,$job.Budget,($job.Size/1024)
        if((Join-Path $batch $name).Length -ge 260){
            throw 'Output path exceeds the Windows PowerShell path limit; use a shorter App checkout path.'
        }
    }
    New-Item -ItemType Directory -Path $batch | Out-Null
    $records=@()
    foreach($job in $jobs){
        $name='{0}_test_FF_flash{1:000}KiB_app{2:000}KiB.bin' -f $stem,$job.Budget,($job.Size/1024)
        $path=Join-Path $batch $name
        $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try{$stream.Write($job.Bytes,0,$job.Bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
        $saved=[IO.File]::ReadAllBytes($path)
        [DeviceFfTestBin]::Validate($saved)
        if([Convert]::ToBase64String($saved) -cne [Convert]::ToBase64String($job.Bytes)){throw 'Saved BIN differs from generated image.'}
        $records+=[pscustomobject][ordered]@{
            file=$name;flash_budget_kib=$job.Budget;app_kib=($job.Size/1024);input_bytes=$job.Size
            app_start=('0x{0:X8}' -f $start);app_end=('0x{0:X8}' -f ($start+$job.Size-1))
            crc32=('0x{0:X8}' -f [DeviceFfTestBin]::Read32($saved,$saved.Length-4))
            sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            source_bin=[IO.Path]::GetFileName($source);source_bytes=$original.Length;source_sha256=$sourceHash
            app_version=('0x{0:X8}' -f $app.Version);app_build=$app.Build;version_control=$app.VersionControl
            can_mode=$(if($original[0x10e] -eq 2){'CAN FD'}else{'Classical CAN'})
            padding='0xFF';hardware='NOT RUN'
        }
    }
    if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $sourceHash){throw 'Original BIN hash changed.'}
    $records | Export-Csv -LiteralPath (Join-Path $batch 'manifest.csv') -NoTypeInformation -Encoding UTF8
    if($skipped.Count){$skipped | Export-Csv -LiteralPath (Join-Path $batch 'skipped.csv') -NoTypeInformation -Encoding UTF8}
    $note=@'
FF-filled App test images;1KiB=1024bytes.
Names show both Boot+App Flash budget and actual BIN size. Protected tail is excluded.
Source executable/vectors/metadata are preserved except BIN0x118..0x11B image_size.
FF starts at the original CRC location; the new final4bytes contain LE CRC32.
App version/build/update policy/minimum Boot remain unchanged. Normal live Tool gates apply.
This script does not compile, change Boot/Host firmware, connect COM or program a board.
Load the matching test BIN into the existing CAN Upgrade workflow.
FF-filled data represents this test pattern, not arbitrary application contents.
manifest.csv records exact bytes/CRC/hash/source;skipped.csv explains unavailable sizes.
Keep Tool JSON operation reports for actual results;offline checks are not hardware acceptance.
'@
    [IO.File]::WriteAllText((Join-Path $batch 'README.txt'),$note,(New-Object Text.UTF8Encoding($true)))
    Write-Host ('PASS: '+$records.Count+' FF-filled test BINs; original BIN unchanged.')
    Write-Host ('Output: '+$batch)
    foreach($skip in $skipped){Write-Host ('SKIP '+$skip.flash_budget_kib+'KiB: '+$skip.reason)}
} catch {
    [Console]::Error.WriteLine($_.ToString())
    [Console]::Error.WriteLine($_.ScriptStackTrace)
    if($Pause){Read-Host 'Press Enter to close' | Out-Null}
    exit 1
}
if($Pause){Read-Host 'Press Enter to close' | Out-Null}
