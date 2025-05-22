#pragma once


#include "debug/DebugDrawer.h"

namespace ad {

 
    debug::DebugDrawer gDebugDrawer;


} // namespace ad

#define DBGDRAW ::ad::gDebugDrawer
