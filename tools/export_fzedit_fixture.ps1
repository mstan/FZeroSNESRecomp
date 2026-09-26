param([Parameter(Mandatory=$true)][string]$Editor,
      [Parameter(Mandatory=$true)][string]$Rom,
      [Parameter(Mandatory=$true)][string]$Output,
      [int]$Track=0)
# Run with the editor's architecture (the supplied 1.2.0 editor is x86).
# Calls data export APIs only; never starts the editor UI.
$ErrorActionPreference='Stop'
$editorPath=(Resolve-Path -LiteralPath $Editor).Path
$romPath=(Resolve-Path -LiteralPath $Rom).Path
$outputPath=[IO.Path]::GetFullPath($Output)
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath)) | Out-Null
Set-Location -LiteralPath ([IO.Path]::GetDirectoryName($editorPath))
[Environment]::CurrentDirectory=[IO.Path]::GetDirectoryName($editorPath)
$assembly=[Reflection.Assembly]::LoadFrom($editorPath)
$romType=$assembly.GetType('FZEdit.FZeroRomBase')
$image=[Activator]::CreateInstance($romType,[object[]]@($romPath))
$map=$assembly.GetType('FZEdit.ROMBackedMap').GetMethod('getMap').Invoke($null,[object[]]@($image,$Track))
$assembly.GetType('FZEdit.FZEditMap').GetMethod('exportMap').Invoke($null,[object[]]@($outputPath,$map,$image))
Write-Output "Exported $outputPath"
