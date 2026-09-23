// Copyright 2026 momentics <momentics@gmail.com>; MIT license; see /COPYING at the repository root.

module github.com/momentics/libgsml3parser/bindings/go

// Runtime dependencies: none — the standard library only (cgo + sync).
// Tests use the stdlib testing package; -race is supported on all GOOS/GOARCH
// targets this project builds (linux/amd64, windows/amd64, darwin/arm64+amd64).
//
// Version: this Go module deliberately carries NO product version (go modules
// of an internal binding do not use semver tags); the repository-root VERSION
// file is the single source of truth (planK decision #13) — there is no value
// here that can drift out of sync with it.
go 1.21
