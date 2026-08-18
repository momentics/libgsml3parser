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

// Umbrella header - single include for full gsml3parser API.

#include "gsml3parser/expected.h"
#include "gsml3parser/bitreader.h"
#include "gsml3parser/bitwriter.h"
#include "gsml3parser/types.h"
#include "gsml3parser/enums.h"
#include "gsml3parser/protocol_types.h"
#include "gsml3parser/gsm_common.h"
#include "gsml3parser/arena.h"
#include "gsml3parser/parser_config.h"
#include "gsml3parser/l3header.h"
#include "gsml3parser/common/l3common.h"
#include "gsml3parser/mm/l3mmelements.h"
#include "gsml3parser/cc/l3ccelements.h"
#include "gsml3parser/rr/l3rrmessages.h"
#include "gsml3parser/mm/l3mmmessages.h"
#include "gsml3parser/cc/l3ccmessages.h"
#include "gsml3parser/ss/l3ssmessages.h"
#include "gsml3parser/message_types.h"
#include "gsml3parser/parser.h"
#include "gsml3parser/visitor.h"
#include "gsml3parser/bitstream/byte_source.h"
#include "gsml3parser/bitstream/framer.h"
#include "gsml3parser/bitstream/stream_processor.h"
#include "gsml3parser/enum_formatters.h"
#include "gsml3parser/lapdm.h"
#include "gsml3parser/lapdm_frame.h"
#include "gsml3parser/lapdm_entity.h"
#include "gsml3parser/dispatcher.h"
#include "gsml3parser/stack/ms_context.h"
#include "gsml3parser/stack/l3_timer.h"
#include "gsml3parser/stack/state_machine.h"
#include "gsml3parser/stack/response_builder.h"
#include "gsml3parser/stack/transaction.h"
#include "gsml3parser/stack/channel_pool.h"
#include "gsml3parser/stack/procedure_types.h"
#include "gsml3parser/stack/response_sink.h"
#include "gsml3parser/stack/procedure.h"
#include "gsml3parser/stack/procedure_runner.h"
#include "gsml3parser/stack/subscriber_registry.h"
#include "gsml3parser/stack/procedures/location_update.h"
#include "gsml3parser/stack/procedures/authentication.h"
#include "gsml3parser/stack/procedures/call_setup_mo.h"
#include "gsml3parser/stack/procedures/call_setup_mt.h"
#include "gsml3parser/stack/procedures/channel_assignment.h"
#include "gsml3parser/stack/procedures/ciphering_mode.h"
#include "gsml3parser/stack/procedures/paging.h"
#include "gsml3parser/stack/procedures/handover.h"
#include "gsml3parser/flat_handler.h"
#include "gsml3parser/stack/sharded_channel_pool.h"
#include "gsml3parser/bitstream/inline_framer.h"
#include "gsml3parser/bitstream/zero_copy_processor.h"
#include "gsml3parser/abis/rsl_types.h"
#include "gsml3parser/abis/rsl_parser.h"
#include "gsml3parser/abis/rsl_builder.h"
