param([int]$Target=2,[string]$Profile,[int]$TimeoutSeconds=900)
$ErrorActionPreference='Stop'
$lcsProject=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsWorkspace=(Resolve-Path (Join-Path $lcsProject '../..')).Path
$lcsRuntime=Join-Path $lcsWorkspace 'work/lcs/runtime'
$lcsLogRoot=Join-Path $lcsProject ('logs/campaign-vic'+$Target)
New-Item -ItemType Directory -Path $lcsLogRoot -Force | Out-Null
if(-not $Profile){$Profile=Join-Path $lcsProject 'test-save/profile.txt'}
$lcsLines=Get-Content -LiteralPath $Profile
$lcsSource=Join-Path $lcsRuntime $lcsLines[0].Substring(6)
$lcsName='campaign-vic'+$Target+'-'+[guid]::NewGuid().ToString('N')
$lcsFolder=Join-Path $lcsRuntime ('userfiles/'+$lcsName)
New-Item -ItemType Directory -Path $lcsFolder | Out-Null
Copy-Item -LiteralPath $lcsSource -Destination (Join-Path $lcsFolder 'GTAVCsf8.b')
$lcsLines[0]='game:\userfiles\'+$lcsName+'\GTAVCsf8.b'
$lcsResume=Join-Path $lcsLogRoot 'input-profile.txt'
$lcsLines | Set-Content -LiteralPath $lcsResume -Encoding ascii
$lcsResultPath=Join-Path $lcsRuntime ('campaign-vic'+$Target+'-result.json')
if(Test-Path -LiteralPath $lcsResultPath){Remove-Item -LiteralPath $lcsResultPath}
& (Join-Path $PSScriptRoot 'start_boot.ps1') -SafehouseResume $lcsResume -CampaignTarget $Target
$lcsTestId=[int](Get-Content -LiteralPath (Join-Path $lcsRuntime 'lcs-boot.pid'))
$lcsXenia=(Resolve-Path (Join-Path $lcsWorkspace 'work/xenia/app/xenia_canary.exe')).Path
$lcsDeadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
try {
 while([DateTime]::UtcNow -lt $lcsDeadline){
  if(Test-Path -LiteralPath $lcsResultPath){
   $lcsResult=Get-Content -LiteralPath $lcsResultPath -Raw | ConvertFrom-Json
   $lcsResult | ConvertTo-Json -Depth 8 | Write-Output
   if($lcsResult.status -ne 'PASS'){throw ('Campaign failed: '+$lcsResult.reason)}
   return
  }
  if(-not (Get-Process -Id $lcsTestId -ErrorAction SilentlyContinue)){throw 'Owned Xenia exited'}
  Start-Sleep -Milliseconds 250
 }
 throw 'Campaign test timed out'
} finally {
 $lcsOwner=Get-CimInstance Win32_Process -Filter ('ProcessId='+$lcsTestId) -ErrorAction SilentlyContinue
 if($lcsOwner -and $lcsOwner.ExecutablePath -eq $lcsXenia -and $lcsOwner.CommandLine.Contains('lcs_boot.xex')){Stop-Process -Id $lcsTestId -ErrorAction SilentlyContinue}
 foreach($lcsFile in @('lcs_boot.log','boot-run.json','safehouse-profile.txt','safehouse-loaded.ppm','safehouse-driving.ppm','safehouse-save-menu.ppm',('campaign-vic'+$Target+'-result.json'))){
  $lcsPath=Join-Path $lcsRuntime $lcsFile
  if(Test-Path -LiteralPath $lcsPath){Copy-Item -LiteralPath $lcsPath -Destination $lcsLogRoot}
 }
}
