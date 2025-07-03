#include "ShaderConstants.h"

#include "Lights.h"


namespace ad::renderer {


const std::vector<graphics::MacroDefine> & defineShaderConstants()
{
    static const std::vector<graphics::MacroDefine> constantDefines{
        "CLIENT_MAX_ENTITIES " + std::to_string(gMaxEntities),
        //"CLIENT_MAX_JOINTS " + std::to_string(gMaxJoints),
        //"CLIENT_SDF_DOUBLE_SPREAD " + std::to_string(2 * arte::gSdfSpread),
        "CLIENT_MAX_LIGHTS " + std::to_string(gMaxLights),
        "CLIENT_MAX_SHADOW_MAPS " + std::to_string(gMaxShadowMaps),
        //"CLIENT_MAX_SHADOW_LIGHTS " + std::to_string(gMaxShadowLights),
        //"CLIENT_CASCADES_PER_SHADOW " + std::to_string(gCascadesPerShadow),
    };
    return constantDefines;
}


} // namespace ad::renderer