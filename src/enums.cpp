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

#include "gsml3parser/enums.h"

namespace gsml3parser {

const char* RRCause2Str(RRCause cause) {
    switch (cause) {
        case RRCause::Normal_Event:                  return "Normal_Event";
        case RRCause::Unspecified:                   return "Unspecified";
        case RRCause::Channel_Unacceptable:          return "Channel_Unacceptable";
        case RRCause::Timer_Expired:                 return "Timer_Expired";
        case RRCause::No_Activity_On_The_Radio:      return "No_Activity";
        case RRCause::Preemptive_Release:            return "Preemptive_Release";
        case RRCause::UTRAN_Configuration_Unknown:   return "UTRAN_Config_Unknown";
        case RRCause::Handover_Impossible:           return "Handover_Impossible";
        case RRCause::Channel_Mode_Unacceptable:     return "Channel_Mode_Unacceptable";
        case RRCause::Frequency_Not_Implemented:     return "Frequency_Not_Implemented";
        case RRCause::Leaving_Group_Call_Area:       return "Leaving_Group_Call_Area";
        case RRCause::Lower_Layer_Failure:           return "Lower_Layer_Failure";
        case RRCause::Call_Already_Cleared:          return "Call_Already_Cleared";
        case RRCause::Semantically_Incorrect_Message: return "Semantically_Incorrect";
        case RRCause::Invalid_Mandatory_Information: return "Invalid_Mandatory_IE";
        case RRCause::Message_Type_Invalid:          return "Message_Type_Invalid";
        case RRCause::Message_Type_Not_Compatible:   return "Message_Type_Not_Compatible";
        case RRCause::Conditional_IE_Error:          return "Conditional_IE_Error";
        case RRCause::No_Cell_Available:             return "No_Cell_Available";
        case RRCause::Protocol_Error_Unspecified:    return "Protocol_Error";
        default:                                     return "Unknown_RR_Cause";
    }
}

const char* MMRejectCause2Str(MMRejectCause cause) {
    switch (cause) {
        case MMRejectCause::Zero:                           return "Zero";
        case MMRejectCause::IMSI_Unknown_In_HLR:          return "IMSI_Unknown_HLR";
        case MMRejectCause::Illegal_MS:                  return "Illegal_MS";
        case MMRejectCause::IMSI_Unknown_In_VLR:         return "IMSI_Unknown_VLR";
        case MMRejectCause::IMEI_Not_Accepted:           return "IMEI_Not_Accepted";
        case MMRejectCause::Illegal_ME:                  return "Illegal_ME";
        case MMRejectCause::PLMN_Not_Allowed:            return "PLMN_Not_Allowed";
        case MMRejectCause::Location_Area_Not_Allowed:   return "LA_Not_Allowed";
        case MMRejectCause::Roaming_Not_Allowed_In_LA:   return "Roaming_Not_Allowed";
        case MMRejectCause::No_Suitable_Cells_In_LA:     return "No_Suitable_Cells";
        case MMRejectCause::Network_Failure:             return "Network_Failure";
        case MMRejectCause::MAC_Failure:                 return "MAC_Failure";
        case MMRejectCause::Synch_Failure:               return "Synch_Failure";
        case MMRejectCause::Congestion:                  return "Congestion";
        case MMRejectCause::GSM_Authentication_Unacceptable: return "Auth_Unacceptable";
        case MMRejectCause::Not_Authorized_In_CSG:       return "Not_Authorized_CSG";
        case MMRejectCause::Service_Option_Not_Supported: return "Service_Not_Supported";
        case MMRejectCause::Requested_Service_Not_Subscribed: return "Service_Not_Subscribed";
        case MMRejectCause::Service_Option_Out_Of_Order: return "Service_Option_Out_Of_Order";
        case MMRejectCause::Call_Cannot_Be_Identified:   return "Call_Cannot_Be_Identified";
        case MMRejectCause::Semantically_Incorrect_Message: return "Semantically_Incorrect";
        case MMRejectCause::Invalid_Mandatory_Information: return "Invalid_Mandatory_IE";
        case MMRejectCause::Message_Type_Invalid:        return "Message_Type_Invalid";
        case MMRejectCause::Message_Type_Not_Compatible: return "Message_Type_Not_Compatible";
        case MMRejectCause::IE_Invalid:                  return "IE_Invalid";
        case MMRejectCause::Conditional_IE_Error:        return "Conditional_IE_Error";
        case MMRejectCause::Message_Not_Compatible:      return "Message_Not_Compatible";
        case MMRejectCause::Protocol_Error_Unspecified:  return "Protocol_Error";
        default:                                         return "Unknown_MM_Cause";
    }
}

const char* CCCause2Str(CCCause cause) {
    switch (cause) {
        case CCCause::Unassigned_Number:                  return "Unassigned_Number";
        case CCCause::No_Route_To_Destination:            return "No_Route";
        case CCCause::Channel_Unacceptable:               return "Channel_Unacceptable";
        case CCCause::Operator_Determined_Barring:        return "Operator_Barring";
        case CCCause::Normal_Call_Clearing:               return "Normal_Clearing";
        case CCCause::User_Busy:                          return "User_Busy";
        case CCCause::No_User_Responding:                 return "No_Response";
        case CCCause::User_Alerting_No_Answer:            return "No_Answer";
        case CCCause::Call_Rejected:                      return "Call_Rejected";
        case CCCause::Number_Changed:                     return "Number_Changed";
        case CCCause::Preemption:                         return "Preemption";
        case CCCause::Non_Selected_User_Clearing:         return "Non_Selected_User_Clearing";
        case CCCause::Destination_Out_Of_Order:           return "Out_Of_Order";
        case CCCause::Invalid_Number_Format:              return "Invalid_Number";
        case CCCause::Facility_Rejected:                  return "Facility_Rejected";
        case CCCause::Response_To_STATUS_ENQUIRY:         return "Response_To_STATUS_ENQUIRY";
        case CCCause::Normal_Unspecified:                 return "Normal_Unspecified";
        case CCCause::No_Channel_Available:               return "No_Channel";
        case CCCause::Network_Out_Of_Order:               return "Network_Failure";
        case CCCause::Temporary_Failure:                  return "Temporary_Failure";
        case CCCause::Switching_Equipment_Congestion:     return "Switching_Congestion";
        case CCCause::Access_Information_Discarded:       return "Access_Info_Discarded";
        case CCCause::Requested_Channel_Not_Available:    return "Channel_Not_Available";
        case CCCause::Resources_Unavailable:              return "Resources_Unavailable";
        case CCCause::Quality_Of_Service_Unavailable:     return "QoS_Unavailable";
        case CCCause::Requested_Facility_Not_Subscribed:  return "Facility_Not_Subscribed";
        case CCCause::Bearer_Capability_Not_Authorized:   return "Bearer_Not_Authorized";
        case CCCause::Bearer_Capability_Not_Available:    return "Bearer_Not_Available";
        case CCCause::Service_Or_Option_Not_Available:    return "Service_Unavailable";
        case CCCause::Bearer_Service_Not_Implemented:     return "Bearer_Not_Implemented";
        case CCCause::ACM_GE_Max:                         return "ACM_GE_Max";
        case CCCause::Requested_Facility_Not_Implemented: return "Facility_Not_Implemented";
        case CCCause::Only_Restricted_Digital_Info:       return "Only_Restricted_Digital";
        case CCCause::Service_Or_Option_Not_Implemented:  return "Service_Not_Implemented";
        case CCCause::Invalid_Transaction_ID:             return "Invalid_Transaction_ID";
        case CCCause::User_Not_Member_Of_CUG:             return "Not_Member_CUG";
        case CCCause::Incompatible_Destination:           return "Incompatible_Destination";
        case CCCause::Invalid_Transit_Network:            return "Invalid_Transit_Network";
        case CCCause::Semantically_Incorrect_Message:     return "Semantically_Incorrect";
        case CCCause::Invalid_Mandatory_Information:      return "Invalid_Mandatory_IE";
        case CCCause::Message_Type_Not_Implemented:       return "Message_Type_Not_Implemented";
        case CCCause::Message_Not_Compatible_With_State:  return "Message_Incompatible_State";
        case CCCause::IE_Not_Implemented:                 return "IE_Not_Implemented";
        case CCCause::Conditional_IE_Error:               return "Conditional_IE_Error";
        case CCCause::Message_Not_Compatible:             return "Message_Not_Compatible";
        case CCCause::Recovery_On_Timer_Expiry:           return "Recovery_Timer_Expiry";
        case CCCause::Protocol_Error_Unspecified:         return "Protocol_Error";
        case CCCause::Interworking_Unspecified:           return "Interworking_Unspecified";
        default:                                          return "Unknown_CC_Cause";
    }
}

const char* BSSCause2Str(BSSCause cause) {
    switch (cause) {
        case BSSCause::Radio_Interface_Failure:         return "Radio_Interface_Failure";
        case BSSCause::Uplink_Quality:                 return "Uplink_Quality";
        case BSSCause::Uplink_Strength:                return "Uplink_Strength";
        case BSSCause::Downlink_Quality:               return "Downlink_Quality";
        case BSSCause::Downlink_Strength:              return "Downlink_Strength";
        case BSSCause::Distance:                       return "Distance";
        case BSSCause::Operator_Intervention:          return "Operator_Intervention";
        case BSSCause::Channel_Assignment_Failure:     return "Channel_Assignment_Failure";
        case BSSCause::Handover_Successful:            return "Handover_Successful";
        case BSSCause::Better_Cell:                    return "Better_Cell";
        case BSSCause::Traffic:                        return "Traffic";
        case BSSCause::Reduce_Load_In_Serving_Cell:    return "Reduce_Load_Serving_Cell";
        case BSSCause::Equipment_Failure:              return "Equipment_Failure";
        case BSSCause::No_Radio_Resource_Available:    return "No_Radio_Resource";
        case BSSCause::CCCH_Overload:                  return "CCCH_Overload";
        case BSSCause::Processor_Overload:             return "Processor_Overload";
        case BSSCause::Traffic_Load:                   return "Traffic_Load";
        case BSSCause::Emergency_Preemption:           return "Emergency_Preemption";
        case BSSCause::Ciphering_Algorithm_Not_Supported: return "Ciphering_Algo_Not_Supported";
        default:                                       return "Unknown_BSS_Cause";
    }
}

} // namespace gsml3parser
