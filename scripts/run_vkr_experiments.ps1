# Полный набор экспериментов ВКР.
# Запуск: .\scripts\run_vkr_experiments.ps1
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
$exe = Join-Path $root "x64\Release\dipl.exe"
if (!(Test-Path $exe)) { throw "Сначала соберите ядро: .\build.ps1" }

$base = Join-Path $root "results\vkr"
New-Item -ItemType Directory -Force $base | Out-Null

$runs = @(
    @{
        Name = "base_topology"
        Args = @("run", "--scenario", "topology", "--num-nodes", "50",
                 "--sims", "30", "--T", "150", "--sample-points", "200",
                 "--lambda", "1.0", "--resources", "200", "--slots", "3",
                 "--qos-class", "1", "--demand", "2000000",
                 "--out", "$base\base_topology", "--progress")
    },
    @{
        Name = "stress_topology"
        Args = @("run", "--scenario", "topology", "--num-nodes", "50",
                 "--sims", "20", "--T", "150", "--sample-points", "200",
                 "--lambda", "1.5", "--resources", "120", "--slots", "2",
                 "--qos-class", "1", "--demand", "10000000",
                 "--access", "100000000", "--core", "1000000000",
                 "--buffer", "20", "--out", "$base\stress_topology", "--progress")
    },
    @{
        Name = "base_timeseries"
        Args = @("run", "--scenario", "timeseries",
                 "--topologies", "star,ring,full_mesh,erdos_renyi,barabasi_albert",
                 "--num-nodes", "50", "--sims", "1", "--T", "150",
                 "--sample-points", "300", "--lambda", "1.0",
                 "--resources", "200", "--slots", "3", "--qos-class", "1",
                 "--demand", "2000000", "--out", "$base\base_timeseries", "--progress")
    },
    @{
        Name = "sweep_lambda"
        Args = @("run", "--scenario", "sweep", "--topologies", "star,ring,full_mesh,erdos_renyi,barabasi_albert",
                 "--num-nodes", "50", "--sims", "20", "--T", "120",
                 "--sample-points", "160", "--resources", "200", "--slots", "3",
                 "--qos-class", "1", "--demand", "2000000",
                 "--sweep-params", "lambda", "--sweep-lambda", "0.5,5.0,6",
                 "--out", "$base\sweep_lambda", "--progress")
    },
    @{
        Name = "sweep_resources"
        Args = @("run", "--scenario", "sweep", "--topologies", "star,ring,full_mesh,erdos_renyi,barabasi_albert",
                 "--num-nodes", "50", "--sims", "20", "--T", "120",
                 "--sample-points", "160", "--lambda", "1.5", "--slots", "2",
                 "--qos-class", "1", "--demand", "2000000",
                 "--sweep-params", "resources", "--sweep-resources", "20,300,6",
                 "--out", "$base\sweep_resources", "--progress")
    },
    @{
        Name = "strategy"
        Args = @("run", "--scenario", "strategy", "--topologies", "star,ring,full_mesh,erdos_renyi,barabasi_albert",
                 "--num-nodes", "50", "--sims", "30", "--T", "150",
                 "--sample-points", "200", "--lambda", "1.5", "--slots", "2",
                 "--qos-class", "1", "--demand", "2000000",
                 "--resource-levels", "20,40,80,150",
                 "--out", "$base\strategy", "--progress")
    },
    @{
        Name = "attack"
        Args = @("run", "--scenario", "attack", "--topologies", "star,ring,full_mesh,erdos_renyi,barabasi_albert",
                 "--attack-strategies", "targeted,random,cascading",
                 "--num-nodes", "50", "--sims", "20", "--T", "150",
                 "--sample-points", "200", "--lambda", "1.2",
                 "--resources", "150", "--slots", "3", "--qos-class", "1",
                 "--demand", "2000000", "--out", "$base\attack", "--progress")
    }
)

$total = [Diagnostics.Stopwatch]::StartNew()
foreach ($run in $runs) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Write-Host ("=== {0} ===" -f $run.Name)
    & $exe @($run.Args) | Out-Null
    $code = $LASTEXITCODE
    $sw.Stop()
    Write-Host ("{0}: код {1}, {2:N1} c" -f $run.Name, $code, $sw.Elapsed.TotalSeconds)
}
$total.Stop()
Write-Host ("ВСЕГО: {0:N1} c" -f $total.Elapsed.TotalSeconds)
