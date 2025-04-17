#include "Scene.h"

#include "log/Logging.h"

#include <graphics/AppInterface.h>
#include <graphics/ApplicationGlfw.h>
#include <graphics/CameraUtilities.h>

#include <reflect/DearImguiWitness.h>
#include <reflect/ReflectHelpers.h>

#include <renderer/BufferIndexedBinding.h>
#include <renderer/BufferLoad.h>
#include <renderer/Uniforms.h>

#include <ui/ImguiUi.h>
#include <ui/Widgets.h>
#include <ui/Widgets-impl.h>


namespace ad {

void loadToBuffer(const renderer::EntitiesBlock_glsl & aData,
                  const graphics::UniformBufferObject & aBuffer,
                  graphics::BufferHint aUsageHint)
{
    graphics::load(aBuffer, std::span{ aData.mEntities }, aUsageHint);
}


// TODO: This is brittle, take some time to understand this part of query API
// I have to understand the relation between attrib location, vertex attribute index
void validateVertexAttributes(const renderer::IntrospectProgram & aProgram) 
{
    GLint numAttributes;
    glGetProgramiv(aProgram, GL_ACTIVE_ATTRIBUTES, &numAttributes);

    for (GLint i = 0; i < numAttributes; i++)
    {
        char name[128];
        GLint size;
        GLenum type;
        glGetActiveAttrib(aProgram, i, sizeof(name), nullptr, &size, &type, name);
        // TODO: ensure the name was not longer than provided buffer

        // Get the location of this attribute in the shader
        GLint location = glGetAttribLocation(aProgram, name);
        if (location == -1) {
            continue; // Might be an optimized-out attribute
        }

        GLint attribType;
        GLint attribInteger;
        // Give the type of value in the GL_ARRAY_BUFFER, not wether it was bound with
        // glVertexAttribPointer or glVertexAttribIPointer
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_TYPE, &attribType); 
        glGetVertexAttribiv(location, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &attribInteger);  // Key check
        bool vaoUsesIntegerMode = (attribInteger == GL_TRUE);  // Set by glVertexAttribIPointer

        //ADLOG(trace)("Active attribute #{} '{}', location {}, shader type {}, vao type {}, is integer: {}.", 
        //    i, name, location, graphics::to_string(type), graphics::to_string(attribType), vaoUsesIntegerMode);
        
        bool mismatch = false;
        if ((type == GL_INT || type == GL_INT_VEC2 || type == GL_INT_VEC3 || type == GL_INT_VEC4
             || type == GL_UNSIGNED_INT || type == GL_UNSIGNED_INT_VEC2 || type == GL_UNSIGNED_INT_VEC3 || type == GL_UNSIGNED_INT_VEC4) &&
            !vaoUsesIntegerMode) 
        {
            mismatch = true;
        }
        else if ((type == GL_FLOAT || type == GL_FLOAT_VEC2 || type == GL_FLOAT_VEC3 || type == GL_FLOAT_VEC4) &&
            vaoUsesIntegerMode) 
        {
            mismatch = true;
        }

        if (mismatch) 
        {
            ADLOG(error)("Attribute '{}' in program '{}' has a type mismatch. Active attribe type is {}, but use integer mode is {}",
                         name, aProgram.mName, graphics::to_string(type), vaoUsesIntegerMode);
        }
    }
}


const std::filesystem::path gProgramPath = "programs/ch10_area_lights_TessellateSphere.prog";
//const std::filesystem::path gProgramPath = "programs/WrapLighting.prog";
const std::filesystem::path gLightProgramPath = "programs/TessSphere_PlainColor.prog";
const std::filesystem::path gLineProgramPath = "programs/FatLine.prog";

template <class T_witness>
void describe(T_witness aWitness, Scene::TessellationControl & aValue)
{
    GIVE_EX(make_Clamped(aValue.mPatchVertices, {.mMin = 1u, .mMax = (GLuint)aValue.mMaxPatchVertices}), 
            PatchVertices);
    GIVE(OuterLevel);
    GIVE(InnerLevel);

    static const math::Vec<4, GLfloat> maxTess{
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
        (GLfloat)aValue.mMaxTessGenLevel,
    };
    aValue.mOuterLevel = math::min(aValue.mOuterLevel, maxTess);
    aValue.mInnerLevel = math::min(aValue.mInnerLevel, maxTess.xy());
}


