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

"""libgsml3parser Python binding — standard-library only (ctypes).

A ctypes wrapper over the stable C ABI (gsml3parser_c.h) of the GSM L3 /
A-bis RSL / LAPDm / BTS-stack parser: zero third-party runtime dependencies,
full RAII, typed errors, GC-safe callbacks and the queue-model BTS stack.

Quickstart::

    import gsml3parser as g                       # loads build_bindings/bin/gsml3parser.*

    msg = g.Message.from_hex("60 0D 00")          # RR Channel Release
    print(g.version(), msg.name, msg.size(), msg.write().hex())

    with g.GsmL3Stack(tmsi=0x87654321) as stack:  # registry + session + orchestrator + LAPDm entity
        l3 = g.build_cm_service_request(1, 4, 0x87654321, None)
        tx = stack.send_frame(g.lapdm_mini.ui(0, False, l3))   # auto-response: parse -> feed -> build -> send
        assert g.lapdm_frame_decode(tx[0]).format == 2         # UI frame back from the BTS side

See the module docs of gsml3parser.stack for the ownership, threading and
callback-safety rules that apply to everything below.
"""

from pathlib import Path

# Product version — the repository-root VERSION file is the single source of
# truth; this runtime value mirrors it (the build-time
# metadata does too, via pyproject.toml dynamic version).
_REPO_VERSION_FILE = Path(__file__).resolve().parents[3] / "VERSION"
try:
    __version__ = _REPO_VERSION_FILE.read_text(encoding="utf-8").strip()
except OSError:      # installed outside the repository tree
    __version__ = "0.19.0"   # mirrors the current VERSION file (the repository has no
                              # published releases — nothing else to sync with)

# Error model (codes mirror enum gsml3_error 1:1; raise hierarchy on top).
from ._errors import (
    OK, INVALID_ARG, TRUNCATED, INVALID_PD, INVALID_MTI, LENGTH_MISMATCH,
    INVALID_IE, INVALID_VALUE, UNSUPPORTED, SOURCE_EXHAUSTED, NO_MEMORY,
    BUFFER_TOO_SMALL, INTERNAL, DUPLICATE, CODE_CLOSED,
    GsmL3Error, InvalidArgument, TruncatedInput, ProtocolDomainError,
    UnsupportedOperation, GsmL3NoMemory, BufferTooSmall, InternalError,
    ClosedGsmL3ObjectError, raise_last_error,
)

# High-level API: RAII wrappers, the queue-model stack, typed value objects,
# LAPDm decode and ALL stateless S4/S7/S9 builders (238 C functions total are
# registered with argtypes/restype in gsml3parser._library — enforced by
# test_api_surface.py against the C header).
from .stack import (
    Config, Message, RslFrame, Registry, Session, Orchestrator, LapdmEntity,
    GsmL3Stack, StepResult, L3Event, FrameInfo, TimerExpiryPy,
    Channel, MobileIdentity, Lai, lapdm_frame_decode,
)

# Enum-mirror constants used across the demo/tests (values from gsml3parser_c.h).
from .stack import (
    PD_GCC, PD_BCC, PD_CC, PD_MM, PD_RR, PD_GMM, PD_SMS, PD_SM, PD_SS, PD_LS,
    PD_EXT, PD_TST,
    ID_NO_ID, ID_IMSI, ID_IMEI, ID_IMEISV, ID_TMSI,
    TOKEN_NONE, TOKEN_CM_SERVICE_ACCEPT, TOKEN_CALL_PROCEEDING,
    PRIM_L3_UNIT_DATA, PRIM_L3_ESTABLISH_CONFIRM,
    LAPDM_STATE_UNUSED, LAPDM_STATE_LINK_RELEASED, LAPDM_STATE_AWAITING_ESTABLISH,
    LAPDM_STATE_AWAITING_RELEASE, LAPDM_STATE_LINK_ESTABLISHED,
    PROC_CALL_SETUP_MO, PROC_UNKNOWN, ACTION_CONTINUE, ACTION_SEND_RESPONSE,
)

