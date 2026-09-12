# =============================================================================
# Flash QSPI stores — Resource + Model + Calibration (one script)
# =============================================================================
# Programs external NOR via direct QSPI register writes (Raytac non-DK pins).
# Each store is flashed in its own J-Link session (reliable QSPI state).
#
# Defaults (matches 51_qspi.overlayinc / PARTITION_LAYOUT.md):
#   resource     resource.bin      @ 0x0F6000  (512 KB primary)
#   model        model_pack.bin    @ 0x1F6000  (512 KB primary)
#   calibration  calibration.bin   @ 0x2F6000  (64 KB)
#
# Usage:
#   .\flash_qspi_stores.ps1
#   .\flash_qspi_stores.ps1 -Targets resource,model
#   .\flash_qspi_stores.ps1 -Targets calibration -CalibrationFile path\to\cal.bin
#   .\flash_qspi_stores.ps1 -ModelVariant pack -ModelSlot primary
#   .\flash_qspi_stores.ps1 -RequireAll
#   .\flash_qspi_stores.ps1 -EraseOnly                 # erase selected partitions only
#   .\flash_qspi_stores.ps1 -EraseChip                 # wipe entire 8 MB QSPI (no program)
# =============================================================================

param(
    [string]$Targets = "resource,model,calibration",
    [string]$BuildDir = "build_sdk_v330",
    [ValidateSet("wearable", "pulse", "pack")]
    [string]$ModelVariant = "pack",
    [ValidateSet("primary", "secondary")]
    [string]$ModelSlot = "primary",
    [string]$CalibrationFile = "",
    [string]$JLinkSerialNumber = "",
    [switch]$RequireAll,
    [switch]$EraseOnly,
    [switch]$EraseChip,
    [switch]$NoReset
)

$ErrorActionPreference = "Stop"

$CHUNK_SIZE  = 0x10000
$SECTOR_SIZE = 0x1000
$RAM_BUFFER  = 0x20000000
# Verify must NOT use the loadbin staging address (0x20000000): after reset the
# app reclaims low RAM and mem32 there shows garbage (e.g. BB00C8C8). Use a
# high scratch region — same as the previously working flash_resource.ps1.
$VERIFY_RAM  = 0x20030000
# MX25R6435F — 8 MB external NOR (size bits in DT = 67108864)
$QSPI_CHIP_SIZE = 0x800000
# Per-block erase wait (64 KB). Same as store flash; chip-wide LEN=All is unreliable here.
$BLOCK_ERASE_SLEEP_MS = 1500

$StoreDefs = @{
    resource = @{
        Name       = "Resource"
        Offset     = 0x0F6000
        PartSize   = 0x80000
        MagicWords = @("41504958")
        MagicLabel = "XIPA"
        DefaultRel = "resource.bin"
    }
    model = @{
        Name            = "Model"
        OffsetPrimary   = 0x1F6000
        OffsetSecondary = 0x276000
        PartSize        = 0x80000
        MagicWords      = @("504C444D", "4C444D47")
        MagicLabel      = "MDLP/GMDL"
        DefaultRel      = $null
    }
    calibration = @{
        Name       = "Calibration"
        Offset     = 0x2F6000
        PartSize   = 0x10000
        MagicWords = @("43414C42")
        MagicLabel = "CALB"
        DefaultRel = "calibration.bin"
    }
}

$QSPI = @{
    TASKS_ACTIVATE   = "0x40029000"
    TASKS_READSTART  = "0x40029004"
    TASKS_WRITESTART = "0x40029008"
    TASKS_ERASESTART = "0x4002900C"
    EVENTS_READY     = "0x40029100"
    ENABLE           = "0x40029500"
    READ_SRC         = "0x40029504"
    READ_DST         = "0x40029508"
    READ_CNT         = "0x4002950C"
    WRITE_DST        = "0x40029510"
    WRITE_SRC        = "0x40029514"
    WRITE_CNT        = "0x40029518"
    ERASE_PTR        = "0x4002951C"
    ERASE_LEN        = "0x40029520"
    PSEL_SCK         = "0x40029524"
    PSEL_CSN         = "0x40029528"
    PSEL_IO0         = "0x40029530"
    PSEL_IO1         = "0x40029534"
    PSEL_IO2         = "0x40029538"
    PSEL_IO3         = "0x4002953C"
    IFCONFIG0        = "0x40029544"
    IFCONFIG1        = "0x40029600"
    CINSTRCONF       = "0x40029634"
    CINSTRDAT0       = "0x40029638"
}

