param([ValidateSet('warm','cold')][string]$Kind='warm',[int]$TimeoutSeconds=480)
$ErrorActionPreference='Stop'
$lcsProject=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsWorkspace=(Resolve-Path (Join-Path $lcsProject '../..')).Path
$lcsRuntime=Join-Path $lcsWorkspace 'work/lcs/runtime'
$lcsLogRoot=Join-Path $lcsProject 'logs'
$lcsXenia=(Resolve-Path (Join-Path $lcsWorkspace 'work/xenia/app/xenia_canary.exe')).Path
$lcsTestId=[int](Get-Content -LiteralPath (Join-Path $lcsRuntime 'lcs-boot.pid') -Raw)
$lcsResultPath=Join-Path $lcsRuntime ('safehouse-'+$Kind+'-result.json')
$lcsDeadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
try {
 while([DateTime]::UtcNow -lt $lcsDeadline) {
  if(Test-Path -LiteralPath $lcsResultPath) {
   $lcsResult=$null
   try {$lcsResult=Get-Content -LiteralPath $lcsResultPath -Raw | ConvertFrom-Json} catch {}
   if($lcsResult) {
    Copy-Item -LiteralPath $lcsResultPath -Destination (Join-Path $lcsLogRoot ('safehouse-'+$Kind+'-result.json'))
    $lcsResult | ConvertTo-Json -Depth 8 | Write-Output
    if($lcsResult.status -ne 'PASS'){throw ('Safehouse '+$Kind+' integration failed: '+$lcsResult.reason)}
    return
   }
  }
  if(-not (Get-Process -Id $lcsTestId -ErrorAction SilentlyContinue)){throw 'Owned Xenia exited before test completion'}
  Start-Sleep -Milliseconds 200
 }
 throw ('Safehouse '+$Kind+' timed out; inspect guest log')
} finally {
 $lcsOwner=Get-CimInstance Win32_Process -Filter ('ProcessId='+$lcsTestId) -ErrorAction SilentlyContinue
 if($lcsOwner -and $lcsOwner.ExecutablePath -eq $lcsXenia -and $lcsOwner.CommandLine.Contains('lcs_boot.xex')){Stop-Process -Id $lcsTestId -ErrorAction SilentlyContinue}
 foreach($lcsFile in @('lcs_boot.log','boot-run.json','safehouse-profile.txt','safehouse-save-menu.ppm','safehouse-loaded.ppm','safehouse-driving.ppm')) {
  $lcsSource=Join-Path $lcsRuntime $lcsFile
  if(Test-Path -LiteralPath $lcsSource){Copy-Item -LiteralPath $lcsSource -Destination (Join-Path $lcsLogRoot ('safehouse-'+$Kind+'-'+$lcsFile))}
 }
}
