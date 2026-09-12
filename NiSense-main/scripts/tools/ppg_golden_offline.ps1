# PPG golden offline analysis (Phase 3)
#
# Compares packed PPG CSV exports against engineering expectations and
# documents how to F1-check beat detection vs the open ppg-beats toolbox.
#
# Usage:
#   pwsh scripts/tools/ppg_golden_offline.ps1 -InputPath path\to\ppg_raw.csv
#
# References:
#   docs/architecture/PPG_ALGO_DESIGN_REFERENCES.md
#   https://github.com/peterhcharlton/ppg-beats

param(
    [string]$InputPath = "$env:USERPROFILE\Downloads\ppg_raw.csv",
    [string]$OutDir = "$env:USERPROFILE\Downloads",
    [double]$SampleRateHz = 25.0,
    [int]$MinBeats = 8,
    [double]$HrMin = 40.0,
    [double]$HrMax = 200.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$analyze = Join-Path $here 'analyze_packed_ppg_csv.ps1'

if (-not (Test-Path $InputPath)) {
    Write-Error "Input not found: $InputPath"
}

Write-Host "=== PPG golden offline ==="
Write-Host "Input: $InputPath"
Write-Host "Rate:  $SampleRateHz Hz"
Write-Host ""
Write-Host "Running analyze_packed_ppg_csv.ps1 ..."
& $analyze -InputPath $InputPath -OutDir $OutDir -SampleRateHz $SampleRateHz

Write-Host ""
Write-Host "Acceptance checklist (engineering, not clinical):"
Write-Host "  [ ] Mean |IR AC| / DC (PI) >= 0.3%"
Write-Host "  [ ] Estimated HR in $HrMin-$HrMax BPM with >= $MinBeats peaks"
Write-Host "  [ ] SpO2 R-value in 0.30-2.00 when Red/IR present"
Write-Host "  [ ] Motion segments flagged separately from clean windows"
Write-Host ""
Write-Host "Beat-detector F1 vs ppg-beats (Charlton):"
Write-Host "  1. Export IR AC column from the CSV"
Write-Host "  2. Load in MATLAB/Octave with peterhcharlton/ppg-beats"
Write-Host "  3. Compare MSPTDfast / qppg peaks to device peak times"
Write-Host "  4. Target: F1 close to Charlton Physiol Meas 2022 top detectors"
Write-Host ""
Write-Host "Design refs: docs/architecture/PPG_ALGO_DESIGN_REFERENCES.md"
