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

"""API-surface completeness tests.

The Python binding promises `argtypes`/`restype` for EVERY function of the C
ABI — this pins that promise: the set of functions declared in the header must
equal the set of PROTOTYPES registered by gsml3parser._library, in both
directions (no missing prototype, no stale one). It also pins the two
single-source-of-truth facts of Phase 0: GSML3_ABI_VERSION == EXPECTED_ABI,
and gsml3_version() == repo-root VERSION file content.
"""

import os
import re
from pathlib import Path

# tests/ -> python/ -> bindings/ -> repository root (bindings/python/conftest.py
# puts the package directory itself on sys.path).
REPO_ROOT = Path(__file__).resolve().parents[3]


def _declared_functions() -> set:
    """Parse the C header for every `GSML3_C_API ... gsml3_xxx(` declaration."""
    env_override = os.environ.get("GSML3PARSER_HEADER")
    if env_override:
        header = Path(env_override)
    else:
        header = REPO_ROOT / "include" / "gsml3parser" / "gsml3parser_c.h"
    text = header.read_text(encoding="utf-8")
    return {m.group(1) for m in re.finditer(
        r"^[\s*]*GSML3_C_API[\w\*\s]+\b(gsml3_\w+)\s*\(", text, re.M)}


def test_prototypes_cover_the_whole_c_abi():
    from gsml3parser import _library
    decl = _declared_functions()
    proto = set(_library.PROTOTYPES)
    assert len(decl) == 238, f"header inventory drifted: expected 238 GSML3_C_API functions, found {len(decl)}"
    missing = decl - proto
    assert not missing, f"missing prototypes: {sorted(missing)}"
    stale = proto - decl
    assert not stale, f"stale prototypes: {sorted(stale)}"


def test_abi_version_matches_library():
    import gsml3parser as g
    from gsml3parser import _library
    assert g.abi_version() == _library.EXPECTED_ABI == 1


def test_version_matches_root_version_file():
    """Single source of truth: the library
    reports the version that CMake read from the repo-root VERSION file."""
    import gsml3parser as g
    vfile = REPO_ROOT / "VERSION"
    assert vfile.is_file(), f"missing {vfile} (Phase 0 artifact)"
    assert g.version() == vfile.read_text(encoding="utf-8").strip() == g.__version__
