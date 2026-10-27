# Bounded publication of project-owned timing images. No recursive cleanup.
function Get-TestBinHash([byte[]]$Bytes) {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($Bytes)).Replace('-','') }
    finally { $sha.Dispose() }
}
function Assert-TestBinOutput([string]$Project,[string]$Path) {
    $root=[IO.Path]::GetFullPath((Join-Path $Project 'test_bins')).TrimEnd('\')
    $full=[IO.Path]::GetFullPath($Path)
    if(!$full.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Timing-image output escaped this App test_bins directory.'
    }
    Assert-TestBinPlainPath $full
}
function Read-TestBinOwnership([string]$Project,[string]$Directory,[string]$Owner) {
    Assert-TestBinOutput $Project $Directory
    $manifest=Join-Path $Directory '.owned.csv'
    Assert-TestBinOutput $Project $manifest
    if(!(Test-Path -LiteralPath $manifest -PathType Leaf)){throw 'Automatic output is not generator-owned; existing files are preserved.'}
    $rows=@(Import-Csv -LiteralPath $manifest)
    if(!$rows.Count -or $rows.Count -gt 32){throw 'Invalid timing-image ownership record.'}
    $seen=@{};$owned=@()
    $stem=[regex]::Escape($Owner.Substring('DeviceAppTimingBins/1:'.Length))
    $allowed='^(?:manifest\.csv|skipped\.csv|README\.txt|index\.html|'+$stem+'_test_FF_flash[0-9]+KiB_app[0-9]+KiB\.bin)$'
    foreach($row in $rows) {
        if($row.owner -ne $Owner -or $row.file -ne [IO.Path]::GetFileName($row.file) -or
           $row.file -notmatch $allowed -or
           $seen.ContainsKey($row.file) -or $row.sha256 -notmatch '^[0-9A-F]{64}$') {
            throw 'Invalid timing-image ownership path, identity or hash.'
        }
        $path=Join-Path $Directory $row.file
        Assert-TestBinOutput $Project $path
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or
           (Get-Item -LiteralPath $path).Length -ne [long]$row.bytes -or
           (Get-FileHash -LiteralPath $path).Hash -cne $row.sha256) {
            throw ('Automatic output was changed; preserve/review it before rebuilding: '+$row.file)
        }
        $seen[$row.file]=$true
        $owned+=[pscustomobject]@{File=$row.file;Hash=$row.sha256}
    }
    $actual=@(Get-ChildItem -LiteralPath $Directory -Force)
    if($actual.Count -ne $rows.Count+1 -or @($actual|Where-Object {$_.PSIsContainer -or
        ($_.Name -ne '.owned.csv' -and !$seen.ContainsKey($_.Name))}).Count) {
        throw 'Automatic output contains unowned additions; no files were replaced.'
    }
    $owned+=[pscustomobject]@{File='.owned.csv';Hash=(Get-FileHash -LiteralPath $manifest).Hash}
    return $owned
}
function Remove-OwnedTestBinDirectory([string]$Project,[string]$Directory,$Owned) {
    Assert-TestBinOutput $Project $Directory
    # Complete preflight before deleting any file. Unknown additions are retained.
    foreach($entry in $Owned) {
        $path=Join-Path $Directory $entry.File
        Assert-TestBinOutput $Project $path
        if(!(Test-Path -LiteralPath $path -PathType Leaf) -or
           (Get-FileHash -LiteralPath $path).Hash -cne $entry.Hash) {throw 'Owned staging output changed; cleanup stopped.'}
    }
    foreach($entry in $Owned){[IO.File]::Delete((Join-Path $Directory $entry.File))}
    [IO.Directory]::Delete($Directory,$false)
}
function Publish-TestBinBatch([string]$Project,[string]$Stem,[hashtable]$Files,[bool]$Automatic) {
    $root=Join-Path $Project 'test_bins'
    Assert-TestBinPlainPath $root
    [IO.Directory]::CreateDirectory($root)|Out-Null
    $owner='DeviceAppTimingBins/1:'+$Stem
    $id=[guid]::NewGuid().ToString('N').Substring(0,8)
    $destination=Join-Path $root $(if($Automatic){'auto'}else{(Get-Date -Format 'yyyyMMdd_HHmmss')+'_'+$id.Substring(0,8)})
    $stage=Join-Path $root ('.new_'+$id)
    $previous=Join-Path $root ('.old_'+$id)
    foreach($path in @($destination,$stage,$previous)){Assert-TestBinOutput $Project $path}
    $guard=Get-TestBinHash ([Text.Encoding]::UTF8.GetBytes($Project.ToUpperInvariant()))
    $mutex=New-Object Threading.Mutex($false,('Local\DeviceAppTimingBins_'+$guard));$held=$false
    $created=@();$old=@();$stageExists=$false;$movedOld=$false
    try {
        try {$held=$mutex.WaitOne(10000)} catch [Threading.AbandonedMutexException] {$held=$true}
        if(!$held){throw 'Another timing-image generation is active for this App.'}
        if(@(Get-ChildItem -LiteralPath $root -Force|Where-Object {$_.Name -match '^\.(new|old)_[0-9a-f]{8}$'}).Count) {
            throw 'Interrupted timing-image staging exists; preserve/review it before rebuilding.'
        }
        if(Test-Path -LiteralPath $destination) {
            if(!$Automatic){throw 'Manual output batch already exists.'}
            $old=@(Read-TestBinOwnership $Project $destination $owner)
            $same=$old.Count -eq $Files.Count+1
            foreach($entry in $old) {
                if($entry.File -eq '.owned.csv'){continue}
                if(!$Files.ContainsKey($entry.File) -or (Get-TestBinHash $Files[$entry.File]) -cne $entry.Hash){$same=$false}
            }
            if($same){Write-Host ('Timing images current: '+$destination);return $destination}
        }
        if((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $previous)){throw 'Staging collision.'}
        [IO.Directory]::CreateDirectory($stage)|Out-Null;$stageExists=$true
        $records=@()
        foreach($name in @($Files.Keys|Sort-Object)) {
            if($name -ne [IO.Path]::GetFileName($name)){throw 'Invalid output basename.'}
            $path=Join-Path $stage $name
            Assert-TestBinOutput $Project $path
            if($path.Length -ge 260 -or (Join-Path $destination $name).Length -ge 260){throw 'Timing-image path exceeds the Windows PowerShell path limit.'}
            $stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
            try {$stream.Write($Files[$name],0,$Files[$name].Length);$stream.Flush($true)} finally {
                $stream.Dispose()
                $created+=[pscustomobject]@{File=$name;Hash=(Get-FileHash -LiteralPath $path).Hash}
            }
            $hash=(Get-FileHash -LiteralPath $path).Hash
            if($hash -cne (Get-TestBinHash $Files[$name])){throw 'Saved timing output differs from generated bytes.'}
            $records+=[pscustomobject]@{owner=$owner;file=$name;bytes=$Files[$name].Length;sha256=$hash}
        }
        $csv=($records|ConvertTo-Csv -NoTypeInformation)-join "`r`n"
        $manifest=Join-Path $stage '.owned.csv'
        $data=(New-Object Text.UTF8Encoding($true)).GetBytes($csv)
        $stream=[IO.File]::Open($manifest,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try {$stream.Write($data,0,$data.Length);$stream.Flush($true)} finally {
            $stream.Dispose()
            $created+=[pscustomobject]@{File='.owned.csv';Hash=(Get-FileHash -LiteralPath $manifest).Hash}
        }
        $null=Read-TestBinOwnership $Project $stage $owner
        if($old.Count) {
            $null=Read-TestBinOwnership $Project $destination $owner
            [IO.Directory]::Move($destination,$previous);$movedOld=$true
        }
        try {[IO.Directory]::Move($stage,$destination);$stageExists=$false}
        catch {if($movedOld){[IO.Directory]::Move($previous,$destination);$movedOld=$false};throw}
        if($movedOld){Remove-OwnedTestBinDirectory $Project $previous $old;$movedOld=$false}
        return $destination
    } finally {
        try {if($stageExists){Remove-OwnedTestBinDirectory $Project $stage $created}}
        finally {if($held){$mutex.ReleaseMutex()};$mutex.Dispose()}
    }
}
