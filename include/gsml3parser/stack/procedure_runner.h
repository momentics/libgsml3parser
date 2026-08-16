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

/// Placeholder for ProcedureRunner — full implementation in Phase 3.
///
/// This stub allows SubscriberSession to include a ProcedureRunner member
/// during Phase 2 development. The class provides no functionality and will
/// be replaced with the complete ProcedureRunner + ProcedureFactory API.
///
/// Thread safety: NOT thread-safe.
/// Memory: sizeof(ProcedureRunner) == sizeof(vtable pointer).
#pragma once

namespace gsml3parser {

/// Stub — will be implemented in Phase 3 (Procedure Framework).
class ProcedureRunner {
public:
    ProcedureRunner() = default;
    ~ProcedureRunner() = default;
};

} // namespace gsml3parser