$PIN = @{
    SCK = "0x00000013"
    CSN = "0x00000011"
    IO0 = "0x00000016"
    IO1 = "0x00000017"
    IO2 = "0x00000015"
    IO3 = "0x00000020"
}

function Add-QspiInit([System.Text.StringBuilder]$sb) {
    [void]$sb.AppendLine("h")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_SCK) $($PIN.SCK)")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_CSN) $($PIN.CSN)")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_IO0) $($PIN.IO0)")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_IO1) $($PIN.IO1)")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_IO2) $($PIN.IO2)")
    [void]$sb.AppendLine("w4 $($QSPI.PSEL_IO3) $($PIN.IO3)")
    [void]$sb.AppendLine("w4 $($QSPI.IFCONFIG0) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.IFCONFIG1) 0xF0000080")
    [void]$sb.AppendLine("w4 $($QSPI.ENABLE) 0x00000001")
    [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.TASKS_ACTIVATE) 0x00000001")
    [void]$sb.AppendLine("sleep 200")
}

function Get-LeU32Hex([byte[]]$bytes) {
    if ($bytes.Length -lt 4) { return "00000000" }
    return ("{0:X2}{1:X2}{2:X2}{3:X2}" -f $bytes[3], $bytes[2], $bytes[1], $bytes[0])
}

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName

$requested = @($Targets.Split(",") | ForEach-Object { $_.Trim().ToLower() } | Where-Object { $_ })
if ($requested.Count -eq 0 -or $requested -contains "all") {
    $requested = @("resource", "model", "calibration")
}
foreach ($t in $requested) {
    if (-not $StoreDefs.ContainsKey($t)) {
        Write-Host "Unknown target '$t'. Use: resource, model, calibration (or all)." -ForegroundColor Red
        exit 1
    }
}

function Resolve-StoreBin([string]$Key) {
    $def = $StoreDefs[$Key]
    if ($Key -eq "model") {
        if ($ModelVariant -eq "pack") {
            return (Join-Path $ProjectRoot "$BuildDir\model_pack.bin")
        }
        return (Join-Path $ProjectRoot "$BuildDir\glucose_model_$ModelVariant.bin")
    }
    if ($Key -eq "calibration" -and $CalibrationFile) {
        if ([System.IO.Path]::IsPathRooted($CalibrationFile)) { return $CalibrationFile }
        return (Join-Path $ProjectRoot $CalibrationFile)
    }
    return (Join-Path $ProjectRoot "$BuildDir\$($def.DefaultRel)")
}

function Get-StoreOffset([string]$Key) {
    $def = $StoreDefs[$Key]
    if ($Key -eq "model") {
        if ($ModelSlot -eq "secondary") { return [int]$def.OffsetSecondary }
        return [int]$def.OffsetPrimary
    }
    return [int]$def.Offset
}

$jobs = @()
if (-not $EraseChip) {
    foreach ($key in $requested) {
        $bin = Resolve-StoreBin $key
        $def = $StoreDefs[$key]
        if (-not (Test-Path $bin)) {
            if ($RequireAll) {
                Write-Host "  Missing required bin for ${key}: $bin" -ForegroundColor Red
                exit 1
            }
            Write-Host "  SKIP $($def.Name): bin not found ($bin)" -ForegroundColor Yellow
            continue
        }
        $resolved = (Resolve-Path $bin).Path
        $size = (Get-Item $resolved).Length
        if ($size -gt $def.PartSize) {
            Write-Host "  $($def.Name) bin ($size) exceeds partition ($($def.PartSize))" -ForegroundColor Red
            exit 1
        }
        $hdr = [System.IO.File]::ReadAllBytes($resolved)
        $expectMagic = Get-LeU32Hex $hdr
        $jobs += [pscustomobject]@{
            Key         = $key
            Name        = $def.Name
            Bin         = $resolved
            Size        = [int]$size
            Offset      = Get-StoreOffset $key
            PartSize    = [int]$def.PartSize
            Magics      = $def.MagicWords
            MagicLbl    = $def.MagicLabel
            ExpectMagic = $expectMagic
        }
    }

    if ($jobs.Count -eq 0) {
        Write-Host "Nothing to flash." -ForegroundColor Red
        exit 1
    }
}

