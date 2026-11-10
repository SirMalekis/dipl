# Сборка и запуск модульных тестов ядра.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw "Visual Studio не найдена" }
$install = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvars)) { throw "vcvars64.bat не найден" }

$sources = @(
    "tests\test_core.cpp",
    "qos.cpp", "topology.cpp", "traffic.cpp", "metrics.cpp",
    "config.cpp", "engine.cpp", "experiments.cpp", "csv_export.cpp"
) -join " "

$command = "call `"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /O2 /I. $sources /Fe:tests\test_core.exe /Fotests\\"
cmd /c $command
if ($LASTEXITCODE -ne 0) { throw "Компиляция тестов завершилась с кодом $LASTEXITCODE" }

& ".\tests\test_core.exe"
exit $LASTEXITCODE
