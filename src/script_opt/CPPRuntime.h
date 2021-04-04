// See the file "COPYING" in the main distribution directory for copyright.

#pragma once

#include "zeek/module_util.h"
#include "zeek/ZeekString.h"
#include "zeek/Func.h"
#include "zeek/File.h"
#include "zeek/Frame.h"
#include "zeek/Scope.h"
#include "zeek/RE.h"
#include "zeek/IPAddr.h"
#include "zeek/Val.h"
#include "zeek/OpaqueVal.h"
#include "zeek/Expr.h"
#include "zeek/Event.h"
#include "zeek/EventRegistry.h"
#include "zeek/RunState.h"
#include "zeek/script_opt/CPPFunc.h"
#include "zeek/script_opt/ScriptOpt.h"
#include "zeek/script_opt/CPPRuntimeInit.h"
#include "zeek/script_opt/CPPRuntimeOps.h"
#include "zeek/script_opt/CPPRuntimeVec.h"

namespace zeek {

using BoolValPtr = IntrusivePtr<zeek::BoolVal>;
using CountValPtr = IntrusivePtr<zeek::CountVal>;
using DoubleValPtr = IntrusivePtr<zeek::DoubleVal>;
using StringValPtr = IntrusivePtr<zeek::StringVal>;
using IntervalValPtr = IntrusivePtr<zeek::IntervalVal>;
using PatternValPtr = IntrusivePtr<zeek::PatternVal>;
using FuncValPtr = IntrusivePtr<zeek::FuncVal>;
using FileValPtr = IntrusivePtr<zeek::FileVal>;
using SubNetValPtr = IntrusivePtr<zeek::SubNetVal>;

}
