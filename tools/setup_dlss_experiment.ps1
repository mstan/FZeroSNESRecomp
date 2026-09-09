param(
    [string]$Build = (Join-Path $PSScriptRoot '..\build-experiment'),
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
)
$ErrorActionPreference = 'Stop'
$deps = [IO.Path]::GetFullPath((Join-Path $Build 'dlss-deps'))
New-Item -ItemType Directory -Force -Path $deps | Out-Null
$packages = @(
    @{ Name='bridge'; Url='https://github.com/lisitskyaa/ComfyUI-DLSS5-NR/releases/download/v0.3.0/ComfyUI-DLSS5-NR-v0.3.0-windows-x64.zip'; Hash='3b7d52507a5548d10c3f60f9a5ea4cc5eb2fd9bd715b536533df55887a1d907f' },
    @{ Name='runtime'; Url='https://github.com/RankFTW/rhi-repo/releases/download/dlssnr-310.8.SF-v2/nvngx_dlssnr_310.8.SF-v2.zip'; Hash='1da35941894994eb087e017577829e492454e9bae3a6a9397027069ceb74955c' }
)
foreach ($package in $packages) {
    $archive = Join-Path $deps ($package.Name + '.zip')
    if (!(Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -UseBasicParsing -Uri $package.Url -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive).Hash -ne $package.Hash) {
        throw "Hash mismatch: $archive"
    }
    $destination = Join-Path $deps $package.Name
    if (!(Test-Path -LiteralPath $destination)) {
        Expand-Archive -LiteralPath $archive -DestinationPath $destination
    }
}
$root = Join-Path $deps 'bridge\ComfyUI-DLSS5-NR'
$runtime = Join-Path $root 'runtime\nvngx_dlssnr.dll'
if (!(Test-Path -LiteralPath $runtime)) {
    Copy-Item -LiteralPath (Join-Path $deps 'runtime\nvngx_dlssnr.dll') -Destination $runtime
}
if ((Get-FileHash -LiteralPath $runtime).Hash -ne '6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927') {
    throw 'Extracted runtime hash mismatch'
}
$venv = Join-Path $deps 'venv'
if (!(Test-Path -LiteralPath (Join-Path $venv 'Scripts\python.exe'))) {
    & $Python -m venv $venv
    if ($LASTEXITCODE) { throw 'venv creation failed' }
}
& (Join-Path $venv 'Scripts\python.exe') -m pip install numpy==2.2.6 Pillow==11.3.0
if ($LASTEXITCODE) { throw 'Dependency installation failed' }
& (Join-Path $PSScriptRoot 'build_dlss_bridge.ps1') -Build $Build
$manifest = @{
    bridge_revision='3745b8ab6c70761e8d9e7daf47948a389833086f'
    packages=$packages
    runtime_sha256=(Get-FileHash -LiteralPath $runtime).Hash
    runtime_signature=(Get-AuthenticodeSignature -LiteralPath $runtime).Status.ToString()
    runtime_version=(Get-Item -LiteralPath $runtime).VersionInfo.FileVersion
    gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    numpy='2.2.6'; pillow='11.3.0'
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $deps 'manifest.json') -Encoding UTF8
Write-Host "DLSS experiment runtime ready: $root"