Scene::Scene(graphics::AppInterface & aAppInterface, const imguiui::ImguiUi & aImgui) :
    mVertexSpecification{},
    mIndexBuffer{
        graphics::loadIndexBuffer(mVertexSpecification.mVertexArray,
                                  //std::span{scenic::icosahedron::gIndices},
                                  std::span{mSphere.mIndices},
                                  graphics::BufferHint::StaticDraw)},
    mSurfaceProgram{mEngine.loadProgram(renderer::ReferencePath{gProgramPath})},
    mLightProgram{mEngine.loadProgram(renderer::ReferencePath{gLightProgramPath})},
    mLineProgram{mEngine.loadProgram(renderer::ReferencePath{gLineProgramPath})}
{
    graphics::attachIndexBuffer(mIndexBuffer, mVertexSpecification.mVertexArray);

    graphics::appendToVertexSpecification(
        mVertexSpecification,
        gVertexDescription,
        //std::span{scenic::icosahedron::gVertices},
        std::span{mSphere.mVertices},
        graphics::BufferHint::StaticDraw);

    //graphics::appendToVertexSpecification(
    //    mVertexSpecification,
    //    gInstanceDescription,
    //    std::span{gInstances},
    //    graphics::BufferHint::StaticDraw,
    //    1);

    // Register the camera system with glfw inputs 
    graphics::registerGlfwCallbacks(
        aAppInterface,
        mOrbitalCamera.mOrbitalControl,
        graphics::EscKeyBehaviour::Close,
        // TODO: this is a dirty capture of a parameter given by reference
        &aImgui);

    // TODO use defines here for binding points
    graphics::bind(mViewProjectionBuffer, graphics::BindingIndex{0});
    glObjectLabel(GL_BUFFER, mViewProjectionBuffer, -1, "ViewProjection");
    graphics::bind(mEntitiesBlockBuffer, graphics::BindingIndex{1});
    glObjectLabel(GL_BUFFER, mEntitiesBlockBuffer, -1, "Entities");
    graphics::bind(mMaterialsBlockBuffer, graphics::BindingIndex{2});
    glObjectLabel(GL_BUFFER, mMaterialsBlockBuffer, -1, "Materials");
    graphics::bind(mLightsBlockBuffer, graphics::BindingIndex{4});
    glObjectLabel(GL_BUFFER, mLightsBlockBuffer, -1, "Lights");
}


void Scene::loadPrograms()
{
    mSurfaceProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gProgramPath });
    mLightProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gLightProgramPath });
    mLineProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gLineProgramPath });
}


void Scene::step(const graphics::Timer & /*aTimer*/,
                 math::Size<2, int> aWindowResolution)
{
    mOrbitalCamera.update(aWindowResolution.height());
}


renderer::LightsDataCommon transformLightsData(
    renderer::LightsDataCommon aLightsData, // by value, as we need a copy
    const math::AffineMatrix<4, float>& aTransform)
{
    for (auto idx = 0; idx != aLightsData.mDirectionalCount; ++idx)
    {
        renderer::DirectionalLight_glsl & light = aLightsData.mDirectionalLights[idx];
        // might be unecessary to re-normalize, unless the transform scales
        light.mDirection = math::UnitVec<3, GLfloat>{
            light.mDirection * aTransform.getLinear() };
    }
    for (auto idx = 0; idx != aLightsData.mPointCount; ++idx)
    {
        renderer::PointLight_glsl & light = aLightsData.mPointLights[idx];
        light.mPosition = math::homogeneous::homogenize(
            math::homogeneous::makePosition(light.mPosition) * aTransform).xyz();
    }

    return aLightsData;
}


