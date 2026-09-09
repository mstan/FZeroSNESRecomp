param(
    [string]$Build = (Join-Path $PSScriptRoot '..\build-experiment'),
    [string]$Rom,
    [switch]$Dlss
)
$ErrorActionPreference = 'Stop'
$buildPath = [IO.Path]::GetFullPath($Build)
$env:FZERO_OUTPUT_METHOD = 'Vulkan'
if ($Dlss) { $env:FZERO_DLSS = '1' }
else { Remove-Item Env:FZERO_DLSS -ErrorAction SilentlyContinue }
$env:PATH = "$buildPath;C:\msys64\mingw64\bin;" + $env:PATH
$executable = Join-Path $buildPath 'FZeroSNESRecomp.exe'
if ($Rom) { & $executable $Rom } else { & $executable }
exit $LASTEXITCODE
