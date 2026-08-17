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

#include "gsml3parser/stack/procedure_types.h"

namespace gsml3parser::procedure {

std::string_view procedureTypeName(ProcedureType type)
{
    switch (type) {
        case ProcedureType::LocationUpdate:      return "LocationUpdate";
        case ProcedureType::Authentication:      return "Authentication";
        case ProcedureType::CipheringMode:       return "CipheringMode";
        case ProcedureType::CallSetup_MO:        return "CallSetup_MO";
        case ProcedureType::CallSetup_MT:        return "CallSetup_MT";
        case ProcedureType::ChannelAssignment:   return "ChannelAssignment";
        case ProcedureType::Handover:            return "Handover";
        case ProcedureType::Paging:              return "Paging";
        case ProcedureType::CMServiceRequest:       return "CMServiceRequest";
        case ProcedureType::IMSIDetach:             return "IMSIDetach";
        case ProcedureType::CallRelease:            return "CallRelease";
        case ProcedureType::PeriodicLocationUpdate: return "PeriodicLocationUpdate";
        case ProcedureType::Unknown:                return "?";
    }
    return "?";
}

std::string_view procedureStateName(ProcedureState state)
{
    switch (state) {
        case ProcedureState::Initiated:       return "Initiated";
        case ProcedureState::InProgress:      return "InProgress";
        case ProcedureState::WaitingExternal: return "WaitingExternal";
        case ProcedureState::Completed:       return "Completed";
        case ProcedureState::Failed:          return "Failed";
        case ProcedureState::TimedOut:        return "TimedOut";
    }
    return "Unknown";
}

} // namespace gsml3parser::procedure
