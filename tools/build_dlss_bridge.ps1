param([string]$Build = (Join-Path $PSScriptRoot '..\build-experiment'))
$ErrorActionPreference = 'Stop'
$deps = [IO.Path]::GetFullPath((Join-Path $Build 'dlss-deps'))
$source = Join-Path $deps 'bridge-source'
$root = Join-Path $deps 'bridge\ComfyUI-DLSS5-NR'
$patch = Join-Path $PSScriptRoot 'dlss_temporal.patch'
$revision = '3745b8ab6c70761e8d9e7daf47948a389833086f'
$git = Join-Path $env:ProgramFiles 'Git\cmd\git.exe'
if (!(Test-Path -LiteralPath $git)) { $git = (Get-Command git.exe -ErrorAction Stop).Source }
if (!(Test-Path -LiteralPath $source)) {
    & $git clone --branch v0.3.0 --depth 1 https://github.com/lisitskyaa/ComfyUI-DLSS5-NR.git $source
    if ($LASTEXITCODE) { throw 'Bridge source download failed' }
}
$actual = & $git -C $source rev-parse HEAD
if ($LASTEXITCODE -or $actual -ne $revision) { throw 'Unexpected bridge source revision' }
$previousPreference = $ErrorActionPreference
try {
    # A failed forward check is expected on an already-patched checkout.
    $ErrorActionPreference = 'Continue'
    & $git -C $source apply --check $patch 2>$null
    $forward = $LASTEXITCODE
    & $git -C $source apply --reverse --check $patch 2>$null
    $reverse = $LASTEXITCODE
} finally { $ErrorActionPreference = $previousPreference }
if ($forward -eq 0) {
    & $git -C $source apply $patch
    if ($LASTEXITCODE) { throw 'Temporal patch failed' }
} else {
    if ($reverse) { throw 'Bridge source has incompatible edits; leave it intact and use a fresh build directory' }
}
Push-Location -LiteralPath $source
try {
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File (Join-Path $source 'native\build_native.ps1')
    if ($LASTEXITCODE) { throw 'Native bridge build failed' }
} finally { Pop-Location }
$bridge = Join-Path $source 'native\bin\dlss5nr_bridge.dll'
$shim = Join-Path $source 'runtime\caller\nvngx.dll_comfy.dll'
# Failing on a loaded DLL is intentional: never kill another application's worker.
Copy-Item -LiteralPath $bridge -Destination (Join-Path $root 'native\bin\dlss5nr_bridge.dll') -Force
Copy-Item -LiteralPath $shim -Destination (Join-Path $root 'runtime\caller\nvngx.dll_comfy.dll') -Force
@{
    revision=$revision
    patch_sha256=(Get-FileHash -LiteralPath $patch).Hash
    bridge_sha256=(Get-FileHash -LiteralPath $bridge).Hash
    shim_sha256=(Get-FileHash -LiteralPath $shim).Hash
    version='0.3.0-fzero-temporal-r8-1'
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $deps 'temporal-bridge.json') -Encoding UTF8
