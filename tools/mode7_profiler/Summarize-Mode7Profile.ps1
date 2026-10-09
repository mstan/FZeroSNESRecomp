[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InputDirectory,
    [Parameter(Mandatory = $true)][string]$OutputFile
)
$ErrorActionPreference = 'Stop'
$text = [System.Collections.Generic.List[string]]::new()
$text.Add('F-Zero Mode 7 performance report')
$text.Add('Stage times measure CPU/driver wall time. GPU execution is not measured directly.')
$text.Add('Samples span about two seconds; scene and graphics settings describe the end of each interval.')
$text.Add('Render participants includes the main thread; it is pool capacity. Flat menus/native rendering stay serial.')
$text.Add('')

foreach ($log in Get-ChildItem -LiteralPath $InputDirectory -Filter 'performance-*.jsonl' -File) {
    $records = @()
    $invalid = 0
    foreach ($line in Get-Content -LiteralPath $log.FullName -Encoding UTF8) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        try { $records += ($line | ConvertFrom-Json) } catch { ++$invalid }
    }
    $session = $records | Where-Object kind -eq 'session' | Select-Object -First 1
    $samples = @($records | Where-Object { $_.kind -in 'sample', 'final' -and $_.interval_ms -gt 0 })
    $text.Add("Session: $($log.Name)")
    if ($session) {
        $text.Add("Build: $($session.version) / $($session.revision); engine $($session.framework_revision)")
        $text.Add("CPU: $($session.cpu); $($session.logical_cpus) logical cores; RAM $($session.ram_mb) MB")
        $text.Add("OS: $($session.os)")
        $text.Add("Backend: $($session.backend); GPU: $($session.gpu); driver: $($session.graphics_driver)")
        $text.Add("Attached display adapters: $($session.attached_display_adapters -join ', ')")
        $text.Add("Shader: $($session.shader); loaded: $($session.shader_loaded); vsync: $($session.vsync)")
    }
    $text.Add("Normal exit recorded: $([bool]($records | Where-Object kind -eq 'final'))")
    if ($invalid) { $text.Add("Ignored $invalid incomplete or invalid JSON lines; raw log retained.") }

    $racing = @($samples | Where-Object {
        $_.scene -eq 2 -and $_.subscene -eq 3 -and -not $_.suspended -and $_.stages.paused.calls -eq 0
    })
    foreach ($scope in @(
        [pscustomobject]@{label = 'Whole session'; samples = $samples},
        [pscustomobject]@{label = 'Active racing intervals'; samples = $racing}
    )) {
        $text.Add('')
        $text.Add($scope.label)
        if ($scope.samples.Count -eq 0) { $text.Add('No matching intervals recorded.'); continue }
        $groups = $scope.samples | Group-Object -Property {
            $workers = if ($_.PSObject.Properties.Name -contains 'render_workers') { $_.render_workers } else { 1 }
            "aspect=$($_.aspect), HD=$($_.hd_enabled), requested=$($_.requested_scale)x, effective=$($_.effective_scale)x, source=$($_.source_width)x$($_.source_height), output=$($_.output_width)x$($_.output_height), target=$($_.target_hz) Hz, display=$($_.display_hz) Hz, render participants=$workers"
        }
        foreach ($group in $groups) {
            $duration = ($group.Group | Measure-Object interval_ms -Sum).Sum
            $sim = ($group.Group | Measure-Object simulation_delta -Sum).Sum
            $presents = ($group.Group | Measure-Object presentations_delta -Sum).Sum
            $missed = ($group.Group | Measure-Object missed_delta -Sum).Sum
            $text.Add($group.Name)
            $text.Add(('  {0:F2} seconds; simulation {1:F2} FPS; presentation {2:F2} FPS; missed deadlines {3}' -f ($duration / 1000), ($sim * 1000 / $duration), ($presents * 1000 / $duration), $missed))
            $worst = ($group.Group | Measure-Object frame_interval_max_ms -Maximum).Maximum
            $text.Add(('  Longest presentation interval: {0:F3} ms' -f $worst))
            foreach ($stage in 'simulation', 'ppu', 'composition', 'upload', 'draw_submit', 'present', 'pacing_wait', 'paused') {
                $timings = @($group.Group | ForEach-Object { $_.stages.$stage } | Where-Object { $null -ne $_ })
                $calls = ($timings | Measure-Object calls -Sum).Sum
                if ($calls -gt 0) {
                    $total = ($timings | Measure-Object total_ms -Sum).Sum
                    $maximum = ($timings | Measure-Object max_ms -Maximum).Maximum
                    $text.Add(('  {0}: mean {1:F3} ms, max {2:F3} ms, {3} calls, {4:F1}% of interval' -f $stage, ($total / $calls), $maximum, $calls, ($total * 100 / $duration)))
                }
            }
        }
    }
    $text.Add('')
}
if (-not (Get-ChildItem -LiteralPath $InputDirectory -Filter 'performance-*.jsonl' -File)) { throw 'No performance logs found.' }
$text | Set-Content -LiteralPath $OutputFile -Encoding UTF8
