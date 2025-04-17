#include "Skybox.h"

#include <renderer/Uniforms.h>

namespace ad::scenic {


// TODO: define the exact scope of a pass:
//   Should it handle the binding of UBOs, textures, etc...
//   Do we restore some dynamic mechanism for those? 
// Important: We rely on the calling code to have bound the correct ViewProjection UBO
void passSkyboxBase(const renderer::IntrospectProgram& aProgram,
                    const EnvironmentMap & aMap,
                    GLenum aCulledFace,
                    GLenum aPolygonMode)
{
    PROFILER_SCOPE_RECURRING_SECTION(gRenderProfiler, "pass_skybox", CpuTime, GpuTime);

    const graphics::Texture& envMap = aMap.mTexture;

    // TODO: have a single dummy VAO (cannot be static, because is must be deleted while context is alive)
    graphics::VertexArrayObject dummyVao;
    glBindVertexArray(dummyVao);


    GLint unitIdx = 5;
    glActiveTexture(GL_TEXTURE0 + unitIdx);
    graphics::bind(envMap);
    graphics::setUniform(aProgram, "u_EnvironmentTexture", unitIdx);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    // TODO: could it cause issue with when rendering the skybox if existing fragments have alpha transparency?
    glDisable(GL_BLEND);
    auto scopedDepthMask = graphics::scopeDepthMask(false);

    glEnable(GL_CULL_FACE);
    auto scopedCullFace = graphics::scopeCullFace(aCulledFace);

    auto scopedPolygonMode = graphics::scopePolygonMode(aPolygonMode);

    glUseProgram(aProgram);
    // The cube hardcoded in Cube.glsl is a triangle strip from 14 indices.
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);
}

} // namespce ad::scenic