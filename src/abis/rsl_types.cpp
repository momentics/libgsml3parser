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
        case RSLDiscriminator::Rll:              return "RLL";
        case RSLDiscriminator::CommonChannel:    return "CCHAN";
        case RSLDiscriminator::DedicatedChannel: return "DCHAN";
        case RSLDiscriminator::TrxManagement:    return "TRX";
        case RSLDiscriminator::Lcs:              return "LCS";
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
        case RSL_IE::BSPower:          return "BSPower";
        case RSL_IE::ChanIdent:        return "ChanIdent";
        case RSL_IE::ChanMode:         return "ChanMode";
        case RSL_IE::EncrInfo:         return "EncrInfo";
        case RSL_IE::FrameNumber:      return "FrameNumber";
        case RSL_IE::HandoRef:         return "HandoRef";
        case RSL_IE::L1Info:           return "L1Info";
        case RSL_IE::L3Info:           return "L3Info";
        case RSL_IE::MSIdentity:       return "MSIdentity";
        case RSL_IE::MSPower:          return "MSPower";
        case RSL_IE::PagingGroup:      return "PagingGroup";
        case RSL_IE::PagingLoad:       return "PagingLoad";
        case RSL_IE::PyhsContext:      return "PyhsContext";
        case RSL_IE::AccessDelay:      return "AccessDelay";
        case RSL_IE::RachLoad:         return "RachLoad";
        case RSL_IE::ReqReference:     return "ReqReference";
        case RSL_IE::ReleaseMode:      return "ReleaseMode";
        case RSL_IE::ResourceInfo:     return "ResourceInfo";
        case RSL_IE::RlmCause:         return "RlmCause";
        case RSL_IE::StartngTime:      return "StartngTime";
        case RSL_IE::TimingAdvance:    return "TimingAdvance";
        case RSL_IE::UplinkMeas:       return "UplinkMeas";
        case RSL_IE::Cause:            return "Cause";
        case RSL_IE::MeasResNr:        return "MeasResNr";
        case RSL_IE::MsgId:            return "MsgId";
        case RSL_IE::SysInfoType:      return "SysInfoType";
        case RSL_IE::MSPowerParam:     return "MSPowerParam";
        case RSL_IE::BSPowerParam:     return "BSPowerParam";
        case RSL_IE::PreprocParam:     return "PreprocParam";
        case RSL_IE::PreprocMeas:      return "PreprocMeas";
        case RSL_IE::ImmAssInfo:       return "ImmAssInfo";
        case RSL_IE::SmscbInfo:        return "SmscbInfo";
        case RSL_IE::MSTimingOffset:   return "MSTimingOffset";
        case RSL_IE::ErrMsg:           return "ErrMsg";
        case RSL_IE::FullBCCHInfo:     return "FullBCCHInfo";
        case RSL_IE::ChanNeeded:       return "ChanNeeded";
        case RSL_IE::CbCmdType:        return "CbCmdType";
        case RSL_IE::SmscbMsg:         return "SmscbMsg";
        case RSL_IE::FullImmAssInfo:   return "FullImmAssInfo";
        case RSL_IE::SacchInfo:        return "SacchInfo";
        case RSL_IE::CbchLoadInfo:     return "CbchLoadInfo";
        case RSL_IE::SmscbChanIndicator: return "SmscbChanIndicator";
        case RSL_IE::GroupCallRef:     return "GroupCallRef";
        case RSL_IE::GroupChanDesc:    return "GroupChanDesc";
        case RSL_IE::NchDrxInfo:       return "NchDrxInfo";
        case RSL_IE::CmdIndicator:     return "CmdIndicator";
        case RSL_IE::EmlppPrio:        return "EmlppPrio";
        case RSL_IE::Uic:              return "Uic";
        case RSL_IE::MainChanRef:      return "MainChanRef";
        case RSL_IE::MrConfig:         return "MrConfig";
        case RSL_IE::MrControl:        return "MrControl";
        case RSL_IE::SuppCodecTypes:   return "SuppCodecTypes";
        case RSL_IE::CodecConfig:      return "CodecConfig";
        case RSL_IE::Rtd:              return "Rtd";
        case RSL_IE::TfoStatus:        return "TfoStatus";
        case RSL_IE::LlpApdu:          return "LlpApdu";
        case RSL_IE::OsMoRepAcchCap:   return "OsMoRepAcchCap";
        case RSL_IE::OsMoTrainingSequence: return "OsMoTrainingSequence";
        case RSL_IE::OsMoTopAcchCap:   return "OsMoTopAcchCap";
        case RSL_IE::OsMoOsmuxCid:     return "OsMoOsmuxCid";
        case RSL_IE::IpacSrtpConfig:   return "IpacSrtpConfig";
        case RSL_IE::IpacProxyUdp:     return "IpacProxyUdp";
        case RSL_IE::IpacBscmplTout:   return "IpacBscmplTout";
        case RSL_IE::IpacRemoteIp:     return "IpacRemoteIp";
        case RSL_IE::IpacRemotePort:   return "IpacRemotePort";
        case RSL_IE::IpacRtpPayload:   return "IpacRtpPayload";
        case RSL_IE::IpacLocalPort:    return "IpacLocalPort";
        case RSL_IE::IpacSpeechMode:   return "IpacSpeechMode";
        case RSL_IE::IpacLocalIp:      return "IpacLocalIp";
        case RSL_IE::IpacConnStat:     return "IpacConnStat";
        case RSL_IE::IpacHoCParms:     return "IpacHoCParms";
        case RSL_IE::IpacConnId:       return "IpacConnId";
        case RSL_IE::IpacRtpCsdFmt:    return "IpacRtpCsdFmt";
        case RSL_IE::IpacRtpJitBuf:    return "IpacRtpJitBuf";
        case RSL_IE::IpacRtpCompr:     return "IpacRtpCompr";
        case RSL_IE::IpacRtpPayload2:  return "IpacRtpPayload2";
        case RSL_IE::IpacRtpMplex:     return "IpacRtpMplex";
        case RSL_IE::IpacRtpMplexId:   return "IpacRtpMplexId";
    }
    return "?";
}

