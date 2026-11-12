# Builds and runs the offline tests (the press classification of include\PressReferee.h against models of Simple Dodge's
# Sprint hook and the game's). Needs clang++ (LLVM) on PATH, or set $env:CXX to another C++17 compiler that takes the
# same arguments. Exit code 0 = every test passed.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$cxx = if ($env:CXX) { $env:CXX } else { 'clang++' }
$out = Join-Path ([IO.Path]::GetTempPath()) 'sprintorsneak-test-press-referee.exe'
& $cxx -std=c++17 -I (Join-Path $here '..\include') (Join-Path $here 'test_press_referee.cpp') -o $out
if ($LASTEXITCODE -ne 0) { Write-Output 'build failed'; exit 1 }
& $out
exit $LASTEXITCODE
