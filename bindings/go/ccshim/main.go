// Copyright 2026 momentics <momentics@gmail.com>
// Copyright libgsml3parser contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ccshim is a thin pass-through wrapper that drives a clang for the
// x86_64-pc-windows-msvc target through the Windows cgo path. Go builds cgo
// exclusively with its GNU toolchain flavor, and on windows/amd64 it both
// compiles AND links by invoking $CC directly, so one wrapper sees two kinds
// of argument lists:
//
//  1. Compile steps (-c present): Go appends the GNU-only flag -mthreads to
//     every cgo compilation; clang for the windows-msvc target rejects it. The
//     flag only selects libstdc++ threading support, which has no meaning for
//     this project's pure-C cgo preambles, so the wrapper removes it.
//
//  2. Link steps (no -c): Go passes GNU-ld-style inputs that lld-link in its
//     MSVC mode cannot consume — a linker script handed over positionally or
//     via -T (fix_debug_gdb_scripts.ld: a GNU-ld gdb-script fix-up with no PE
//     counterpart), and the MinGW import libraries -lmingwex/-lmingw32, which
//     no windows-msvc LLVM installation ships. The objects in this build are
//     COFF linked against the MSVC CRT (which clang selects for the target),
//     so nothing in a pure-C cgo build references MinGW-only symbols; the
//     wrapper drops exactly those GNU-ld-only inputs and forwards everything
//     else untouched, letting lld-link finish an MSVC-style link.
//
// Arguments, working directory, and stdio are otherwise passed straight
// through and the real compiler's exit code is propagated.
//
// The real compiler is taken from CCSHIM_TARGET (a full path, or a name found
// on PATH; default "clang"). Built by the unified gate when this clang+lld
// fallback toolchain is selected for cgo.
package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"strings"
)

func main() {
	target := os.Getenv("CCSHIM_TARGET")
	if target == "" {
		target = "clang"
	}

	args := make([]string, 0, len(os.Args)-1)
	for _, a := range os.Args[1:] {
		if a == "-mthreads" {
			continue
		}
		args = append(args, a)
	}
	if !containsFlag(args, "-c") {
		// No -c: this is the link step driven by Go — normalize it (package
		// comment explains exactly which inputs are dropped and why).
		args = normalizeLinkArgs(args)
	}

	cmd := exec.Command(target, args...)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	if err := cmd.Run(); err != nil {
		var exitErr *exec.ExitError
		if errors.As(err, &exitErr) {
			os.Exit(exitErr.ExitCode())
		}
		fmt.Fprintf(os.Stderr, "ccshim: cannot run compiler %q: %v\n", target, err)
		os.Exit(127)
	}
}

// containsFlag reports whether args carries the exact flag name.
func containsFlag(args []string, flag string) bool {
	for _, a := range args {
		if a == flag {
			return true
		}
	}
	return false
}

// normalizeLinkArgs strips the GNU-ld-only inputs described in the package
// comment; everything else is preserved verbatim and in order.
func normalizeLinkArgs(args []string) []string {
	out := make([]string, 0, len(args))
	for i := 0; i < len(args); i++ {
		a := args[i]
		switch {
		case a == "-T":
			// Drop the -T marker and, when present, the .ld script operand.
			if i+1 < len(args) && strings.HasSuffix(args[i+1], ".ld") {
				i++
			}
		case strings.HasPrefix(a, "-T") && strings.HasSuffix(a, ".ld"):
			// Joined -T<file> form.
		case !strings.HasPrefix(a, "-") && strings.HasSuffix(a, ".ld"):
			// Linker script handed over as a positional input.
		case a == "-lmingwex", a == "-lmingw32":
			// MinGW import libraries: absent in windows-msvc toolchains and
			// unneeded for MSVC-CRT objects (see package comment).
		default:
			out = append(out, a)
		}
	}
	return out
}
