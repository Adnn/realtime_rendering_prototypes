#include "ShaderConstants.h"

#include "Material.h"
#include "FrameGraph.h"

#include <engine/ShaderConstants.h>


namespace ad {

    namespace {

        std::vector<graphics::MacroDefine> defineConstants()
        {
            std::vector<graphics::MacroDefine> result = renderer::defineShaderConstants();
            result.emplace_back(
                "CLIENT_MAX_MATERIALS " + std::to_string(gMaxMaterials)
            );
            result.emplace_back(
                "CLIENT_SSAO_SAMPLE_COUNT " + std::to_string(gSsaoSampleCount)
            );
            return result;
        }
            
    } // unnamed namespace 

    const std::vector<graphics::MacroDefine> gClientConstantDefines = defineConstants();


} // namespace ad