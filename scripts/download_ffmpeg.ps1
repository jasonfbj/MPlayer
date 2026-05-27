# Windows FFmpeg 下载脚本
# 用法: powershell -File scripts/download_ffmpeg.ps1

$ErrorActionPreference = "Stop"
$BaseDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$FfmpegDir = Join-Path $BaseDir "third_party\ffmpeg\windows\x64"

$FfmpegVersion = "6.1"
$Url = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z"

Write-Host "Downloading FFmpeg $FfmpegVersion for Windows x64..."
Write-Host "Target: $FfmpegDir"
Write-Host ""
Write-Host "Please download from: $Url"
Write-Host "Extract include/ and lib/ and bin/ to: $FfmpegDir"
Write-Host ""
Write-Host "Or use: Invoke-WebRequest -Uri $Url -OutFile ffmpeg.7z"
