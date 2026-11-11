# Сборка C++-ядра NetSim.
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

function Find-MSBuild {
    $command = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($install) {
            $candidate = Join-Path $install "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path $candidate) { return $candidate }
        }
    }
    throw "MSBuild не найден. Установите Visual Studio с компонентом C++."
}

$msbuild = Find-MSBuild
Write-Host "MSBuild: $msbuild"
& $msbuild "netsim.vcxproj" "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m /v:minimal
if ($LASTEXITCODE -ne 0) { throw "Сборка завершилась с кодом $LASTEXITCODE" }

$exe = Join-Path $root "x64\$Configuration\netsim.exe"
Write-Host "Готово: $exe"