void Scene::render(math::Size<2, int> aRenderResolution)
{
    //
    // Entities
    // 
    const unsigned int objectsCount = 1;
    // Ensure the vector can fit all point lights
    mEntities.mEntities.resize(objectsCount + mLights.mPointCount);
    mLines.mSegments.clear();

    for (std::size_t pointLightIdx = 0; pointLightIdx != mLights.mPointCount; ++pointLightIdx)
    {
        const auto& pointLight = mLights.mPointLights[pointLightIdx];
        auto& entity = mEntities.mEntities[objectsCount + pointLightIdx];
        entity.mLocalToWorld =
            math::trans3d::scaleUniform(pointLight.mRadius.mMin)
            * math::trans3d::translate(pointLight.mPosition.as<math::Vec>());
        entity.mColorFactor = pointLight.mColors.mDiffuseColor;

        // Populate the line segments representing the tube lights
        if (pointLightIdx % 2 == 1)
        {
            mLines.mSegments.push_back(
                renderer::LineSegment_glsl{
                    .mPointA = mLights.mPointLights[pointLightIdx - 1].mPosition,
                    .mPointB = mLights.mPointLights[pointLightIdx].mPosition,
                    .mWidth = 2 * mLights.mPointLights[pointLightIdx - 1].mRadius.mMin,
                });
        }
    }
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);

    //
    // Materials
    // 
    graphics::loadSingle(mMaterialsBlockBuffer, mMaterials, graphics::BufferHint::StreamDraw);

    //
    // Lights
    //
    auto lights_cam = 
        transformLightsData(mLights, mOrbitalCamera.mCamera.getParentToCamera());
    graphics::loadSingle(mLightsBlockBuffer, lights_cam, graphics::BufferHint::StreamDraw);

    //
    // Camera
    //
    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    //
    // Pipeline state
    // 
    glPolygonMode(GL_FRONT_AND_BACK, *mFrameControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(mVertexSpecification.mVertexArray);
    
    glViewport(0, 0, aRenderResolution.width(), aRenderResolution.height());

    // The input patch (directly fed to the TES) are the 3 vertices of a triangle.
    glPatchParameteri(GL_PATCH_VERTICES, mTessControl.mPatchVertices);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, mTessControl.mOuterLevel.data());
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, mTessControl.mInnerLevel.data());

    //
    // Draw
    //
    const GLuint sphereCount = 1;

    // TODO: should be done only once for each pair of VAO-program
    validateVertexAttributes(mSurfaceProgram);
    glUseProgram(mSurfaceProgram);

    glDrawElementsInstancedBaseInstance(
        GL_PATCHES,
        mIndicesCount,
        graphics::MappedGL_v<scenic::Index>,
        0,
        sphereCount,
        0);

    // Render point lights as sphere
    validateVertexAttributes(mLightProgram);
    glUseProgram(mLightProgram);
    glDrawElementsInstancedBaseInstance(
        GL_PATCHES,
        mIndicesCount,
        graphics::MappedGL_v<scenic::Index>,
        0,
        mLights.mPointCount,
        sphereCount);

    // Render tube lights as fat lines
    {
        glDisable(GL_CULL_FACE);
        graphics::setUniform(mLineProgram, "u_FramebufferSize", aRenderResolution);
        {
            // Binds to the general binding point, in addition to binding index 8
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, mLinesSsbo);
            std::span<renderer::LineSegment_glsl> lines{ mLines.mSegments };
            glBufferData(GL_SHADER_STORAGE_BUFFER, lines.size_bytes(), lines.data(), GL_STREAM_DRAW);
        }
        glUseProgram(mLineProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glEnable(GL_CULL_FACE);
    }
}


void Scene::presentUi(bool * aOpen)
{
    ImGui::Begin("Scene", aOpen);

    if (ImGui::Button("Recompile shaders"))
    {
        try
        {
            loadPrograms();
        }
        catch (const std::exception& aException)
        {
            ADLOG(error)("Exception thrown while compiling technique:\n{}",
                         aException.what());
        }
    }

    imguiui::addCombo("Polygon mode",
        mFrameControl.mPolygonMode,
        FrameControl::gPolygonModes.begin(),
        FrameControl::gPolygonModes.end(),
        [](auto aModeIt){return graphics::to_string(*aModeIt);});

    DearImguiWitness witness;

    ImGui::Spacing();
    describe(witness, mTessControl);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Materials"))
    {
        describe(witness, mMaterials);
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Lights"))
    {
        describe(witness, mLights);
    }

    ImGui::End();
}


} // namespace ad
