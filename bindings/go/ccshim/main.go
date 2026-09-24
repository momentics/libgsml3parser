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

// ccshim is a thin pass-through C-compiler wrapper for the Windows cgo path.
//
// With CGO enabled on Windows the Go toolchain unconditionally appends the
// GNU-style -mthreads flag to every C-compiler invocation. clang for the
// x86_64-pc-windows-msvc target — e.g. the LLVM component shipped inside
// Visual Studio (VC/Tools/Llvm) — rejects that flag as unsupported. The flag
// only selects libstdc++ threading support, which has no meaning for this
// project's pure-C cgo preambles, so the wrapper removes it and forwards
// everything else (arguments, working directory, stdio) to the real compiler
// untouched, propagating its exit code.
//
// The real compiler is taken from CCSHIM_TARGET (a full path, or a name found
// on PATH; default "clang"). Built as part of the unified gate when a
// windows-msvc-target clang is selected for cgo.
package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
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
