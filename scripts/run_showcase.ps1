# Пять showcase-режимов: от катастрофы к устойчивой сети.
# Запуск: .\scripts\run_showcase.ps1
# Без скриншотов: .\scripts\run_showcase.ps1 -NoScreenshots
[CmdletBinding()]
param(
    [switch]$NoScreenshots
)

$ErrorActionPreference = "Stop"
$env:PYTHONUTF8 = "1"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$exe = Join-Path $root "x64\Release\netsim.exe"
if (!(Test-Path $exe)) {
    throw "Сначала соберите ядро: .\build.ps1"
}
$python = (Get-Command python -ErrorAction Stop).Source
$base = "results\showcase"
New-Item -ItemType Directory -Force $base | Out-Null
$img = Join-Path $root "docs\img"
New-Item -ItemType Directory -Force $img | Out-Null

$common = @(
    "--num-nodes", "50",
    "--T", "60",
    "--qos-class", "1",
    "--er-p", "0.1",
    "--ba-m", "2",
    "--seed", "42"
)

$stages = @(
    @{
        Name = "01_catastrophe"
        Label = "1. Катастрофа"
        Args = @(
            "--lambda", "5",
            "--resources", "10",
            "--slots", "1",
            "--demand", "20000000",
            "--access", "100000000",
            "--core", "100000000",
            "--buffer", "10"
        )
    },
    @{
        Name = "02_severe"
        Label = "2. Тяжёлый"
        Args = @(
            "--lambda", "2",
            "--resources", "30",
            "--slots", "1",
            "--demand", "10000000",
            "--access", "100000000",
            "--core", "500000000",
            "--buffer", "20"
        )
    },
    @{
        Name = "03_baseline"
        Label = "3. Базовый"
        Args = @(
            "--lambda", "1",
            "--resources", "200",
            "--slots", "3",
            "--demand", "2000000",
            "--access", "1000000000",
            "--core", "10000000000",
            "--buffer", "100"
        )
    },
    @{
        Name = "04_good"
        Label = "4. Хороший"
        Args = @(
            "--lambda", "0.2",
            "--resources", "500",
            "--slots", "5",
            "--demand", "1000000",
            "--access", "1000000000",
            "--core", "10000000000",
            "--buffer", "300"
        )
    },
    @{
        Name = "05_excellent"
        Label = "5. Отличный"
        Args = @(
            "--lambda", "0.02",
            "--resources", "2000",
            "--slots", "10",
            "--demand", "500000",
            "--access", "10000000000",
            "--core", "40000000000",
            "--buffer", "1000"
        )
    }
)

foreach ($stage in $stages) {
    $dir = Join-Path $base $stage.Name
    if (Test-Path $dir) {
        Remove-Item -LiteralPath $dir -Recurse -Force
    }
    New-Item -ItemType Directory -Force $dir | Out-Null
    Write-Host ("=== {0} ===" -f $stage.Label)

    # Сначала timeseries (для вкладки «Динамика»), затем topology
    # (10 прогонов для устойчивых средних и ДИ).
    $tsArgs = @(
        "run", "--scenario", "timeseries",
        "--topology", "erdos_renyi",
        "--sims", "1", "--sample-points", "120"
    ) + $common + $stage.Args + @("--out", $dir, "--progress")
    & $exe @tsArgs | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw ("timeseries {0}: код {1}" -f $stage.Name, $LASTEXITCODE)
    }

    $topoArgs = @(
        "run", "--scenario", "topology",
        "--topology", "erdos_renyi",
        "--topologies", "erdos_renyi",
        "--sims", "10", "--sample-points", "80"
    ) + $common + $stage.Args + @("--out", $dir, "--progress")
    & $exe @topoArgs | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw ("topology {0}: код {1}" -f $stage.Name, $LASTEXITCODE)
    }

    & $python (Join-Path $root "plot_results.py") $dir | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw ("plot_results {0}: код {1}" -f $stage.Name, $LASTEXITCODE)
    }

    if (-not $NoScreenshots) {
        $shot = Join-Path $img ("showcase_{0}.png" -f $stage.Name)
        & $python (Join-Path $root "run_gui.py") --open-run $dir `
            --tab "Динамика" --screenshot $shot | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw ("screenshot {0}: код {1}" -f $stage.Name, $LASTEXITCODE)
        }
        Write-Host ("  screenshot: {0}" -f $shot)
    }
}

& $python (Join-Path $root "scripts\make_showcase_summary.py")
if ($LASTEXITCODE -ne 0) { throw "make_showcase_summary: ошибка" }

& $python (Join-Path $root "tests\showcase_check.py")
if ($LASTEXITCODE -ne 0) { throw "showcase_check: пороговые проверки не пройдены" }

Write-Host "SHOWCASE_OK"
