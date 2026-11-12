# Builds and runs the offline tests: the press classification of include\PressReferee.h against models of Simple Dodge's
# Sprint hook and the game's (test_press_referee.cpp), and the HUD sneak eye after a crouch change, include\StealthMeter.h
# against a model of the game's stealth meter (test_stealth_meter.cpp). Needs clang++ (LLVM) on PATH, or set $env:CXX to
# another C++17 compiler that takes the same arguments. Exit code 0 = every test passed.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$cxx = if ($env:CXX) { $env:CXX } else { 'clang++' }
$failed = 0
foreach ($name in 'test_press_referee', 'test_stealth_meter') {
    $out = Join-Path ([IO.Path]::GetTempPath()) "sprintorsneak-$name.exe"
    & $cxx -std=c++17 -I (Join-Path $here '..\include') (Join-Path $here "$name.cpp") -o $out
    if ($LASTEXITCODE -ne 0) { Write-Output "$name : build failed"; $failed++; continue }
    & $out
    if ($LASTEXITCODE -ne 0) { $failed++ }
}
exit $failed