# The 13 RSL (A-bis) builders.
from .stack import (
    rsl_build_data_req, rsl_build_data_ind, rsl_build_unit_data_req,
    rsl_build_unit_data_ind, rsl_build_chan_activ_ack, rsl_build_chan_activ_nack,
    rsl_build_rf_chan_rel_ack, rsl_build_conn_fail, rsl_build_meas_res,
    rsl_build_hando_det, rsl_build_ccch_load_ind, rsl_build_chan_rqd,
    rsl_build_delete_ind,
)

# The 20 standalone S7 response builders + the token-based pair.
from .stack import (
    response_build_cm_service_accept, response_build_cm_service_reject,
    response_build_identity_request, response_build_authentication_request,
    response_build_location_updating_accept, response_build_location_updating_reject,
    response_build_tmsi_reallocation_command, response_build_channel_release,
    response_build_ciphering_mode_command, response_build_physical_information,
    response_build_immediate_assignment, response_build_assignment_command,
    response_build_call_proceeding, response_build_alerting, response_build_connect,
    response_build_connect_acknowledge, response_build_disconnect,
    response_build_release, response_build_release_complete, response_build_setup,
    response_required_size, response_build_from_token,
)

# The 43 S9 typed L3 builders (including the two the demo chain is built from).
from .stack import (
    build_channel_release, build_channel_request, build_immediate_assignment,
    build_immediate_assignment_reject, build_assignment_command,
    build_assignment_complete, build_assignment_failure,
    build_paging_request_type1, build_paging_request_type2, build_paging_request_type3,
    build_paging_response, build_ciphering_mode_command, build_ciphering_mode_complete,
    build_handover_complete, build_physical_information,
    build_cm_service_request, build_cm_service_accept, build_cm_service_reject,
    build_cm_service_abort, build_identity_request, build_identity_response,
    build_location_updating_request, build_location_updating_accept,
    build_location_updating_reject, build_authentication_request,
    build_authentication_response, build_tmsi_reallocation_command,
    build_tmsi_reallocation_complete, build_imsi_detach_indication,
    build_setup, build_call_proceeding, build_alerting, build_connect,
    build_connect_acknowledge, build_disconnect, build_release,
    build_release_complete, build_facility, build_cp_data, build_cp_status,
    build_cp_smt, build_sms_deliver, build_sup_serv_facility,
)

# MS/peer-side LAPDm mini-codec (simulation & tests only; production send goes
# through the C entity). Exposed as `g.lapdm_mini`.
from . import _lapdm as lapdm_mini

#: Raw registered library proxy + FFI call counters — the closed-path test
#: seam (see gsml3parser._library). Not part of the normal public API.
from ._library import lib, PROTOTYPES, EXPECTED_ABI, CALL_COUNTS


def version() -> str:
    """Library version string from the C core (gsml3_version()) — must equal
    the repository-root VERSION file content (pinned by the Phase-1 tests)."""
    v = lib.gsml3_version()  # static storage; returned as a bytes copy
    return v.decode("utf-8", "replace") if isinstance(v, (bytes, bytearray)) else ""


def abi_version() -> int:
    """C ABI revision reported by the loaded library (== GSML3_ABI_VERSION)."""
    return int(lib.gsml3_abi_version())


