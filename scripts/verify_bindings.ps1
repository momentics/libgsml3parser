# Copyright 2026 momentics <momentics@gmail.com>
# Copyright libgsml3parser contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# verify_bindings.ps1 - unified build/test gate for the FFI bindings (bindings/).
#
# Usage:
#   pwsh scripts/verify_bindings.ps1                  # all three languages, Release
#   pwsh scripts/verify_bindings.ps1 -Only go         # a single language
#   pwsh scripts/verify_bindings.ps1 -SkipLib         # reuse an existing build_bindings
#   pwsh scripts/verify_bindings.ps1 -Generator "Visual Studio 18 2026"   # pin the CMake generator
#
# Runs from the repository root. Steps: repo invariants (VERSION single source of
# truth) -> core shared library (build_bindings, flattened into build_bindings/bin)
# -> Python (pytest + example) -> Go (vet + test -race + example) -> Rust
# (clippy + test + example). Fails fast with a non-zero exit code on any failure.
# Windows cgo: auto-selects a clang + lld-link pair (PATH or Visual Studio's
# LLD/LLVM component) and routes it through the bindings/go/ccshim pass-through
# (the Go toolchain appends a GNU-only flag that windows-msvc clang rejects).
# Generator policy: with no -Generator CMake picks its default for the platform
# (portable on CI runners); local runs pin "Visual Studio 18 2026" to match the
# core gate scripts/verify.ps1.

param(
    [string]$Config = "Release",
    [ValidateSet("python", "go", "rust")] [string[]]$Only,
    [switch]$SkipLib,
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$isWin = ($IsWindows -or $env:OS -eq "Windows_NT")
$binDir = Join-Path $RepoRoot "build_bindings/bin"

# ── Repo invariants: the product version has ONE home (root VERSION file) ───
$versionFile = Join-Path $RepoRoot "VERSION"
if (-not (Test-Path $versionFile)) { throw "Missing root VERSION file — single source of truth for the product version" }
# pwsh (PowerShell 7) reads UTF-8 and strips a BOM during decode; .Trim() then
# removes the trailing newline. The byte-level "no BOM" rule is enforced by the
# final-gate file checks, not here.
$productVersion = (Get-Content $versionFile -Raw).Trim()
if ($productVersion -notmatch '^\d+\.\d+\.\d+$') { throw "Malformed VERSION file: '$productVersion' (expected one semver line X.Y.Z, UTF-8 without BOM)" }
# The root CMakeLists.txt is wired to read the version from that file; a lost
# wiring means the single source of truth is broken.
if (-not (Select-String -Path (Join-Path $RepoRoot "CMakeLists.txt") -Pattern 'file\(READ\s+"\$\{CMAKE_CURRENT_LIST_DIR\}/VERSION"' -Quiet)) {
    throw "CMakeLists.txt no longer reads the product version from the root VERSION file (single-source wiring lost)"
}
# Cargo cannot load a version from a file — its manifests carry literals that
# must match. Collect the workspace-root manifest plus one manifest per top-level
# member directory (bounded walk: never descends into the cargo build tree).
$rustRoot = Join-Path $RepoRoot "bindings/rust"
$manifests = @()
$wsManifest = Join-Path $rustRoot "Cargo.toml"
if (Test-Path $wsManifest) { $manifests += $wsManifest }
Get-ChildItem $rustRoot -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne "target" } | ForEach-Object {
    $memberManifest = Join-Path $_.FullName "Cargo.toml"
    if (Test-Path $memberManifest) { $manifests += $memberManifest }
}
foreach ($manifest in $manifests) {
    # PowerShell's Select-String MatchInfo exposes regex groups only through
    # .Matches[0].Groups (the wrapper itself has no .Groups property).
    $m = Select-String -Path $manifest -Pattern '^version\s*=\s*"([^"]+)"' | Select-Object -First 1
    if (-not $m) { continue }   # workspace-root manifest: no package version
    $manifestVersion = $m.Matches[0].Groups[1].Value
    if ($manifestVersion -ne $productVersion) {
        throw "Version drift (single source of truth = root VERSION): $manifest has $manifestVersion, VERSION has $productVersion — fix the manifest, not the other way round"
    }
}
Write-Host ("=== [0] repo invariants OK: product version {0} (root VERSION file) ===" -f $productVersion) -ForegroundColor Cyan

function Step([int]$i, [string]$msg) { Write-Host "=== [$i/4] $msg ===" -ForegroundColor Cyan }
function Need([string]$exe, [string]$what) {
    if (-not (Get-Command $exe -ErrorAction SilentlyContinue)) {
        throw "Missing required tool: '$exe' ($what). Install it and re-run this gate."
    }
}
$runLangs = @("python", "go", "rust"); if ($Only) { $runLangs = @($Only) }

