param(
    [string]$Python = 'python',
    [string]$EngineRoot,
    [string]$Output
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Output) { $Output = Join-Path $repoRoot 'build-content-converter' }
$outputRoot = [IO.Path]::GetFullPath($Output)
if ($EngineRoot) { $env:SNESRECOMP_ROOT = [IO.Path]::GetFullPath($EngineRoot) }
$pythonCommand = Get-Command $Python -CommandType Application -ErrorAction Stop
# Invoke the resolved executable directly; do not route Windows paths through sh.
& $pythonCommand.Source -X utf8 -m PyInstaller --noconfirm --distpath $outputRoot `
    --workpath (Join-Path $outputRoot 'pyinstaller-work') `
    (Join-Path $PSScriptRoot 'FZeroConvertContent.spec')
if ($LASTEXITCODE -ne 0) { throw "Converter helper build failed ($LASTEXITCODE)" }
& $pythonCommand.Source -X utf8 (Join-Path $PSScriptRoot 'stage_content_importer.py') `
    --write-notices (Join-Path $outputRoot 'licenses')
if ($LASTEXITCODE -ne 0) { throw "Converter license staging failed ($LASTEXITCODE)" }
Get-Item -LiteralPath (Join-Path $outputRoot 'FZeroConvertContent.exe')
