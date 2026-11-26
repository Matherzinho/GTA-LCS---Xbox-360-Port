param([string]$Profile=(Join-Path $PSScriptRoot '../test-save/after-snuff/profile.txt'),[int]$TimeoutSeconds=240)
$ErrorActionPreference='Stop'
$lcsProject=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsRuntime=Join-Path (Resolve-Path (Join-Path $lcsProject '../..')).Path 'work/lcs/runtime'
$lcsSave=(Get-Content -LiteralPath $Profile -First 1).Trim()
& (Join-Path $PSScriptRoot 'start_boot.ps1') -City -StressTest -PerformanceLog -LoadSave $lcsSave
$lcsRun=Get-Content -LiteralPath (Join-Path $lcsRuntime 'boot-run.json') -Raw | ConvertFrom-Json
$lcsOutput=Join-Path $lcsProject ('logs/stress-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $lcsOutput | Out-Null
$lcsDeadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
try {
 while([DateTime]::UtcNow -lt $lcsDeadline){
  $lcsResultPath=Join-Path $lcsRuntime 'stress-result.json'
  if(Test-Path -LiteralPath $lcsResultPath){
   $lcsResult=$null;try{$lcsResult=Get-Content -LiteralPath $lcsResultPath -Raw | ConvertFrom-Json}catch{}
   if($lcsResult){$lcsResult | ConvertTo-Json -Depth 8 | Write-Output;if($lcsResult.status -ne 'PASS'){throw 'Explosion fixture failed'};return}
  }
  if(-not(Get-Process -Id $lcsRun.pid -ErrorAction SilentlyContinue)){throw 'Owned Xenia exited before stress result'}
  Start-Sleep -Milliseconds 250
 }
 throw 'Stress fixture timed out'
} finally {
 $lcsOwner=Get-CimInstance Win32_Process -Filter ('ProcessId='+$lcsRun.pid) -ErrorAction SilentlyContinue
 if($lcsOwner -and $lcsOwner.ExecutablePath -like '*xenia_canary.exe' -and $lcsOwner.CommandLine.Contains('lcs_boot.xex')){Stop-Process -Id $lcsRun.pid -ErrorAction SilentlyContinue}
 foreach($lcsName in @('stress-result.json','stress-cars.ppm','stress-explosions.ppm','stress-wrecks.ppm','lcs_boot.log','boot-run.json')){
  $lcsPath=Join-Path $lcsRuntime $lcsName
  if(Test-Path -LiteralPath $lcsPath){Copy-Item -LiteralPath $lcsPath -Destination $lcsOutput}
 }
}
