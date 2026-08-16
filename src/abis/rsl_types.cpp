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

#include "gsml3parser/abis/rsl_types.h"

namespace gsml3parser {

std::string_view rslDiscriminatorName(RSLDiscriminator disc)
{
    switch (disc) {
        case RSLDiscriminator::RLL:              return "RLL";
        case RSLDiscriminator::CommonChannel:    return "CCHAN";
        case RSLDiscriminator::DedicatedChannel: return "DCHAN";
        case RSLDiscriminator::TRX:              return "TRX";
        case RSLDiscriminator::IPAccess:         return "IPAccess";
    }
    return "?";
}

std::string_view rslIEName(RSL_IE ie)
{
    switch (ie) {
        case RSL_IE::ChanNr:           return "ChanNr";
        case RSL_IE::LinkIdent:        return "LinkIdent";
        case RSL_IE::ActType:          return "ActType";
        case RSL_IE::ChanMode:         return "ChanMode";
        case RSL_IE::EncrInfo:         return "EncrInfo";
        case RSL_IE::BSPower:          return "BSPower";
        case RSL_IE::MSPower:          return "MSPower";
        case RSL_IE::HandoRef:         return "HandoRef";
        case RSL_IE::SACCHInfo:        return "SACCHInfo";
        case RSL_IE::Cause:            return "Cause";
        case RSL_IE::AccessDelay:      return "AccessDelay";
        case RSL_IE::ReqReference:     return "ReqReference";
        case RSL_IE::FrameNumber:      return "FrameNumber";
        case RSL_IE::MSIdentity:       return "MSIdentity";
        case RSL_IE::PagingGroup:      return "PagingGroup";
        case RSL_IE::ChanNeeded:       return "ChanNeeded";
        case RSL_IE::FullImmAssInfo:   return "FullImmAssInfo";
        case RSL_IE::L3Info:           return "L3Info";
        case RSL_IE::SysInfoType:      return "SysInfoType";
        case RSL_IE::FullBCCHInfo:     return "FullBCCHInfo";
        case RSL_IE::MeasResNr:        return "MeasResNr";
        case RSL_IE::UplinkMeas:       return "UplinkMeas";
        case RSL_IE::L1Info:           return "L1Info";
        case RSL_IE::TimingAdvance:    return "TimingAdvance";
        case RSL_IE::MSTimingOffset:   return "MSTimingOffset";
        case RSL_IE::ReleaseMode:      return "ReleaseMode";
    }
    return "?";
}

std::string_view rslErrorCauseName(RSLErrorCause cause)
{
    switch (cause) {
        case RSLErrorCause::NormalUnspecified:         return "NormalUnspecified";
        case RSLErrorCause::RRUnavailable:             return "RRUnavailable";
        case RSLErrorCause::EquipmentFailure:          return "EquipmentFailure";
        case RSLErrorCause::ServiceOptionUnavailable:  return "ServiceOptionUnavailable";
        case RSLErrorCause::ServiceOptionUnimplemented: return "ServiceOptionUnimplemented";
        case RSLErrorCause::ResourceUnavailable:       return "ResourceUnavailable";
        case RSLErrorCause::IEContentError:            return "IEContentError";
        case RSLErrorCause::MandatoryIEMissing:        return "MandatoryIEMissing";
        case RSLErrorCause::OptionalIEError:           return "OptionalIEError";
        case RSLErrorCause::MessageSeqError:           return "MessageSeqError";
        case RSLErrorCause::MessageTypeError:          return "MessageTypeError";
        case RSLErrorCause::DiscriminatorError:        return "DiscriminatorError";
        case RSLErrorCause::ProtocolError:             return "ProtocolError";
        case RSLErrorCause::EncryptionUnimplemented:   return "EncryptionUnimplemented";
    }
    return "?";
}

} // namespace gsml3parser