# ── [1/4] C core shared library ────────────────────────────────────
if (-not $SkipLib) {
    Step 1 "Core shared library ($Config)"
    Need cmake "CMake >= 3.20 with a C++20 compiler"
    $cmakeArgs = @("-S", ".", "-B", "build_bindings", "-DBUILD_SHARED_LIBS=ON",
                   "-DBUILD_TESTS=OFF", "-DBUILD_EXAMPLES=OFF")
    # Generator policy: an explicit -Generator always wins (local pin, e.g.
    # "Visual Studio 18 2026"); otherwise let CMake auto-select the platform
    # default — GH runners may not carry a specific VS version. Note: $args is
    # a reserved automatic variable in PowerShell and must NOT be reused here.
    if ($Generator)      { $cmakeArgs += @("-G", $Generator) }
    elseif (-not $isWin) { $cmakeArgs += @("-DCMAKE_BUILD_TYPE=$Config") }   # single-config (Ninja/Make); a VS multi-config generator takes --config below instead
    & cmake @cmakeArgs; if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)" }
    # --config only exists for multi-config generators (VS); Unix single-config
    # generators take CMAKE_BUILD_TYPE at configure time and reject the flag.
    if ($isWin) { & cmake --build build_bindings --config $Config --parallel --target gsml3parser }
    else        { & cmake --build build_bindings --parallel --target gsml3parser }
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed (exit $LASTEXITCODE)" }
    # Flatten artifacts into one directory: identical link path for ctypes/cgo/Rust
    # on both platforms.
    New-Item -ItemType Directory -Force $binDir | Out-Null
    if ($isWin) {
        Copy-Item "build_bindings/$Config/gsml3parser.dll",
                  "build_bindings/$Config/gsml3parser.lib" -Destination $binDir -Force
    } else {
        Get-ChildItem "build_bindings/libgsml3parser.so*" | ForEach-Object {
            Copy-Item $_.FullName -Destination $binDir -Force
        }
    }
} else {
    Step 1 "Core shared library (skipped; reusing $binDir)"
    if (-not (Test-Path $binDir)) { throw "build_bindings/bin not found; run without -SkipLib" }
}

# Make the library discoverable for every consumer below.
$env:GSML3PARSER_LIB_DIR = $binDir                     # Rust build.rs, Python loader
if ($isWin) { $env:Path = "$binDir;$($env:Path)" }     # Windows: .dll + cgo .lib discovery
else        { $env:LD_LIBRARY_PATH = "${binDir}:$($env:LD_LIBRARY_PATH)" }

# ── [2/4] Python binding (ctypes, stdlib) ─────────────────────────
if ($runLangs -contains "python") {
    Step 2 "Python: pytest + example"
    Need python "CPython >= 3.10 with the standard library"
    # pytest is a test-only dependency; the binding itself is standard-library
    # only. Install it for this user if it is missing (idempotent otherwise).
    python -c "import pytest" 2>$null
    if ($LASTEXITCODE -ne 0) {
        & python -m pip install --user pytest
        if ($LASTEXITCODE -ne 0) { throw "failed to install pytest (test-only dependency)" }
    }
    & python -m pytest (Join-Path $RepoRoot "bindings/python/tests") -q
    if ($LASTEXITCODE -ne 0) { throw "Python tests failed (exit $LASTEXITCODE)" }
    & python (Join-Path $RepoRoot "bindings/python/examples/bts_simulation.py")
    if ($LASTEXITCODE -ne 0) { throw "Python example failed (exit $LASTEXITCODE)" }
}

