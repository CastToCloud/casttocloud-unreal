param([string]$PluginDir)

Write-Host "[CastToCloud] CLI download script started"
$Version = "latest"
if (Test-Path "$PluginDir\Scripts\Cli\version.txt") {
    $pinnedVersion = (Get-Content "$PluginDir\Scripts\Cli\version.txt" -Raw).Trim()
    if ($pinnedVersion) { $Version = $pinnedVersion }
}

function Download-Cli($Platform, $SubDir, $FileName) {
    $CliPath     = "$PluginDir\Binaries\$SubDir\$FileName"
    $VersionPath = "$PluginDir\Binaries\$SubDir\casttocloud-cli.version"

    if ((Test-Path $CliPath) -and (Test-Path $VersionPath) -and (Get-Content $VersionPath -Raw).Trim() -eq $Version) {
        Write-Host "[CastToCloud] $Platform CLI $Version up to date, skipping"
        return
    }

    New-Item -ItemType Directory -Force -Path (Split-Path $CliPath) | Out-Null
    Write-Host "[CastToCloud] Downloading $Platform CLI $Version -> $CliPath"

    try {
        (New-Object System.Net.WebClient).DownloadFile("https://api.casttocloud.com/cli/download?platform=$Platform&version=$Version", $CliPath)
        Set-Content $VersionPath $Version -NoNewline
    } catch {
        Write-Host "[CastToCloud] WARNING: $Platform CLI download failed"
    }
}

Download-Cli windows Win64 casttocloud-cli.exe
Download-Cli macos   Mac   casttocloud-cli
Download-Cli linux   Linux casttocloud-cli

Write-Host "[CastToCloud] CLI download script finished"
