$ErrorActionPreference='Stop'
$lcsProject=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsWorkspace=(Resolve-Path (Join-Path $lcsProject '../..')).Path
$lcsRuntime=Join-Path $lcsWorkspace 'work/lcs/runtime'
$lcsLogs=Join-Path $lcsProject 'logs'
$lcsLines=Get-Content -LiteralPath (Join-Path $lcsLogs 'safehouse-cold-safehouse-profile.txt')
$lcsOriginal=Join-Path $lcsRuntime $lcsLines[0].Substring(6)
$lcsHash=(Get-FileHash -LiteralPath $lcsOriginal).Hash
$lcsName='invalid-lcx4-'+[guid]::NewGuid().ToString('N')
$lcsFolder=Join-Path $lcsRuntime ('userfiles/'+$lcsName)
New-Item -ItemType Directory -Path $lcsFolder | Out-Null
$lcsBytes=[IO.File]::ReadAllBytes($lcsOriginal)
$lcsBytes[$lcsBytes.Length-1]=$lcsBytes[$lcsBytes.Length-1] -bxor 1
[IO.File]::WriteAllBytes((Join-Path $lcsFolder 'GTAVCsf8.b'),$lcsBytes)
$lcsLines[0]='game:\userfiles\'+$lcsName+'\GTAVCsf8.b'
$lcsProfile=Join-Path $lcsLogs 'corrupt-profile.txt'
$lcsLines | Set-Content -LiteralPath $lcsProfile -Encoding ascii
& (Join-Path $PSScriptRoot 'start_boot.ps1') -SafehouseResume $lcsProfile
$lcsExpectedFailure=$false
try {& (Join-Path $PSScriptRoot 'watch_safehouse_test.ps1') -Kind cold -TimeoutSeconds 60}
catch {$lcsExpectedFailure=$true}
$lcsResult=Get-Content -LiteralPath (Join-Path $lcsLogs 'safehouse-cold-result.json') -Raw | ConvertFrom-Json
if(-not $lcsExpectedFailure -or $lcsResult.status -ne 'FAIL' -or $lcsResult.reason -ne 'cold title menu did not recognize saved slot'){throw 'Corrupted slot was not rejected as expected'}
if((Get-FileHash -LiteralPath $lcsOriginal).Hash -ne $lcsHash){throw 'Valid slot changed during corrupt-copy test'}
$lcsRun=Get-Content -LiteralPath (Join-Path $lcsRuntime 'boot-run.json') -Raw | ConvertFrom-Json
[ordered]@{status='PASS';date='2026-11-26';xex_sha256=$lcsRun.sha256;changed_bytes=1;original_unchanged=$true;valid_save_sha256=$lcsHash.ToLower();native_rejection=$lcsResult.reason;scope='Cloned slot rejected by the real title menu before city load; valid original unchanged'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $lcsLogs 'corrupt-native-result.json') -Encoding utf8
Write-Output 'PASS: corrupted clone rejected; valid save unchanged'
