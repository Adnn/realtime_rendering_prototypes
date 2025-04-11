#pragma once


#include <renderer/commons.h>

#include <string>
#include <vector>


namespace ad::renderer {


constexpr unsigned int gMaxEntities = 512;
//constexpr unsigned int gMaxJoints   = 512;


const std::vector<graphics::MacroDefine> & defineShaderConstants();


} // namespace ad::renderer