$jlinkCmd = $null
foreach ($path in @(
    "C:\Program Files\SEGGER\JLink\JLink.exe",
    "C:\Program Files (x86)\SEGGER\JLink\JLink.exe"
)) {
    if (Test-Path $path) { $jlinkCmd = $path; break }
}
if (-not $jlinkCmd) {
    Write-Host "  J-Link Commander not found." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host " Flash QSPI stores (direct register programming)" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
if ($EraseChip) {
    Write-Host "  CHIP ERASE only: entire $($QSPI_CHIP_SIZE / 1MB) MB QSPI" -ForegroundColor Yellow
    Write-Host "    (image-1, scratch, Resource, Model, calibration, NVS, records, app)" -ForegroundColor Yellow
} else {
    foreach ($j in $jobs) {
        $kb = [math]::Round($j.Size / 1024, 1)
        Write-Host ("  {0,-12} 0x{1:X6}  {2,7} KB  magic={3}  {4}" -f `
            $j.Name, $j.Offset, $kb, $j.ExpectMagic, $j.Bin) -ForegroundColor Green
    }
}
Write-Host ""

function Invoke-EraseEntireQspi {
    param([bool]$DoReset)

    $tempDir = Join-Path $PSScriptRoot "_qspi_chip_erase"
    if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $blockCount = [int]($QSPI_CHIP_SIZE / $CHUNK_SIZE)  # 128 × 64 KB = 8 MB

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine("r")
    [void]$sb.AppendLine("// ===== FULL QSPI ERASE: $blockCount x 64KB blocks =====")
    Add-QspiInit $sb

    [void]$sb.AppendLine("// JEDEC")
    [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.CINSTRDAT0) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.CINSTRCONF) 0x0000349F")
    [void]$sb.AppendLine("sleep 100")
    [void]$sb.AppendLine("mem32 $($QSPI.CINSTRDAT0) 1")

    # Walk the whole die with 64 KB block erase (proven path on this board).
    # Nordic ERASE.LEN=All (chip 0xC7) is not reliable with our direct-register setup.
    for ($i = 0; $i -lt $blockCount; $i++) {
        $addr = $i * $CHUNK_SIZE
        [void]$sb.AppendLine("// block $i @ 0x$($addr.ToString('X6'))")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_PTR) 0x$($addr.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_LEN) 0x00000001")  # 64 KB
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_ERASESTART) 0x00000001")
        [void]$sb.AppendLine("sleep $BLOCK_ERASE_SLEEP_MS")
        [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
        # Re-init periodically — long scripts can drop QSPI state after many ops
        if ((($i + 1) % 32) -eq 0) {
            Add-QspiInit $sb
        }
    }

    # Verify key offsets are blank (poison scratch, then QSPI read)
    Add-QspiInit $sb
    $checkOffs = @(
        @{ Addr = 0x000000; Dst = $VERIFY_RAM;      Label = "image-1@0" },
        @{ Addr = 0x0F6000; Dst = $VERIFY_RAM + 0x10; Label = "resource@0xF6000" },
        @{ Addr = 0x1F6000; Dst = $VERIFY_RAM + 0x20; Label = "model@0x1F6000" },
        @{ Addr = 0x2F6000; Dst = $VERIFY_RAM + 0x30; Label = "calib@0x2F6000" }
    )
    foreach ($c in $checkOffs) {
        [void]$sb.AppendLine("w4 0x$($c.Dst.ToString('X8')) 0xDEADBEEF")
        [void]$sb.AppendLine("w4 $($QSPI.READ_SRC) 0x$($c.Addr.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.READ_DST) 0x$($c.Dst.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.READ_CNT) 0x00000004")
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_READSTART) 0x00000001")
        [void]$sb.AppendLine("sleep 150")
        [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
        [void]$sb.AppendLine("mem32 0x$($c.Dst.ToString('X8')) 1")
    }

    if ($DoReset) {
        [void]$sb.AppendLine("r")
        [void]$sb.AppendLine("g")
    }
    [void]$sb.AppendLine("exit")

    $jlinkScript = Join-Path $tempDir "chip_erase.jlink"
    $sb.ToString() | Out-File -FilePath $jlinkScript -Encoding ASCII -Force

    $outFile = Join-Path $tempDir "jlink_log.txt"
    $consoleFile = Join-Path $tempDir "jlink_stdout.txt"
    $errFile = Join-Path $tempDir "jlink_stderr.txt"

    $estSec = [math]::Ceiling(($blockCount * $BLOCK_ERASE_SLEEP_MS) / 1000) + 15
    $t0 = Get-Date
    Write-Host ("  [{0}] Full QSPI erase: {1} x 64KB (~{2}s, do not unplug)..." -f `
        $t0.ToString('HH:mm:ss'), $blockCount, $estSec) -ForegroundColor Yellow

    $jlinkArgs = @(
        "-device", "nRF52840_xxAA",
        "-if", "SWD",
        "-speed", "4000",
        "-autoconnect", "1",
        "-CommandFile", $jlinkScript,
        "-Log", $outFile
    )
    if ($JLinkSerialNumber) {
        $jlinkArgs = @("-SelectEmuBySN", $JLinkSerialNumber) + $jlinkArgs
    }

    $proc = Start-Process -FilePath $jlinkCmd -ArgumentList $jlinkArgs -NoNewWindow -PassThru `
        -RedirectStandardOutput $consoleFile -RedirectStandardError $errFile

    $barWidth = 40
    while (-not $proc.HasExited) {
        $elapsed = ((Get-Date) - $t0).TotalSeconds
        $percent = [math]::Min(100, [int](($elapsed / [math]::Max(1, $estSec)) * 100))
        $filled = [int](($percent / 100) * $barWidth)
        $bar = ('=' * [math]::Max(0, $filled - 1))
        if ($filled -gt 0) { $bar += '>' }
        $bar = $bar.PadRight($barWidth, ' ')
        Write-Host -NoNewline ("`r    Progress: [$bar] {0}% ({1:N0}s)  " -f $percent, $elapsed)
        Start-Sleep -Milliseconds 500
    }
    $proc.WaitForExit()
    Start-Sleep -Milliseconds 300
    $dt = ((Get-Date) - $t0).TotalSeconds
    Write-Host ("`r    Progress: [{0}] 100% ({1:N1}s)  " -f ('=' * $barWidth), $dt) -ForegroundColor Green

    $log = ""
    foreach ($f in @($outFile, $consoleFile, $errFile)) {
        if (Test-Path $f) {
            $part = (Get-Content $f -Raw -ErrorAction SilentlyContinue) -as [string]
            if ($part) { $log += $part + "`n" }
        }
    }
    $logKeep = Join-Path $PSScriptRoot "_qspi_last_chip_erase.log"
    if ($log) { $log | Out-File -FilePath $logKeep -Encoding utf8 -Force }

    $ok = $true
    $jedec = [regex]::Match($log, "40029638\s*=\s*([0-9A-Fa-f]+)")
    if ($jedec.Success -and ($jedec.Groups[1].Value -notmatch "1728C2")) {
        Write-Host "    JEDEC unexpected: 0x$($jedec.Groups[1].Value)" -ForegroundColor Red
        $ok = $false
    }

    foreach ($c in $checkOffs) {
        $hex = $c.Dst.ToString("X8")
        $matches = [regex]::Matches($log, "(?im)(?:0x)?$hex\s*[=:]\s*([0-9A-Fa-f]{8})")
        if ($matches.Count -eq 0) {
            Write-Host "    $($c.Label): verify readback not found" -ForegroundColor Red
            $ok = $false
            continue
        }
        $word0 = $matches[$matches.Count - 1].Groups[1].Value.ToUpper()
        if ($word0 -eq "DEADBEEF") {
            Write-Host "    $($c.Label): QSPI read did not land (still DEADBEEF)" -ForegroundColor Red
            $ok = $false
        } elseif ($word0 -eq "FFFFFFFF") {
            Write-Host "    $($c.Label): 0xFFFFFFFF (erased)" -ForegroundColor Green
        } else {
            Write-Host "    $($c.Label): 0x$word0 (expected FFFFFFFF)" -ForegroundColor Red
            $ok = $false
        }
    }

    if ($ok) {
        Remove-Item $logKeep -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "    Log: $logKeep" -ForegroundColor DarkGray
    }
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    return $ok
}

function Invoke-FlashOneStore {
    param($Job, [bool]$DoReset)

    $tempDir = Join-Path $PSScriptRoot ("_qspi_{0}_chunks" -f $Job.Key)
    if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $chunks = @()
    if (-not $EraseOnly) {
        $bytes = [System.IO.File]::ReadAllBytes($Job.Bin)
        $n = [int][math]::Ceiling($bytes.Length / $CHUNK_SIZE)
        for ($i = 0; $i -lt $n; $i++) {
            $off = $i * $CHUNK_SIZE
            $len = [math]::Min($CHUNK_SIZE, $bytes.Length - $off)
            $chunkPath = Join-Path $tempDir ("chunk_{0}.bin" -f $i)
            $piece = New-Object byte[] $len
            [Array]::Copy($bytes, $off, $piece, 0, $len)
            [System.IO.File]::WriteAllBytes($chunkPath, $piece)
            $chunks += [pscustomobject]@{
                Path       = $chunkPath
                FlashOff   = [int]($Job.Offset + $off)
                ByteCount  = [int]$len
                Index      = $i
            }
        }
    }

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine("r")
    [void]$sb.AppendLine("// ===== $($Job.Name) @ 0x$($Job.Offset.ToString('X6')) =====")
    Add-QspiInit $sb

    [void]$sb.AppendLine("// JEDEC")
    [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.CINSTRDAT0) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.CINSTRCONF) 0x0000349F")
    [void]$sb.AppendLine("sleep 100")
    [void]$sb.AppendLine("mem32 $($QSPI.CINSTRDAT0) 1")

    $blocks = [int][math]::Floor($Job.PartSize / $CHUNK_SIZE)
    $remain = $Job.PartSize - ($blocks * $CHUNK_SIZE)
    $sectors = if ($remain -gt 0) { [int][math]::Ceiling($remain / $SECTOR_SIZE) } else { 0 }
    for ($i = 0; $i -lt $blocks; $i++) {
        $addr = $Job.Offset + ($i * $CHUNK_SIZE)
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_PTR) 0x$($addr.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_LEN) 0x00000001")
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_ERASESTART) 0x00000001")
        [void]$sb.AppendLine("sleep 1500")
        [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
    }
    for ($s = 0; $s -lt $sectors; $s++) {
        $addr = $Job.Offset + ($blocks * $CHUNK_SIZE) + ($s * $SECTOR_SIZE)
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_PTR) 0x$($addr.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_LEN) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_ERASESTART) 0x00000001")
        [void]$sb.AppendLine("sleep 500")
        [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
    }

    if (-not $EraseOnly) {
        # BUGFIX (false-positive verification, 2026-08-28): the previous
        # version wrote each chunk and immediately read it back before
        # moving to the next chunk. That interleaved read-right-after-write
        # gave a false "verified byte-exact" pass for every chunk while the
        # store was still corrupted underneath — a *later*, standalone,
        # freshly re-initialized read of the very same address (the
        # existing small magic-word check at the end) showed completely
        # different data. That pattern — reads immediately following a
        # write on the same address look fine, a cold read later doesn't —
        # points at the read-right-after-write not reflecting genuinely
        # committed flash content yet. All writes now happen first, exactly
        # as before; verification reads happen afterward, in their own pass,
        # each preceded by its own Add-QspiInit (matching the one existing
        # check that actually caught problems reliably), so every
        # verification read is a cold read of settled flash, not a
        # same-breath echo of the write that just happened.
        foreach ($c in $chunks) {
            $pathUnix = $c.Path -replace '\\', '/'
            [void]$sb.AppendLine("// chunk $($c.Index) $($c.ByteCount)B -> 0x$($c.FlashOff.ToString('X8'))")
            [void]$sb.AppendLine("loadbin `"$pathUnix`" 0x$($RAM_BUFFER.ToString('X8'))")
            Add-QspiInit $sb
            [void]$sb.AppendLine("w4 $($QSPI.WRITE_DST) 0x$($c.FlashOff.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.WRITE_SRC) 0x$($RAM_BUFFER.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.WRITE_CNT) 0x$($c.ByteCount.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
            [void]$sb.AppendLine("w4 $($QSPI.TASKS_WRITESTART) 0x00000001")
            # This sleep is still a guess, not a real wait-for-completion —
            # J-Link Commander's script format has no conditionals, so a
            # true poll-until-ready loop isn't possible here. Widened
            # substantially as a mitigation; the real backstop is the cold
            # readback verification pass below, done after every write in
            # this job has already completed and settled.
            $waitMs = [math]::Max(4000, [int][math]::Ceiling($c.ByteCount / 128) * 15)
            [void]$sb.AppendLine("sleep $waitMs")
            [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")

            $c | Add-Member -NotePropertyName ReadbackPath `
                -NotePropertyValue (Join-Path $tempDir ("chunk_{0}_readback.bin" -f $c.Index))
        }

        # Cold verification pass — runs only after every chunk above has
        # been written.
        #
        # BUGFIX (2026-09-08): this used to call Add-QspiInit (pin config +
        # ENABLE + TASKS_ACTIVATE) before EVERY chunk's read, same as the
        # write loop above. Real-world result: chunk 0's read always
        # succeeded, every chunk after it always failed — 100% reproducible
        # across repeated runs, for both the Resource and Model stores.
        # That deterministic pattern (not random/intermittent, which is
        # what a genuine timing or hardware issue would look like) points
        # at the repeated re-activation itself: a full job was issuing
        # Add-QspiInit up to 14 times (once for erase, once per write chunk,
        # once per read chunk, once for the final check) — re-enabling and
        # re-activating an already-active QSPI peripheral over and over is
        # not how it's meant to be used; normally it's configured once and
        # then just gets task triggers (WRITESTART/READSTART/ERASESTART)
        # afterward. The old, simple, pre-existing small magic-word check
        # (still below, at the very end) only ever called Add-QspiInit
        # once per whole job and was the one thing that stayed reliable
        # through all of this — consistent with "repeated re-activation is
        # the problem," not "a fresh init before every read is required."
        #
        # Fixed: one Add-QspiInit here, before the whole read loop, not
        # inside it. Each chunk still gets its own READ_SRC/DST/CNT and
        # its own settle sleep — only the peripheral re-activation is no
        # longer repeated per chunk.
        [void]$sb.AppendLine("// ===== cold readback verification, all chunks =====")
        Add-QspiInit $sb
        foreach ($c in $chunks) {
            $rbPathUnix = $c.ReadbackPath -replace '\\', '/'
            [void]$sb.AppendLine("sleep 250")
            [void]$sb.AppendLine("w4 $($QSPI.READ_SRC) 0x$($c.FlashOff.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.READ_DST) 0x$($VERIFY_RAM.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.READ_CNT) 0x$($c.ByteCount.ToString('X8'))")
            [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
            [void]$sb.AppendLine("w4 $($QSPI.TASKS_READSTART) 0x00000001")
            [void]$sb.AppendLine("sleep $([math]::Max(2000, [int][math]::Ceiling($c.ByteCount / 128) * 15))")
            [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
            [void]$sb.AppendLine("SaveBin `"$rbPathUnix`" 0x$($VERIFY_RAM.ToString('X8')) 0x$($c.ByteCount.ToString('X8'))")
        }

        # Verify into high scratch RAM (not loadbin @ 0x20000000).
        [void]$sb.AppendLine("// VERIFY_$($Job.Key.ToUpper()) expect $($Job.ExpectMagic)")
        Add-QspiInit $sb
        [void]$sb.AppendLine("w4 $($QSPI.READ_SRC) 0x$($Job.Offset.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.READ_DST) 0x$($VERIFY_RAM.ToString('X8'))")
        [void]$sb.AppendLine("w4 $($QSPI.READ_CNT) 0x00000010")
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_READSTART) 0x00000001")
        [void]$sb.AppendLine("sleep 200")
        [void]$sb.AppendLine("mem32 $($QSPI.EVENTS_READY) 1")
        [void]$sb.AppendLine("mem32 0x$($VERIFY_RAM.ToString('X8')) 4")
    }

    if ($DoReset) {
        [void]$sb.AppendLine("r")
        [void]$sb.AppendLine("g")
    }
    [void]$sb.AppendLine("exit")

    $jlinkScript = Join-Path $tempDir "flash.jlink"
    $sb.ToString() | Out-File -FilePath $jlinkScript -Encoding ASCII -Force

    $outFile = Join-Path $tempDir "jlink_log.txt"
    $consoleFile = Join-Path $tempDir "jlink_stdout.txt"
    $errFile = Join-Path $tempDir "jlink_stderr.txt"

    $eraseBlocks = [math]::Ceiling($Job.PartSize / $CHUNK_SIZE)
    $est = ($eraseBlocks * 2) + ($chunks.Count * 3)
    $t0 = Get-Date
    Write-Host ("  [{0}] {1}: erase+program ({2} chunks, ~{3}s)..." -f `
        $t0.ToString('HH:mm:ss'), $Job.Name, $chunks.Count, $est) -ForegroundColor Cyan

    $jlinkArgs = @(
        "-device", "nRF52840_xxAA",
        "-if", "SWD",
        "-speed", "4000",
        "-autoconnect", "1",
        "-CommandFile", $jlinkScript,
        "-Log", $outFile
    )
    if ($JLinkSerialNumber) {
        $jlinkArgs = @("-SelectEmuBySN", $JLinkSerialNumber) + $jlinkArgs
    }

    # Avoid Start-Process stdout redirect (can drop the final mem32 on longer runs).
    # J-Link -Log captures the full session, including verify readback.
    $proc = Start-Process -FilePath $jlinkCmd -ArgumentList $jlinkArgs -NoNewWindow -PassThru `
        -RedirectStandardOutput $consoleFile -RedirectStandardError $errFile

    $barWidth = 40
    while (-not $proc.HasExited) {
        $elapsed = ((Get-Date) - $t0).TotalSeconds
        $percent = [math]::Min(100, [int](($elapsed / [math]::Max(1, $est)) * 100))
        $filled = [int](($percent / 100) * $barWidth)
        $bar = ('=' * [math]::Max(0, $filled - 1))
        if ($filled -gt 0) { $bar += '>' }
        $bar = $bar.PadRight($barWidth, ' ')
        Write-Host -NoNewline ("`r    Progress: [$bar] {0}% ({1:N0}s)  " -f $percent, $elapsed)
        Start-Sleep -Milliseconds 200
    }
    $proc.WaitForExit()
    Start-Sleep -Milliseconds 300
    $dt = ((Get-Date) - $t0).TotalSeconds
    Write-Host ("`r    Progress: [{0}] 100% ({1:N1}s)  " -f ('=' * $barWidth), $dt) -ForegroundColor Green

    $log = ""
    foreach ($f in @($outFile, $consoleFile, $errFile)) {
        if (Test-Path $f) {
            $part = (Get-Content $f -Raw -ErrorAction SilentlyContinue) -as [string]
            if ($part) { $log += $part + "`n" }
        }
    }
    $logKeep = Join-Path $PSScriptRoot ("_qspi_last_{0}.log" -f $Job.Key)
    if ($log) { $log | Out-File -FilePath $logKeep -Encoding utf8 -Force }

    $ok = $true
    $jedec = [regex]::Match($log, "40029638\s*=\s*([0-9A-Fa-f]+)")
    if ($jedec.Success -and ($jedec.Groups[1].Value -notmatch "1728C2")) {
        Write-Host "    JEDEC unexpected: 0x$($jedec.Groups[1].Value)" -ForegroundColor Red
        $ok = $false
    }

    $readyBad = 0
    foreach ($m in [regex]::Matches($log, "40029100\s*=\s*([0-9A-Fa-f]+)")) {
        if ($m.Groups[1].Value -ne "00000001") { $readyBad++ }
    }
    if ($readyBad -gt 0) {
        Write-Host "    WARNING: $readyBad QSPI ops without READY=1" -ForegroundColor Yellow
        $ok = $false
    }

    # BUGFIX (silent partial-corruption): byte-exact comparison of every
    # chunk actually written, not just a 16-byte magic-word spot check.
    # This is the check that would have caught the 2026-08-27 Model-pack
    # corruption — that run had zero ready-bit warnings and a correct
    # magic word, and was still corrupted somewhere in the body.
    if (-not $EraseOnly) {
        $badChunks = @()
        foreach ($c in $chunks) {
            if (-not (Test-Path $c.ReadbackPath)) {
                Write-Host ("    $($Job.Name) chunk $($c.Index): readback file missing " +
                    "(SaveBin may not have run — check $logKeep)") -ForegroundColor Red
                $badChunks += $c.Index
                continue
            }
            $srcHash = (Get-FileHash -Algorithm SHA256 -Path $c.Path).Hash
            $rbHash  = (Get-FileHash -Algorithm SHA256 -Path $c.ReadbackPath).Hash
            if ($srcHash -ne $rbHash) {
                Write-Host ("    $($Job.Name) chunk $($c.Index) @ 0x$($c.FlashOff.ToString('X8')): " +
                    "READBACK MISMATCH — flash does not contain what was written") -ForegroundColor Red
                $badChunks += $c.Index
            }
        }
        if ($badChunks.Count -gt 0) {
            Write-Host ("    $($Job.Name): {0} of {1} chunk(s) failed full-content verification " +
                "— do not trust this store, re-run this job" -f $badChunks.Count, $chunks.Count) `
                -ForegroundColor Red
            $ok = $false
        } else {
            Write-Host "    $($Job.Name): all $($chunks.Count) chunk(s) verified byte-exact" -ForegroundColor Green
        }
    }

    if (-not $EraseOnly) {
        $vr = $VERIFY_RAM.ToString("X8")
        # J-Link variants: "20030000 = ...." or "0x20030000 = ...." or "20030000: ...."
        $vm = [regex]::Match(
            $log,
            "(?im)(?:0x)?$vr\s*[=:]\s*([0-9A-Fa-f]{8}(?:[ \t]+[0-9A-Fa-f]{8}){0,7})"
        )
        # Fallback: last occurrence of expected magic as a standalone word near end of log
        if (-not $vm.Success) {
            $vm = [regex]::Match(
                $log,
                "(?im)(?:0x)?$vr\s*[=:]\s*(.+)$"
            )
        }
        if ($vm.Success) {
            $data = ($vm.Groups[1].Value -split '[\r\n]')[0].Trim()
            # Normalize: keep leading hex words only
            $hexWords = [regex]::Matches($data, "[0-9A-Fa-f]{8}") | ForEach-Object { $_.Value }
            if ($hexWords.Count -eq 0) {
                Write-Host "    $($Job.Name): verify line had no hex words: $data" -ForegroundColor Red
                $ok = $false
            } else {
                $word0 = $hexWords[0].ToUpper()
                $data = ($hexWords | Select-Object -First 4) -join ' '
                if ($word0 -eq "DEADBEEF") {
                    Write-Host "    $($Job.Name): QSPI verify read did not land (still DEADBEEF)" -ForegroundColor Red
                    $ok = $false
                } else {
                    $magicOk = ($word0 -eq $Job.ExpectMagic.ToUpper())
                    if (-not $magicOk) {
                        foreach ($magic in $Job.Magics) {
                            if ($word0 -eq $magic.ToUpper()) { $magicOk = $true; break }
                        }
                    }
                    if ($magicOk) {
                        Write-Host "    $($Job.Name): magic $word0 OK ($($Job.MagicLbl))" -ForegroundColor Green
                    } else {
                        Write-Host "    $($Job.Name): magic mismatch got $word0 expect $($Job.ExpectMagic) ($($Job.MagicLbl))" -ForegroundColor Red
                        Write-Host "           readback: $data" -ForegroundColor DarkGray
                        $ok = $false
                    }
                }
            }
        } else {
            # Last-resort: expected magic word appears after VERIFY_ marker in the log
            $em = $Job.ExpectMagic.ToUpper()
            if ($log -match "(?is)VERIFY_$($Job.Key.ToUpper()).*?\b$em\b") {
                Write-Host "    $($Job.Name): magic $em OK ($($Job.MagicLbl)) [log fallback]" -ForegroundColor Green
            } else {
                Write-Host "    $($Job.Name): verify readback not found (addr 0x$vr)" -ForegroundColor Red
                $tail = ($log -split "`r?`n" | Where-Object { $_.Trim() -ne "" } | Select-Object -Last 15) -join "`n    "
                if ($tail) {
                    Write-Host "    J-Link tail:`n    $tail" -ForegroundColor DarkGray
                }
                Write-Host "    Full log: $logKeep" -ForegroundColor DarkGray
                $ok = $false
            }
        }
    }

    if ($ok) {
        Remove-Item $logKeep -Force -ErrorAction SilentlyContinue
    }
    Remove-Item $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    return $ok
}

$allOk = $true

if ($EraseChip) {
    $ok = Invoke-EraseEntireQspi -DoReset:(-not $NoReset)
    if ($ok) {
        Write-Host ""
        Write-Host "  Entire QSPI chip erased successfully." -ForegroundColor Green
        exit 0
    }
    Write-Host ""
    Write-Host "  Chip erase completed with errors — see above." -ForegroundColor Red
    exit 1
}

for ($ji = 0; $ji -lt $jobs.Count; $ji++) {
    $job = $jobs[$ji]
    $isLast = ($ji -eq ($jobs.Count - 1))
    $doReset = $isLast -and (-not $NoReset)
    $ok = Invoke-FlashOneStore -Job $job -DoReset:$doReset
    if (-not $ok) { $allOk = $false }
    Write-Host ""
}

if ($allOk) {
    Write-Host "  QSPI stores programmed successfully." -ForegroundColor Green
    exit 0
}
Write-Host "  Completed with errors — see above." -ForegroundColor Red
exit 1
