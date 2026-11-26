param([switch]$City,[switch]$VideoTest,[switch]$StressTest,[switch]$Diagnostics,[switch]$PerformanceLog,[switch]$TracePose,[switch]$GameplayTest,[switch]$SafehouseTest,[switch]$CaptureAudio,[string]$XeniaPath,[string]$LoadSave,[string]$SafehouseResume,[int]$CampaignTarget=0)
# Reuse the diagnostic runner's path setup, with no diagnostics selected.
. (Join-Path $PSScriptRoot 'test_xenia.ps1') -XeniaPath $XeniaPath -ProbeNames @()
$lcsExe=Join-Path $lcsLaunchRoot 'lcs_boot.xex'
$lcsPidFile=Join-Path $lcsRuntime 'lcs-boot.pid'
if(Test-Path -LiteralPath $lcsPidFile) {
    $lcsOldId=[int](Get-Content -LiteralPath $lcsPidFile -Raw)
    $lcsOld=Get-CimInstance Win32_Process -Filter ('ProcessId='+$lcsOldId) -ErrorAction SilentlyContinue
    if($lcsOld -and $lcsOld.ExecutablePath -eq $lcsXenia -and $lcsOld.CommandLine.Contains($lcsExe)) { Stop-Process -Id $lcsOldId -ErrorAction SilentlyContinue }
}
$lcsVideoFlag=Join-Path $lcsRuntime 'video-test.txt'
if($VideoTest){Set-Content -LiteralPath $lcsVideoFlag -Value '';Remove-Item -LiteralPath (Join-Path $lcsRuntime 'video-result.json') -ErrorAction SilentlyContinue}
elseif(Test-Path -LiteralPath $lcsVideoFlag){Remove-Item -LiteralPath $lcsVideoFlag}
$lcsStressFlag=Join-Path $lcsRuntime 'stress-test.txt'
if($StressTest){Set-Content -LiteralPath $lcsStressFlag -Value '';Remove-Item -LiteralPath (Join-Path $lcsRuntime 'stress-result.json') -ErrorAction SilentlyContinue}
elseif(Test-Path -LiteralPath $lcsStressFlag){Remove-Item -LiteralPath $lcsStressFlag}
$lcsCityFlag=Join-Path $lcsRuntime 'boot-city.txt'
if($City) { Set-Content -LiteralPath $lcsCityFlag -Value '' }
elseif(Test-Path -LiteralPath $lcsCityFlag) { Remove-Item -LiteralPath $lcsCityFlag }
$lcsPerfFlag=Join-Path $lcsRuntime 'performance-log.txt'
if($PerformanceLog){Set-Content -LiteralPath $lcsPerfFlag -Value ''}
elseif(Test-Path -LiteralPath $lcsPerfFlag){Remove-Item -LiteralPath $lcsPerfFlag}
$lcsDiagnosticsEnabled=[bool]($Diagnostics -or $TracePose -or $GameplayTest -or $SafehouseTest -or $SafehouseResume -or $CampaignTarget)
$lcsDiagnosticsFlag=Join-Path $lcsRuntime 'boot-diagnostics.txt'
if($lcsDiagnosticsEnabled){Set-Content -LiteralPath $lcsDiagnosticsFlag -Value ''}
elseif(Test-Path -LiteralPath $lcsDiagnosticsFlag){Remove-Item -LiteralPath $lcsDiagnosticsFlag}
$lcsTraceFlag=Join-Path $lcsRuntime 'boot-pose-trace.txt'
if($TracePose) { Set-Content -LiteralPath $lcsTraceFlag -Value '' }
elseif(Test-Path -LiteralPath $lcsTraceFlag) { Remove-Item -LiteralPath $lcsTraceFlag }
foreach($lcsPoseFrame in @(120,240,360,480,600,720)) {
    $lcsPosePath=Join-Path $lcsRuntime ('skin-pose-{0:00000}.json' -f $lcsPoseFrame)
    if(Test-Path -LiteralPath $lcsPosePath) { Remove-Item -LiteralPath $lcsPosePath }
}
foreach($lcsTestFlag in @(@('gameplay-test.txt',[bool]$GameplayTest),@('safehouse-test.txt',[bool]$SafehouseTest),@('audio-capture.txt',[bool]$CaptureAudio))) {
    $lcsFlagPath=Join-Path $lcsRuntime $lcsTestFlag[0]
    if($lcsTestFlag[1]) { Set-Content -LiteralPath $lcsFlagPath -Value '' }
    elseif(Test-Path -LiteralPath $lcsFlagPath) { Remove-Item -LiteralPath $lcsFlagPath }
}
$lcsCampaignFlag=Join-Path $lcsRuntime 'campaign-test.txt'
if($CampaignTarget){Set-Content -LiteralPath $lcsCampaignFlag -Value $CampaignTarget -Encoding ascii;Set-Content -LiteralPath (Join-Path $lcsRuntime 'campaign-input.txt') -Value '0 idle 0 0 0' -Encoding ascii}
elseif(Test-Path -LiteralPath $lcsCampaignFlag){Remove-Item -LiteralPath $lcsCampaignFlag}
if(-not $CampaignTarget -and (Test-Path -LiteralPath (Join-Path $lcsRuntime 'campaign-input.txt'))){Remove-Item -LiteralPath (Join-Path $lcsRuntime 'campaign-input.txt')}
$lcsResumeFlag=Join-Path $lcsRuntime 'safehouse-resume.txt'
if($SafehouseResume){Copy-Item -LiteralPath $SafehouseResume -Destination $lcsResumeFlag}
elseif(Test-Path -LiteralPath $lcsResumeFlag){Remove-Item -LiteralPath $lcsResumeFlag}
$lcsReloadFlag=Join-Path $lcsRuntime 'reload-test.txt'
if($LoadSave) { Set-Content -LiteralPath $lcsReloadFlag -Value $LoadSave -Encoding ascii }
elseif(Test-Path -LiteralPath $lcsReloadFlag) { Remove-Item -LiteralPath $lcsReloadFlag }
if($SafehouseTest) { Set-Content -LiteralPath (Join-Path $lcsRuntime 'safehouse-command.txt') -Value '0 idle' -Encoding ascii }
elseif(Test-Path -LiteralPath (Join-Path $lcsRuntime 'safehouse-command.txt')) { Remove-Item -LiteralPath (Join-Path $lcsRuntime 'safehouse-command.txt') }
$lcsStopFlag=Join-Path $lcsRuntime 'boot-stop.txt'
if(Test-Path -LiteralPath $lcsStopFlag) { Remove-Item -LiteralPath $lcsStopFlag }
foreach($lcsCapture in @('safehouse-save-menu.ppm','safehouse-loaded.ppm','safehouse-driving.ppm','safehouse-warm-result.json','safehouse-cold-result.json','gameplay-test-result.json','gameplay-walking.ppm','gameplay-driving.ppm','gameplay-loaded.ppm','boot-menu.ppm','boot-city.ppm','boot-city-latest.ppm','boot-frame-00120.ppm','boot-frame-00240.ppm','boot-frame-00360.ppm','boot-frame-00480.ppm','boot-frame-00600.ppm','boot-frame-00720.ppm')) {
    $lcsCapturePath=Join-Path $lcsRuntime $lcsCapture
    if(Test-Path -LiteralPath $lcsCapturePath) { Remove-Item -LiteralPath $lcsCapturePath }
}
Copy-Item -LiteralPath (Join-Path $lcsBuild 'lcs_boot.xex') -Destination $lcsExe
$lcsHostLog=Join-Path $lcsLaunchRoot 'xenia-boot-city.log'
$lcsStarted=[DateTime]::UtcNow
$lcsArgs=@('--headless=true','--allow_game_relative_writes=true','--readback_resolve=full',
    '--async_shader_compilation=false',('--log_file="'+$lcsHostLog+'"'),('"'+$lcsExe+'"'))
$lcsProcess=Start-Process -FilePath $lcsXenia -ArgumentList $lcsArgs -WorkingDirectory $lcsLaunchRoot -WindowStyle Hidden -PassThru
$lcsProcess.Id | Set-Content -LiteralPath $lcsPidFile
[ordered]@{pid=$lcsProcess.Id;started_utc=$lcsStarted.ToString('o');autostart_city=[bool]$City;trace_pose=[bool]$TracePose;
    video_test=[bool]$VideoTest;stress_test=[bool]$StressTest;performance_log=[bool]$PerformanceLog;diagnostics=$lcsDiagnosticsEnabled;campaign_target=$CampaignTarget;sha256=(Get-FileHash -LiteralPath $lcsExe -Algorithm SHA256).Hash.ToLower();
    gameplay_test=[bool]$GameplayTest;capture_audio=[bool]$CaptureAudio;safehouse_test=[bool]$SafehouseTest;safehouse_resume=[bool]$SafehouseResume;
    scope='Real game boot, native XSDK audio, basic renderer; internal, not a release'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $lcsRuntime 'boot-run.json') -Encoding utf8
Write-Output ('Started LCS boot, PID '+$lcsProcess.Id+'; city='+[bool]$City)
