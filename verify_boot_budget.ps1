param([string]$MapPath=(Join-Path $PSScriptRoot 'DefaultBuild\RL78_F13_Boot_loader_CAN.map'))
$ErrorActionPreference='Stop'
$text=[IO.File]::ReadAllText($MapPath).Replace("`r`n","`n")
$part=[regex]::Match($text,'(?s)\*\*\* Mapping List \*\*\*(.*?)\*\*\* Total Section Size \*\*\*')
if(!$part.Success){throw 'Missing linker mapping evidence.'}
$end=-1L
foreach($section in [regex]::Matches($part.Groups[1].Value,'(?m)^(\S+)\s*\n +([0-9a-f]{8}) +([0-9a-f]{8}) +([0-9a-f]+) +\d+')){
    $name=$section.Groups[1].Value
    $start=[Convert]::ToInt64($section.Groups[2].Value,16)
    $last=[Convert]::ToInt64($section.Groups[3].Value,16)
    $size=[Convert]::ToInt64($section.Groups[4].Value,16)
    if(!$size -or $start -ge 0xF0000 -or $name -eq '.monitor2'){continue}
    if($size -ne $last-$start+1 -or $last -ge 0x1E00){throw "Boot8KiB /512-byte spare guard failed: $name"}
    if($last -gt $end){$end=$last}
}
if($end -lt 0){throw 'Missing Boot ROM sections.'}
Write-Host ('PASS: Boot8KiB, highest0x{0:X4}, spare{1} bytes (minimum512).' -f $end,(8192L-$end-1L))
