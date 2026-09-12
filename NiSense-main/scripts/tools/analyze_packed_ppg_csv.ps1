param(
    [string]$InputPath = "$env:USERPROFILE\Downloads\ppg_raw.csv",
    [string]$OutDir = "$env:USERPROFILE\Downloads",
    [double]$SampleRateHz = 25.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Parse-NumericSeries {
    param([string]$Text)

    $result = [ordered]@{
        Values = [System.Collections.Generic.List[double]]::new()
        TotalTokens = 0
        BadTokens = 0
    }

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return [pscustomobject]$result
    }

    $tokens = $Text.Split(';')
    $result.TotalTokens = $tokens.Length

    foreach ($t in $tokens) {
        $v = 0.0
        if ([double]::TryParse($t, [ref]$v)) {
            $result.Values.Add($v)
        }
        else {
            $result.BadTokens++
        }
    }

    return [pscustomobject]$result
}

function Get-Mean {
    param([double[]]$Data)
    if ($Data.Length -eq 0) { return 0.0 }
    $sum = 0.0
    foreach ($v in $Data) { $sum += $v }
    return ($sum / $Data.Length)
}

function Get-Std {
    param([double[]]$Data, [double]$Mean)
    if ($Data.Length -lt 2) { return 0.0 }
    $ss = 0.0
    foreach ($v in $Data) {
        $d = $v - $Mean
        $ss += ($d * $d)
    }
    return [Math]::Sqrt($ss / ($Data.Length - 1))
}

function Get-Correlation {
    param([double[]]$A, [double[]]$B)

    $n = [Math]::Min($A.Length, $B.Length)
    if ($n -lt 4) { return 0.0 }

    $am = 0.0
    $bm = 0.0
    for ($i = 0; $i -lt $n; $i++) {
        $am += $A[$i]
        $bm += $B[$i]
    }
    $am /= $n
    $bm /= $n

    $cov = 0.0
    $av = 0.0
    $bv = 0.0
    for ($i = 0; $i -lt $n; $i++) {
        $da = $A[$i] - $am
        $db = $B[$i] - $bm
        $cov += ($da * $db)
        $av += ($da * $da)
        $bv += ($db * $db)
    }

    if ($av -le 0.0 -or $bv -le 0.0) { return 0.0 }
    return ($cov / [Math]::Sqrt($av * $bv))
}

function Get-NaiveBpm {
    param([double[]]$Data, [double]$Fs)

    if ($Data.Length -lt 8) { return 0.0 }

    $smooth = [double[]]::new($Data.Length - 2)
    for ($i = 1; $i -lt ($Data.Length - 1); $i++) {
        $smooth[$i - 1] = ($Data[$i - 1] + $Data[$i] + $Data[$i + 1]) / 3.0
    }

    $mean = Get-Mean -Data $smooth
    $peaks = 0
    for ($i = 1; $i -lt ($smooth.Length - 1); $i++) {
        if ($smooth[$i] -gt $smooth[$i - 1] -and $smooth[$i] -gt $smooth[$i + 1] -and $smooth[$i] -gt $mean) {
            $peaks++
        }
    }

    $durationSec = $Data.Length / $Fs
    if ($durationSec -le 0.0) { return 0.0 }
    return (60.0 * $peaks / $durationSec)
}

if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Input file not found: $InputPath"
}

