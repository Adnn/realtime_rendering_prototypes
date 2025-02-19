#include "ShaderConstants.h"

#include "Material.h"

#include <engine/ShaderConstants.h>


namespace ad {

    namespace {

        std::vector<graphics::MacroDefine> defineConstants()
        {
            std::vector<graphics::MacroDefine> result = renderer::gClientConstantDefines;
            result.emplace_back(
                "CLIENT_MAX_MATERIALS " + std::to_string(gMaxMaterials)
            );
            return result;
        }
            
    } // unnamed namespace 

    const std::vector<graphics::MacroDefine> gClientConstantDefines = defineConstants();


} // namespace ad