# Быстрый smoke-тест CLI: сценарии без --progress не должны падать.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$exe = Join-Path $root "x64\Release\netsim.exe"
if (!(Test-Path $exe)) {
    throw "Сначала соберите ядро: .\build.ps1"
}

$out = Join-Path $root "test_output\cli_structure"
Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
& $exe run --scenario structure --topologies star --num-nodes 4 --out $out
if ($LASTEXITCODE -ne 0) { throw "structure без --progress: код $LASTEXITCODE" }

$out = Join-Path $root "test_output\cli_topology"
Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
& $exe run --scenario topology --topologies star --num-nodes 4 --sims 1 `
    --T 5 --sample-points 10 --out $out
if ($LASTEXITCODE -ne 0) { throw "topology без --progress: код $LASTEXITCODE" }

& $exe selftest
if ($LASTEXITCODE -ne 0) { throw "selftest: код $LASTEXITCODE" }

Write-Host "CLI_SMOKE_OK"