if (-not (Test-Path -LiteralPath $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$rows = Import-Csv -LiteralPath $InputPath
if ($rows.Count -eq 0) {
    throw "Input CSV has no rows: $InputPath"
}

$runSummary = [System.Collections.Generic.List[object]]::new()
$cleaned = [System.Collections.Generic.List[object]]::new()

for ($run = 0; $run -lt $rows.Count; $run++) {
    $row = $rows[$run]
    $irSeries = Parse-NumericSeries -Text ([string]$row.IR_Samples)
    $redSeries = Parse-NumericSeries -Text ([string]$row.Red_Samples)
    $axSeries = Parse-NumericSeries -Text ([string]$row.Accel_X_Samples)
    $aySeries = Parse-NumericSeries -Text ([string]$row.Accel_Y_Samples)
    $azSeries = Parse-NumericSeries -Text ([string]$row.Accel_Z_Samples)

    [double[]]$ir = $irSeries.Values.ToArray()
    [double[]]$red = $redSeries.Values.ToArray()
    [double[]]$ax = $axSeries.Values.ToArray()
    [double[]]$ay = $aySeries.Values.ToArray()
    [double[]]$az = $azSeries.Values.ToArray()

    $n = [Math]::Min($ir.Length, $red.Length)
    for ($i = 0; $i -lt $n; $i++) {
        $accelX = if ($i -lt $ax.Length) { [Math]::Round($ax[$i], 3) } else { [double]::NaN }
        $accelY = if ($i -lt $ay.Length) { [Math]::Round($ay[$i], 3) } else { [double]::NaN }
        $accelZ = if ($i -lt $az.Length) { [Math]::Round($az[$i], 3) } else { [double]::NaN }
        $cleaned.Add([pscustomobject]@{
            run_index = $run + 1
            sample_index = $i
            raw_ir = [Math]::Round($ir[$i], 3)
            raw_red = [Math]::Round($red[$i], 3)
            accel_x = $accelX
            accel_y = $accelY
            accel_z = $accelZ
        })
    }

    $irMean = Get-Mean -Data $ir
    $redMean = Get-Mean -Data $red
    $irStd = Get-Std -Data $ir -Mean $irMean
    $redStd = Get-Std -Data $red -Mean $redMean
    $axMean = Get-Mean -Data $ax
    $ayMean = Get-Mean -Data $ay
    $azMean = Get-Mean -Data $az
    $axStd = Get-Std -Data $ax -Mean $axMean
    $ayStd = Get-Std -Data $ay -Mean $ayMean
    $azStd = Get-Std -Data $az -Mean $azMean
    $corr = Get-Correlation -A $ir -B $red
    $bpm = Get-NaiveBpm -Data $ir -Fs $SampleRateHz

    $sameLen = ($ir.Length -eq $red.Length)
    $accelSameLen = ($ax.Length -eq $n -and $ay.Length -eq $n -and $az.Length -eq $n)
    $countOk = ($ir.Length -ge 280 -and $red.Length -ge 280 -and $sameLen)
    $bpmOk = ($bpm -ge 40.0 -and $bpm -le 160.0)
    $corrOk = ([Math]::Abs($corr) -ge 0.20)
    $badOk = ($irSeries.BadTokens -eq 0 -and $redSeries.BadTokens -eq 0)
    $accelBadOk = ($axSeries.BadTokens -eq 0 -and $aySeries.BadTokens -eq 0 -and $azSeries.BadTokens -eq 0)

    $score = 0
    if ($countOk) { $score += 30 }
    if ($bpmOk) { $score += 30 }
    if ($corrOk) { $score += 25 }
    if ($badOk) { $score += 15 }

    $label = if ($score -ge 75) { 'LIKELY_PPG' } elseif ($score -ge 45) { 'MIXED' } else { 'UNRELIABLE' }

    $runSummary.Add([pscustomobject]@{
        run_index = $run + 1
        verdict = $label
        score = $score
        ir_valid = $ir.Length
        ir_total_tokens = $irSeries.TotalTokens
        ir_bad_tokens = $irSeries.BadTokens
        red_valid = $red.Length
        red_total_tokens = $redSeries.TotalTokens
        red_bad_tokens = $redSeries.BadTokens
        accel_x_valid = $ax.Length
        accel_y_valid = $ay.Length
        accel_z_valid = $az.Length
        accel_lengths_match_ppg = $accelSameLen
        accel_x_bad_tokens = $axSeries.BadTokens
        accel_y_bad_tokens = $aySeries.BadTokens
        accel_z_bad_tokens = $azSeries.BadTokens
        accel_bad_tokens_ok = $accelBadOk
        lengths_match = $sameLen
        corr_ir_red = [Math]::Round($corr, 6)
        est_bpm_ir = [Math]::Round($bpm, 2)
        ir_mean = [Math]::Round($irMean, 3)
        ir_std = [Math]::Round($irStd, 3)
        red_mean = [Math]::Round($redMean, 3)
        red_std = [Math]::Round($redStd, 3)
        accel_x_mean = [Math]::Round($axMean, 3)
        accel_y_mean = [Math]::Round($ayMean, 3)
        accel_z_mean = [Math]::Round($azMean, 3)
        accel_x_std = [Math]::Round($axStd, 3)
        accel_y_std = [Math]::Round($ayStd, 3)
        accel_z_std = [Math]::Round($azStd, 3)
    })
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$summaryPath = Join-Path $OutDir ("ppg_run_summary_{0}.csv" -f $stamp)
$cleanPath = Join-Path $OutDir ("ppg_clean_samples_{0}.csv" -f $stamp)

$runSummary | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$cleaned | Export-Csv -LiteralPath $cleanPath -NoTypeInformation -Encoding UTF8

Write-Host ("Input:   {0}" -f $InputPath)
Write-Host ("Summary: {0}" -f $summaryPath)
Write-Host ("Clean:   {0}" -f $cleanPath)
Write-Host ""
Write-Host "Run verdicts:"
$runSummary | Format-Table run_index, verdict, score, ir_valid, red_valid, accel_x_valid, accel_y_valid, accel_z_valid, ir_bad_tokens, red_bad_tokens, accel_x_bad_tokens, accel_y_bad_tokens, accel_z_bad_tokens, corr_ir_red, est_bpm_ir -AutoSize