std::string_view rslErrorCauseName(RSLErrorCause cause)
{
    switch (cause) {
        case RSLErrorCause::RadioIfFail:          return "RadioIfFail";
        case RSLErrorCause::RadioLinkFail:        return "RadioLinkFail";
        case RSLErrorCause::HandoverAccFail:      return "HandoverAccFail";
        case RSLErrorCause::TalkerAccFail:        return "TalkerAccFail";
        case RSLErrorCause::OmIntervention:       return "OmIntervention";
        case RSLErrorCause::NormalUnspec:         return "NormalUnspec";
        case RSLErrorCause::TMsrfpciExp:          return "TMsrfpciExp";
        case RSLErrorCause::EquipmentFail:        return "EquipmentFail";
        case RSLErrorCause::RrUnavail:            return "RrUnavail";
        case RSLErrorCause::TerrChFail:           return "TerrChFail";
        case RSLErrorCause::CcchOverload:         return "CcchOverload";
        case RSLErrorCause::AcchOverload:         return "AcchOverload";
        case RSLErrorCause::ProcessorOverload:    return "ProcessorOverload";
        case RSLErrorCause::BtsNotEquipped:       return "BtsNotEquipped";
        case RSLErrorCause::RemoteTrauFailure:    return "RemoteTrauFailure";
        case RSLErrorCause::NotifOverflow:        return "NotifOverflow";
        case RSLErrorCause::ResUnavail:           return "ResUnavail";
        case RSLErrorCause::TranscUnavail:        return "TranscUnavail";
        case RSLErrorCause::ServOptUnavail:       return "ServOptUnavail";
        case RSLErrorCause::EncrUnimpl:           return "EncrUnimpl";
        case RSLErrorCause::ServOptUnimpl:        return "ServOptUnimpl";
        case RSLErrorCause::RchAlrActvAlloc:      return "RchAlrActvAlloc";
        case RSLErrorCause::IpaRchNotActvAlloc:   return "IpaRchNotActvAlloc";
        case RSLErrorCause::IpaConnInvalid:       return "IpaConnInvalid";
        case RSLErrorCause::IpaConnInUse:         return "IpaConnInUse";
        case RSLErrorCause::IpaConnAlreadyExists: return "IpaConnAlreadyExists";
        case RSLErrorCause::InvalidMessage:       return "InvalidMessage";
        case RSLErrorCause::MsgDiscr:             return "MsgDiscr";
        case RSLErrorCause::MsgType:              return "MsgType";
        case RSLErrorCause::MsgSeq:               return "MsgSeq";
        case RSLErrorCause::IeError:              return "IeError";
        case RSLErrorCause::MandIeError:          return "MandIeError";
        case RSLErrorCause::OptIeError:           return "OptIeError";
        case RSLErrorCause::IeNonexist:           return "IeNonexist";
        case RSLErrorCause::IeLength:             return "IeLength";
        case RSLErrorCause::IeContent:            return "IeContent";
        case RSLErrorCause::Proto:                return "Proto";
        case RSLErrorCause::Interworking:         return "Interworking";
    }
    return "?";
}

} // namespace gsml3parser
