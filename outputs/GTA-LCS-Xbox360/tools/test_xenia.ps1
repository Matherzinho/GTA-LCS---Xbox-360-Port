param([string]$XeniaPath,[string[]]$ProbeNames=@('lcs_data_probe','lcs_render_probe','lcs_model_probe','lcs_io_probe','lcs_text_probe','lcs_scene_probe','lcs_audio_probe'))
$ErrorActionPreference = 'Stop'
$lcsProject = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$lcsWorkspace = (Resolve-Path (Join-Path $lcsProject '../..')).Path
$lcsRuntime = Join-Path $lcsWorkspace 'work/lcs/runtime'
$lcsBuild = Join-Path $lcsWorkspace 'work/lcs/build-xsdk'
# Xenia's Windows host rename can report success while failing on a Unicode host
# path. Keep the same directory, using its OS-provided ASCII 8.3 spelling.
$lcsLaunchRoot = $lcsRuntime
if($lcsRuntime -match '[^\x00-\x7F]') {
    if(-not ('LcsXeniaNativePath' -as [type])) {
        Add-Type -TypeDefinition 'using System.Text; using System.Runtime.InteropServices; public static class LcsXeniaNativePath { [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern uint GetShortPathName(string path, StringBuilder output, uint capacity); }'
    }
    $lcsShort = [System.Text.StringBuilder]::new(2048)
    $lcsShortCount = [LcsXeniaNativePath]::GetShortPathName($lcsRuntime,$lcsShort,2048)
    if($lcsShortCount -gt 0 -and $lcsShortCount -lt 2048 -and $lcsShort.ToString() -notmatch '[^\x00-\x7F]') {
        $lcsLaunchRoot = $lcsShort.ToString()
    }
}
if (-not $XeniaPath) { $XeniaPath = Join-Path $lcsWorkspace 'work/xenia/app/xenia_canary.exe' }
$lcsXenia = (Resolve-Path -LiteralPath $XeniaPath).Path
foreach ($lcsName in $ProbeNames) {
    if($lcsName -notin @('lcs_data_probe','lcs_render_probe','lcs_model_probe','lcs_io_probe','lcs_text_probe','lcs_scene_probe','lcs_audio_probe')) { throw 'Unknown LCS diagnostic' }
    $lcsExe = Join-Path $lcsLaunchRoot ($lcsName+'.xex')
    $lcsGuestLog = Join-Path $lcsRuntime ($lcsName+'.log')
    $lcsHostLog = Join-Path $lcsRuntime ($lcsName+'-xenia.log')
    Copy-Item -LiteralPath (Join-Path $lcsBuild ($lcsName+'.xex')) -Destination $lcsExe
    Set-Content -LiteralPath $lcsGuestLog -Value ''
    $lcsArgs = @('--headless=true','--allow_game_relative_writes=true',
        '--readback_resolve=full','--async_shader_compilation=false',
        ('--log_file="'+$lcsHostLog+'"'),('"'+$lcsExe+'"'))
    $lcsProcess = Start-Process -FilePath $lcsXenia -ArgumentList $lcsArgs -WorkingDirectory $lcsLaunchRoot -WindowStyle Hidden -PassThru
    try {
        $lcsDeadline = [DateTime]::UtcNow.AddSeconds(45)
        $lcsPassed = $false
        while ([DateTime]::UtcNow -lt $lcsDeadline) {
            $lcsText = Get-Content -LiteralPath $lcsGuestLog -Raw
            if ($lcsText -match 'exit: status 0\b') { $lcsPassed = $true; break }
            if ($lcsText -match 'exit: status [1-9]|assertion .*failed|exit: signal') { throw "$lcsName failed: $lcsText" }
            if ($lcsProcess.HasExited) { throw "$lcsName host exited before guest completed" }
            Start-Sleep -Milliseconds 200
        }
        if (-not $lcsPassed) { throw "$lcsName timed out; inspect $lcsHostLog" }
        Write-Output "PASS: $lcsName (internal diagnostic only)"
    } finally {
        # Only the exact process started for this test may be stopped.
        $lcsOwner = Get-CimInstance Win32_Process -Filter ('ProcessId='+$lcsProcess.Id) -ErrorAction SilentlyContinue
        if ($lcsOwner -and $lcsOwner.ExecutablePath -eq $lcsXenia -and $lcsOwner.CommandLine.Contains($lcsExe)) {
            Stop-Process -Id $lcsProcess.Id
        }
        Copy-Item -LiteralPath $lcsGuestLog -Destination (Join-Path $lcsProject ('logs/'+$lcsName+'.log'))
    }
}
