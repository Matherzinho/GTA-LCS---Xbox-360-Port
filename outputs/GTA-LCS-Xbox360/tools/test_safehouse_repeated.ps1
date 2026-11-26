param([switch]$UseCompletedWarm)
$ErrorActionPreference='Stop'
$lcsProject=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsWorkspace=(Resolve-Path (Join-Path $lcsProject '../..')).Path
$lcsLogs=Join-Path $lcsProject 'logs'
$lcsRuntime=Join-Path $lcsWorkspace 'work/lcs/runtime'
function Archive-LcsCycle([string]$Kind,[string]$Name) {
 $lcsArchive=Join-Path $lcsLogs $Name
 New-Item -ItemType Directory -Path $lcsArchive -Force | Out-Null
 Get-ChildItem -LiteralPath $lcsLogs -Filter ('safehouse-'+$Kind+'-*') -File | Copy-Item -Destination $lcsArchive
}
if(-not $UseCompletedWarm) {
 & (Join-Path $PSScriptRoot 'start_boot.ps1') -City -SafehouseTest
 & (Join-Path $PSScriptRoot 'watch_safehouse_test.ps1') -Kind warm -TimeoutSeconds 600
}
$lcsResult=Get-Content -LiteralPath (Join-Path $lcsLogs 'safehouse-warm-result.json') -Raw | ConvertFrom-Json
$lcsRun=Get-Content -LiteralPath (Join-Path $lcsLogs 'safehouse-warm-boot-run.json') -Raw | ConvertFrom-Json
$lcsBuildHash=(Get-FileHash -LiteralPath (Join-Path $lcsWorkspace 'work/lcs/build-xsdk/lcs_boot.xex')).Hash.ToLower()
if($lcsResult.status -ne 'PASS' -or $lcsRun.sha256 -ne $lcsBuildHash){throw 'Fresh warm-cycle result does not match this build'}
Archive-LcsCycle warm 'safehouse-lcx4-first'
$lcsProfile=Join-Path $lcsLogs 'safehouse-warm-safehouse-profile.txt'
& (Join-Path $PSScriptRoot 'start_boot.ps1') -SafehouseTest -SafehouseResume $lcsProfile
& (Join-Path $PSScriptRoot 'watch_safehouse_test.ps1') -Kind warm -TimeoutSeconds 240
Archive-LcsCycle warm 'safehouse-lcx4-repeat'
& (Join-Path $PSScriptRoot 'start_boot.ps1') -SafehouseResume $lcsProfile
& (Join-Path $PSScriptRoot 'watch_safehouse_test.ps1') -Kind cold -TimeoutSeconds 240
Archive-LcsCycle cold 'safehouse-lcx4-cold'
Write-Output 'PASS: first save, repeated overwrite/reload and cold title-menu load'
