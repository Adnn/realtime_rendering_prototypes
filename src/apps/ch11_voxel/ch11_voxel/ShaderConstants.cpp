#include "ShaderConstants.h"

#include "FrameGraph.h"

#include <engine/ShaderConstants.h>

#include <scenic/Material.h>


namespace ad {

    namespace {

        std::vector<graphics::MacroDefine> defineConstants()
        {
            std::vector<graphics::MacroDefine> result = renderer::defineShaderConstants();
            result.emplace_back(
                "CLIENT_MAX_MATERIALS " + std::to_string(scenic::gMaxMaterials)
            );
            return result;
        }
            
    } // unnamed namespace 

    const std::vector<graphics::MacroDefine> gClientConstantDefines = defineConstants();


} // namespace ad