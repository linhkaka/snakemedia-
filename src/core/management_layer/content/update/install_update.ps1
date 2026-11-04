# Jenny Script fork: replaces the application files with a downloaded release, then restarts the app.
# The app starts this script and closes itself. If anything fails, the files that were already
# replaced are restored, so the old version keeps working. Files the release does not contain
# (anything the user keeps in the folder) are left alone.
param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [Parameter(Mandatory = $true)][string]$ZipPath,
    [Parameter(Mandatory = $true)][string]$InstallDir,
    [Parameter(Mandatory = $true)][string]$ExePath,
    # For tests: no message box and no restart
    [switch]$Silent
)

$ErrorActionPreference = 'Stop'
$workDir = Split-Path -Parent $ZipPath
$logPath = Join-Path $workDir 'update.log'
$stagingDir = Join-Path $workDir 'staging'
$backupDir = Join-Path $workDir 'backup'
$replaced = New-Object System.Collections.ArrayList
$added = New-Object System.Collections.ArrayList

function Write-Log([string]$Message) {
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value "$(Get-Date -Format s) $Message"
}

function Copy-WithRetry([string]$Source, [string]$Target) {
    # The closed app and antivirus scanners can hold a file for a few more seconds
    for ($attempt = 1; ; $attempt++) {
        try {
            Copy-Item -LiteralPath $Source -Destination $Target -Force
            return
        } catch {
            if ($attempt -ge 8) { throw }
            Start-Sleep -Seconds 1
        }
    }
}

function Restore-OldFiles {
    foreach ($relative in $replaced) {
        Copy-WithRetry (Join-Path $backupDir $relative) (Join-Path $InstallDir $relative)
    }
    foreach ($relative in $added) {
        Remove-Item -LiteralPath (Join-Path $InstallDir $relative) -Force -ErrorAction SilentlyContinue
    }
}

$failure = $null
try {
    Write-Log "Update started, install folder: $InstallDir"

    Wait-Process -Id $ProcessId -Timeout 120 -ErrorAction SilentlyContinue
    if (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue) {
        throw 'The application did not close in time'
    }

    foreach ($folder in @($stagingDir, $backupDir)) {
        if (Test-Path -LiteralPath $folder) { Remove-Item -LiteralPath $folder -Recurse -Force }
    }
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $stagingDir -Force
    $source = Get-ChildItem -LiteralPath $stagingDir -Directory |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName (Split-Path -Leaf $ExePath)) } |
        Select-Object -First 1
    if ($null -eq $source) { throw 'The archive does not contain the application' }

    foreach ($file in Get-ChildItem -LiteralPath $source.FullName -Recurse -File) {
        $relative = $file.FullName.Substring($source.FullName.Length).TrimStart('\')
        $target = Join-Path $InstallDir $relative
        $targetFolder = Split-Path -Parent $target
        if (-not (Test-Path -LiteralPath $targetFolder)) {
            New-Item -ItemType Directory -Path $targetFolder -Force | Out-Null
        }
        if (Test-Path -LiteralPath $target) {
            $backup = Join-Path $backupDir $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
            Copy-Item -LiteralPath $target -Destination $backup -Force
            Copy-WithRetry $file.FullName $target
            [void]$replaced.Add($relative)
        } else {
            Copy-WithRetry $file.FullName $target
            [void]$added.Add($relative)
        }
    }

    Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $backupDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Log "Update finished: $($replaced.Count) files replaced, $($added.Count) added"
} catch {
    $failure = $_.Exception.Message
    Write-Log "Update failed: $failure"
    try {
        Restore-OldFiles
        Write-Log 'Old files restored'
    } catch {
        Write-Log "Restore failed: $($_.Exception.Message)"
        $failure = "$failure. Restore failed: $($_.Exception.Message)"
    }
}

if (-not $Silent) {
    if ($null -ne $failure) {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(
            "Không cập nhật được, app sẽ mở lại bản cũ.`nKịch bản của bạn không bị ảnh hưởng.`n`nChi tiết: $logPath",
            'Jenny Script')
    }
    Start-Process -FilePath $ExePath -WorkingDirectory $InstallDir
}

if ($null -ne $failure) { exit 1 }
exit 0
