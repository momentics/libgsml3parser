# verify.ps1 — full build + test + example verification for libgsml3parser.
#
# Usage:
#   pwsh scripts/verify.ps1                 # Release (default)
#   pwsh scripts/verify.ps1 -Config Debug   # Debug
#
# Runs from the repository root (parent of this script's directory).
# Fails fast (non-zero exit) on any configure/build/test/example failure.

param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# Always run from the repository root so relative paths are stable.
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

Write-Host "=== [1/4] Configure ($Config) ===" -ForegroundColor Cyan
cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -G "Visual Studio 18 2026"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)" }

Write-Host "=== [2/4] Build ($Config, max parallel) ===" -ForegroundColor Cyan
cmake --build build --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build failed (exit $LASTEXITCODE)" }

Write-Host "=== [3/4] Unit tests (ctest) ===" -ForegroundColor Cyan
ctest --test-dir build -C $Config --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "ctest failed (exit $LASTEXITCODE)" }

Write-Host "=== [4/4] Examples ===" -ForegroundColor Cyan
$exDir = Join-Path $RepoRoot "build\examples\$Config"
if (-not (Test-Path $exDir)) { throw "Examples directory not found: $exDir" }

Get-ChildItem $exDir\*.exe | ForEach-Object {
    $args = @()
    # example_parse_file requires a hex string or file argument.
    if ($_.Name -eq "example_parse_file.exe") { $args = @("060D00") }
    # example_multithread accepts optional (threads, iterations); run with defaults.
    Write-Host "  Running $($_.Name) $args"
    & $_.FullName @args
    if ($LASTEXITCODE -ne 0) { throw "Example failed: $($_.Name) (exit $LASTEXITCODE)" }
}

Write-Host "=== VERIFY PASSED: build + tests + examples ($Config) ===" -ForegroundColor Green