# ── [3/4] Go binding (cgo; needs a working C toolchain: gcc / clang+lld / MSVC) ─
if ($runLangs -contains "go") {
    Step 3 "Go: vet + test -race + example"
    Need go "Go >= 1.21 (CGO requires a C compiler)"

    if (-not $isWin -and [string]::IsNullOrEmpty($env:CC)) {
        # cgo's default is gcc; the Linux CI images install clang, so pin it there
        $env:CC = "clang"
    }
    if ($isWin -and [string]::IsNullOrEmpty($env:CC)) {
        # On Windows the Go toolchain appends the GNU-only flag -mthreads to every
        # cgo compile; a clang for the x86_64-pc-windows-msvc target rejects it.
        # Select a clang + lld-link pair (on PATH, or the LLD/LLVM component of a
        # Visual Studio install) and drive it through the pass-through shim at
        # bindings/go/ccshim, which drops -mthreads and links via lld-link. An
        # explicitly set CC is always respected as-is.
        $shimDir = Join-Path $env:TEMP "gsml3_ccshim"
        New-Item -ItemType Directory -Force $shimDir | Out-Null
        & go build -C (Join-Path $RepoRoot "bindings/go/ccshim") -o (Join-Path $shimDir "ccshim.exe") .
        if ($LASTEXITCODE -ne 0) { throw "failed to build the ccshim compiler wrapper (exit $LASTEXITCODE)" }
        $shimExe = Join-Path $shimDir "ccshim.exe"

        # Candidate bin directories: a PATH clang whose sibling lld-link exists,
        # then each Visual Studio install's LLD component (VC\Tools\Llvm).
        $clangBins = @()
        $cmdClang = Get-Command clang.exe -ErrorAction SilentlyContinue
        if ($cmdClang -and (Test-Path (Join-Path (Split-Path $cmdClang.Source) "lld-link.exe"))) {
            $clangBins += Split-Path $cmdClang.Source
        }
        $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            & $vswhere -latest -products * -property installationPath 2>$null | ForEach-Object {
                foreach ($arch in @("x64", "AMD64")) {
                    $llvm = Join-Path $_ "VC\Tools\Llvm\$arch\bin"
                    if ((Test-Path (Join-Path $llvm "clang.exe")) -and (Test-Path (Join-Path $llvm "lld-link.exe"))) { $clangBins += $llvm }
                }
            }
        }

        # Probe each candidate exactly like the Go build will: compile a trivial
        # C file with the stdlib flag set (incl. -mthreads, stripped by the shim)
        # and link it through lld-link. First pair that links wins.
        $probeDir = Join-Path $env:TEMP "gsml3_ccprobe"
        New-Item -ItemType Directory -Force $probeDir | Out-Null
        Set-Content (Join-Path $probeDir "probe.c") "int main(void) { return 0; }"
        $passing = @()
        foreach ($bin in ($clangBins | Where-Object { $_ } | Sort-Object -Unique)) {
            $env:CCSHIM_TARGET = (Join-Path $bin "clang.exe")
            $obj = Join-Path $probeDir ([IO.Path]::GetRandomFileName())
            & $shimExe -xc -c (Join-Path $probeDir "probe.c") -o $obj -O2 -Wall -Werror `
                -fno-stack-protector -Wdeclaration-after-statement -mthreads 2>$null
            if ($LASTEXITCODE -ne 0) { continue }
            & $shimExe -fuse-ld=lld $obj -o (Join-Path $probeDir "probe.exe") 2>$null
            Remove-Item $obj, (Join-Path $probeDir "probe.exe") -Force -ErrorAction SilentlyContinue
            if ($LASTEXITCODE -eq 0) { $passing += $bin }
        }
        if ($passing.Count -eq 0) {
            throw "No usable C toolchain found for Go cgo on this Windows machine (looked for a clang.exe + lld-link.exe pair on PATH and in the LLD/LLVM component of installed Visual Studios). Install that VS component, or set CC to a compiler whose flag set is understood by Go's cgo path."
        }
        $chosenBin = $passing[0]
        $env:CCSHIM_TARGET = (Join-Path $chosenBin "clang.exe")
        $env:CC = $shimExe
        $env:CGO_LDFLAGS = "-fuse-ld=lld"   # link through lld-link, not the MSVC linker
        $env:Path = "$chosenBin;$($env:Path)"
        Write-Host ("    cgo toolchain: clang + lld from {0} (via bindings/go/ccshim)" -f $chosenBin) -ForegroundColor DarkGray
    }
    $env:CGO_ENABLED = "1"   # the binding is cgo by design; explicit and per-run (not go env -w)
    Push-Location (Join-Path $RepoRoot "bindings/go")
    & go vet ./...;          if ($LASTEXITCODE -ne 0) { Pop-Location; throw "go vet failed" }
    & go test -count=1 -race ./...
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "Go tests failed (exit $LASTEXITCODE)" }
    & go run ./cmd/gsmexample
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "Go example failed (exit $LASTEXITCODE)" }
    Pop-Location
}

# ── [4/4] Rust binding (gsml3parser-sys + safe wrapper) ───────────
if ($runLangs -contains "rust") {
    Step 4 "Rust: clippy + test + example"
    Need cargo "Rust stable >= 1.75 with the clippy component (CI installs it explicitly)"
    # Strict lint pass: warnings are rejected in both binding crates.
    # (--manifest-path is a per-command option and follows the subcommand.)
    & cargo clippy --manifest-path bindings/rust/Cargo.toml --workspace -- -D warnings
    if ($LASTEXITCODE -ne 0) { throw "cargo clippy failed (exit $LASTEXITCODE)" }
    & cargo test --manifest-path bindings/rust/Cargo.toml --workspace
    if ($LASTEXITCODE -ne 0) { throw "cargo test failed (exit $LASTEXITCODE)" }
    & cargo run --manifest-path bindings/rust/Cargo.toml -p gsml3parser --example bts_simulation
    if ($LASTEXITCODE -ne 0) { throw "Rust example failed (exit $LASTEXITCODE)" }
}

Write-Host ("=== BINDINGS VERIFY PASSED: core lib + {0} ({1}) ===" -f ($runLangs -join ", "), $Config) `
    -ForegroundColor Green
