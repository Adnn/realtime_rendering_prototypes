#include "ShaderConstants.h"

#include "FrameGraph.h"
#include "Scene.h"

#include <engine/ShaderConstants.h>

#include <scenic/Material.h>


namespace ad {

    namespace {

        std::vector<graphics::MacroDefine> defineConstants()
        {
            std::vector<graphics::MacroDefine> result = renderer::defineShaderConstants();
            result.emplace_back(
                "CLIENT_MAX_MATERIALS " + std::to_string(scenic::gMaxMaterials));
            result.emplace_back(
                "CLIENT_CONETRACE_AO " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::ConeTrace_AO));
            result.emplace_back(
                "CLIENT_CONETRACE_DIFFUSE " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::ConeTrace_Diffuse));
            result.emplace_back(
                "CLIENT_CONETRACE_SPECULAR " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::ConeTrace_Specular));
            result.emplace_back(
                "CLIENT_VOXEL_MODE_OCCUPANCY " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::VoxelsOccupancy));
            result.emplace_back(
                "CLIENT_VOXEL_MODE_ALBEDO " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::VoxelsAlbedo));
            result.emplace_back(
                "CLIENT_VOXEL_MODE_NORMALS " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::VoxelsNormals));
            result.emplace_back(
                "CLIENT_VOXEL_MODE_IRRADIANCE " 
                + std::to_string((GLuint)Scene::SceneControl::Mode::VoxelsIrradiance));
            result.emplace_back(
                "CLIENT_TONEMAPPING_REINHARD " 
                + std::to_string((GLuint)FrameGraph::FrameControl::ToneMapping::Reinhard));
            result.emplace_back(
                "CLIENT_TONEMAPPING_ACES " 
                + std::to_string((GLuint)FrameGraph::FrameControl::ToneMapping::Aces));
            result.emplace_back(
                "CLIENT_TONEMAPPING_ACESAPPROX " 
                + std::to_string((GLuint)FrameGraph::FrameControl::ToneMapping::AcesApprox));
            result.emplace_back(
                "CLIENT_SHADOW_SHADOWMAP " 
                + std::to_string((GLuint)FrameGraph::FrameControl::ShadowMethod::ShadowMap));
            result.emplace_back(
                "CLIENT_SHADOW_CONETRACING " 
                + std::to_string((GLuint)FrameGraph::FrameControl::ShadowMethod::ConeTracing));
            return result;
        }
            
    } // unnamed namespace 

    const std::vector<graphics::MacroDefine> gClientConstantDefines = defineConstants();


} // namespace ad