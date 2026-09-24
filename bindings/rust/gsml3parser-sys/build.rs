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

// Resolves the prebuilt shared library of the C++ core (gsml3parser) and emits
// cargo: link directives. Search order (first hit wins):
//   1. $GSML3PARSER_LIB_DIR      — explicit override (CI, custom installs)
//   2. <repo>/build_bindings/bin — flat artifacts dir created by the unified
//                                  gate script scripts/verify_bindings.ps1
//   3. <repo>/build/Release, <repo>/build  — core CMake build directories
//   4. <repo>/install_shared/lib           — cmake --install layout (CI)
// The repository root is three levels above this manifest directory.

use std::env;
use std::path::{Path, PathBuf};

fn repo_root() -> PathBuf {
    let manifest = env::var("CARGO_MANIFEST_DIR").expect("cargo sets CARGO_MANIFEST_DIR");
    Path::new(&manifest).ancestors().nth(3).expect("repo root").to_path_buf()
}

fn main() {
    let lib_names = [
        "gsml3parser.dll",
        "libgsml3parser.dylib",
        "libgsml3parser.so",
    ];
    let mut dirs: Vec<PathBuf> = vec![];
    if let Ok(dir) = env::var("GSML3PARSER_LIB_DIR") {
        dirs.push(PathBuf::from(dir));
    }
    let root = repo_root();
    for cand in [
        "build_bindings/bin",
        "build/Release",
        "build",
        "install_shared/lib",
    ] {
        dirs.push(root.join(cand));
    }

    let found = dirs.iter().find_map(|dir| {
        lib_names
            .iter()
            .find(|name| dir.join(*name).is_file())
            .map(|name| dir.join(name))
    });
    match found {
        Some(path) => {
            println!("cargo:rustc-link-search=native={}", path.parent().unwrap().display());
            // Dynamic link against the stable C ABI. On Windows the import lib
            // (gsml3parser.lib) must sit next to the dll; on Unix the .so is found by name.
            println!("cargo:rustc-link-lib=dylib=gsml3parser");
            println!("cargo:warning=gsml3parser-sys: linking {}", path.display());
        }
        None => panic!("gsml3parser-sys: could not find the shared library (gsml3parser.dll / libgsml3parser.so).\n\
                       Build it with: cmake -S <repo> -B build_bindings -DBUILD_SHARED_LIBS=ON && cmake --build build_bindings,\n\
                       or point $GSML3PARSER_LIB_DIR at a directory that contains it (scripts/verify_bindings.ps1 does this automatically)."),
    }
    // Re-run when the environment or any candidate library appears.
    println!("cargo:rerun-if-env-changed=GSML3PARSER_LIB_DIR");
    for dir in &dirs {
        println!("cargo:rerun-if-changed={}", dir.display());
    }
}