__all__ = [
    # package meta
    "__version__", "version", "abi_version",
    # error model
    "OK", "INVALID_ARG", "TRUNCATED", "INVALID_PD", "INVALID_MTI", "LENGTH_MISMATCH",
    "INVALID_IE", "INVALID_VALUE", "UNSUPPORTED", "SOURCE_EXHAUSTED", "NO_MEMORY",
    "BUFFER_TOO_SMALL", "INTERNAL", "DUPLICATE", "CODE_CLOSED",
    "GsmL3Error", "InvalidArgument", "TruncatedInput", "ProtocolDomainError",
    "UnsupportedOperation", "GsmL3NoMemory", "BufferTooSmall", "InternalError",
    "ClosedGsmL3ObjectError", "raise_last_error",
    # classes & value objects
    "Config", "Message", "RslFrame", "Registry", "Session", "Orchestrator",
    "LapdmEntity", "GsmL3Stack", "StepResult", "L3Event", "FrameInfo",
    "TimerExpiryPy", "Channel", "MobileIdentity", "Lai", "lapdm_frame_decode",
    # enum-mirror constants
    "PD_GCC", "PD_BCC", "PD_CC", "PD_MM", "PD_RR", "PD_GMM", "PD_SMS", "PD_SM",
    "PD_SS", "PD_LS", "PD_EXT", "PD_TST",
    "ID_NO_ID", "ID_IMSI", "ID_IMEI", "ID_IMEISV", "ID_TMSI",
    "TOKEN_NONE", "TOKEN_CM_SERVICE_ACCEPT", "TOKEN_CALL_PROCEEDING",
    "PRIM_L3_UNIT_DATA", "PRIM_L3_ESTABLISH_CONFIRM",
    "LAPDM_STATE_UNUSED", "LAPDM_STATE_LINK_RELEASED",
    "LAPDM_STATE_AWAITING_ESTABLISH", "LAPDM_STATE_AWAITING_RELEASE",
    "LAPDM_STATE_LINK_ESTABLISHED",
    "PROC_CALL_SETUP_MO", "PROC_UNKNOWN", "ACTION_CONTINUE", "ACTION_SEND_RESPONSE",
    # builders
    "rsl_build_data_req", "rsl_build_data_ind", "rsl_build_unit_data_req",
    "rsl_build_unit_data_ind", "rsl_build_chan_activ_ack", "rsl_build_chan_activ_nack",
    "rsl_build_rf_chan_rel_ack", "rsl_build_conn_fail", "rsl_build_meas_res",
    "rsl_build_hando_det", "rsl_build_ccch_load_ind", "rsl_build_chan_rqd",
    "rsl_build_delete_ind",
    "response_build_cm_service_accept", "response_build_cm_service_reject",
    "response_build_identity_request", "response_build_authentication_request",
    "response_build_location_updating_accept", "response_build_location_updating_reject",
    "response_build_tmsi_reallocation_command", "response_build_channel_release",
    "response_build_ciphering_mode_command", "response_build_physical_information",
    "response_build_immediate_assignment", "response_build_assignment_command",
    "response_build_call_proceeding", "response_build_alerting",
    "response_build_connect", "response_build_connect_acknowledge",
    "response_build_disconnect", "response_build_release",
    "response_build_release_complete", "response_build_setup",
    "response_required_size", "response_build_from_token",
    "build_channel_release", "build_channel_request", "build_immediate_assignment",
    "build_immediate_assignment_reject", "build_assignment_command",
    "build_assignment_complete", "build_assignment_failure",
    "build_paging_request_type1", "build_paging_request_type2",
    "build_paging_request_type3", "build_paging_response",
    "build_ciphering_mode_command", "build_ciphering_mode_complete",
    "build_handover_complete", "build_physical_information",
    "build_cm_service_request", "build_cm_service_accept",
    "build_cm_service_reject", "build_cm_service_abort",
    "build_identity_request", "build_identity_response",
    "build_location_updating_request", "build_location_updating_accept",
    "build_location_updating_reject", "build_authentication_request",
    "build_authentication_response", "build_tmsi_reallocation_command",
    "build_tmsi_reallocation_complete", "build_imsi_detach_indication",
    "build_setup", "build_call_proceeding", "build_alerting", "build_connect",
    "build_connect_acknowledge", "build_disconnect", "build_release",
    "build_release_complete", "build_facility", "build_cp_data", "build_cp_status",
    "build_cp_smt", "build_sms_deliver", "build_sup_serv_facility",
    # modules / seams
    "lapdm_mini", "lib", "PROTOTYPES", "EXPECTED_ABI", "CALL_COUNTS",
]
