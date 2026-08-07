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

#pragma once

#include <cstdint>

namespace gsml3parser {

// RR Cause (GSM 04.08 10.5.2.31)
enum class RRCause : uint8_t {
    Normal_Event                     = 0,
    Unspecified                      = 1,
    Channel_Unacceptable             = 2,
    Timer_Expired                    = 3,
    No_Activity_On_The_Radio         = 4,
    Preemptive_Release               = 5,
    UTRAN_Configuration_Unknown      = 6,
    Handover_Impossible              = 8,
    Channel_Mode_Unacceptable        = 9,
    Frequency_Not_Implemented        = 0xa,
    Leaving_Group_Call_Area          = 0xb,
    Lower_Layer_Failure              = 0xc,
    Call_Already_Cleared             = 0x41,
    Semantically_Incorrect_Message   = 0x5f,
    Invalid_Mandatory_Information    = 0x60,
    Message_Type_Invalid             = 0x61,
    Message_Type_Not_Compatible      = 0x62,
    Conditional_IE_Error             = 0x64,
    No_Cell_Available                = 0x65,
    Protocol_Error_Unspecified       = 0x6f
};

const char* RRCause2Str(RRCause cause);

// MM Reject Cause (GSM 04.08 10.5.3.6)
enum class MMRejectCause : uint8_t {
    Zero                            = 0,
    IMSI_Unknown_In_HLR             = 2,
    Illegal_MS                      = 3,
    IMSI_Unknown_In_VLR             = 4,
    IMEI_Not_Accepted               = 5,
    Illegal_ME                      = 6,
    PLMN_Not_Allowed                = 0xb,
    Location_Area_Not_Allowed       = 0xc,
    Roaming_Not_Allowed_In_LA       = 0xd,
    No_Suitable_Cells_In_LA         = 0xf,
    Network_Failure                 = 0x11,
    MAC_Failure                     = 0x14,
    Synch_Failure                   = 0x15,
    Congestion                      = 0x16,
    GSM_Authentication_Unacceptable = 0x17,
    Not_Authorized_In_CSG           = 0x19,
    Service_Option_Not_Supported    = 0x20,
    Requested_Service_Not_Subscribed = 0x21,
    Service_Option_Out_Of_Order     = 0x22,
    Call_Cannot_Be_Identified       = 0x26,
    Semantically_Incorrect_Message  = 0x5f,
    Invalid_Mandatory_Information   = 0x60,
    Message_Type_Invalid            = 0x61,
    Message_Type_Not_Compatible     = 0x62,
    IE_Invalid                      = 0x63,
    Conditional_IE_Error            = 0x64,
    Message_Not_Compatible          = 0x65,
    Protocol_Error_Unspecified      = 0x6f
};

const char* MMRejectCause2Str(MMRejectCause cause);

// CC Cause (GSM 04.08 10.5.4.11)
enum class CCCause : uint8_t {
    Unknown_L3_Cause                 = 0,
    Unassigned_Number                = 1,
    No_Route_To_Destination          = 3,
    Channel_Unacceptable             = 6,
    Operator_Determined_Barring      = 8,
    Normal_Call_Clearing             = 16,
    User_Busy                        = 17,
    No_User_Responding               = 18,
    User_Alerting_No_Answer          = 19,
    Call_Rejected                    = 21,
    Number_Changed                   = 22,
    Preemption                       = 25,
    Non_Selected_User_Clearing       = 26,
    Destination_Out_Of_Order         = 27,
    Invalid_Number_Format            = 28,
    Facility_Rejected                = 29,
    Response_To_STATUS_ENQUIRY       = 30,
    Normal_Unspecified               = 31,
    No_Channel_Available             = 34,
    Network_Out_Of_Order             = 38,
    Temporary_Failure                = 41,
    Switching_Equipment_Congestion   = 42,
    Access_Information_Discarded     = 43,
    Requested_Channel_Not_Available  = 44,
    Resources_Unavailable            = 47,
    Quality_Of_Service_Unavailable   = 49,
    Requested_Facility_Not_Subscribed = 50,
    Bearer_Capability_Not_Authorized = 57,
    Bearer_Capability_Not_Available  = 58,
    Incoming_Calls_Barred_Within_CUG = 55,
    Service_Or_Option_Not_Available  = 63,
    Bearer_Service_Not_Implemented   = 65,
    ACM_GE_Max                       = 68,
    Requested_Facility_Not_Implemented = 69,
    Only_Restricted_Digital_Info     = 70,
    Service_Or_Option_Not_Implemented = 79,
    Invalid_Transaction_ID           = 81,
    User_Not_Member_Of_CUG           = 87,
    Incompatible_Destination         = 88,
    Invalid_Transit_Network          = 91,
    Semantically_Incorrect_Message   = 95,
    Invalid_Mandatory_Information    = 96,
    Message_Type_Not_Implemented     = 97,
    Message_Not_Compatible_With_State = 98,
    IE_Not_Implemented               = 99,
    Conditional_IE_Error             = 100,
    Message_Not_Compatible           = 101,
    Recovery_On_Timer_Expiry         = 102,
    Protocol_Error_Unspecified       = 111,
    Interworking_Unspecified         = 127
};

const char* CCCause2Str(CCCause cause);

// CC cause location
enum class CCCauseLocation : uint8_t {
    User                      = 0,
    Private_Serving_Local     = 1,
    Public_Serving_Local      = 2,
    Transit                   = 3,
    Public_Serving_Remote     = 4,
    Private_Serving_Remote    = 5,
    International             = 7,
    Beyond_Inter_Networking   = 10
};

// BSS Cause (GSM 48.008 3.2.2.5)
enum class BSSCause : uint8_t {
    Radio_Interface_Failure     = 1,
    Uplink_Quality             = 2,
    Uplink_Strength            = 3,
    Downlink_Quality           = 4,
    Downlink_Strength          = 5,
    Distance                   = 6,
    Operator_Intervention      = 7,
    Channel_Assignment_Failure = 0xa,
    Handover_Successful        = 0xb,
    Better_Cell                = 0xc,
    Traffic                    = 0xf,
    Reduce_Load_In_Serving_Cell = 0x10,
    Traffic_Load_In_Target_Cell_Higher_Than_In_Source_Cell = 0x11,
    Relocation_Triggered       = 0x12,
    Equipment_Failure          = 0x20,
    No_Radio_Resource_Available = 0x21,
    CCCH_Overload              = 0x23,
    Processor_Overload         = 0x24,
    DTM_Handover_SGSN_Failure  = 0x2a,
    DTM_Handover_PS_Allocation_Failure = 0x2b,
    Traffic_Load               = 0x28,
    Emergency_Preemption       = 0x29,
    Transcoding_Mismatch       = 0x30,
    Requested_Speech_Version_Unavailable = 0x33,
    Ciphering_Algorithm_Not_Supported = 0x40
};

const char* BSSCause2Str(BSSCause cause);

} // namespace gsml3parser
