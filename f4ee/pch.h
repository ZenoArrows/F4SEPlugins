#pragma once

#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include <REX/W32.h>
#include <Scaleform/Scaleform.h>

#include "common/ITypes.h"
#include "common/IDebugLog.h"

#define DYNAMIC_CAST(obj, from, to) (RE::to *) RE::RTDynamicCast((void*)(obj), 0, (void*)(RE::RTTI::from).address(), (void*)(RE::RTTI::to).address(), 0)
#define CALL_MEMBER_FN(obj, fn)	(obj)->fn

inline void * Runtime_DynamicCast(void * srcObj, REL::ID fromType, REL::ID toType)
{
    return RE::RTDynamicCast((void*)(srcObj), 0, (void*)fromType.address(), (void*)toType.address(), 0);
}

inline const void * Runtime_DynamicCast(const void * srcObj, REL::ID fromType, REL::ID toType)
{
    return RE::RTDynamicCast((void*)(srcObj), 0, (void*)fromType.address(), (void*)toType.address(), 0);
}

using namespace std::literals;
using namespace RE;
using namespace Scaleform;
