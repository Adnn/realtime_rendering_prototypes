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

#include <scenic/ColorPalettes.h>

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


// This program replicates the results from figure 2
//const std::filesystem::path gProgramPath = "programs/ch10_ltc_ShowBasicLtc.prog";

// This program replicates the plots from the ltc_code repository
// (and lower line of Figure 5 in the paper).
// Use controls alpha and view polar angle, and the sphere show resulting LTC.
//const std::filesystem::path gProgramPath = "programs/ch10_ltc_ShowGgxLtc.prog";


// The integration demo, lighting a sphere from a polygon
const std::filesystem::path gProgramPath = "programs/ch10_ltc_PolygonLight.prog";

const std::filesystem::path gLightProgramPath = "programs/RenderModel_PlainColor.prog";


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
    mSphereVertexSpecification{},
    mSphereIndexBuffer{
        graphics::loadIndexBuffer(mSphereVertexSpecification.mVertexArray,
                                  //std::span{scenic::icosahedron::gIndices},
                                  std::span{mSphere.mIndices},
                                  graphics::BufferHint::StaticDraw)},
    mSurfaceProgram{mEngine.loadProgram(renderer::ReferencePath{gProgramPath})},
    mLightProgram{mEngine.loadProgram(renderer::ReferencePath{gLightProgramPath})},
    mLtcColorMap{GL_TEXTURE_1D},
    mLtc_1{ mEngine.loadDds(renderer::ReferencePath{"textures/ltc_1.dds"}) },
    mLtc_2{ mEngine.loadDds(renderer::ReferencePath{"textures/ltc_2.dds"}) }
{
    getCardlightLayout(std::cout);
    std::cout << std::endl;

    graphics::attachIndexBuffer(mSphereIndexBuffer, mSphereVertexSpecification.mVertexArray);

    graphics::appendToVertexSpecification(
        mSphereVertexSpecification,
        gVertexDescription,
        std::span{mSphere.mVertices},
        graphics::BufferHint::StaticDraw);

    scenic::Position cardVertices[4] = {
        { 0.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f },
    };
    graphics::appendToVertexSpecification(
        mCardLightVertexSpecification,
        { {0, 3, /*offset*/0, graphics::MappedGL<GLfloat>::enumerator}, },
        std::span{cardVertices},
        graphics::BufferHint::StaticDraw);
    graphics::appendToVertexSpecification(
        mCardLightVertexSpecification,
        { {1, 3, /*offset*/0, graphics::MappedGL<GLfloat>::enumerator}, },
        std::span{scenic::quad::gNormals},
        graphics::BufferHint::StaticDraw);

    // Register the camera system with glfw inputs 
    graphics::registerGlfwCallbacks(
        aAppInterface,
        mOrbitalCamera.mOrbitalControl,
        graphics::EscKeyBehaviour::Close,
        // TODO: this is a dirty capture of a parameter given by reference
        &aImgui);

    // Load the LTC color map 
    {
        // Ltc Color Map

        // Just to create the textures in OpenGL (use the opportunity to name them)
        graphics::bind(mLtcColorMap);
        glObjectLabel(GL_TEXTURE, mLtcColorMap, -1, "LtcColorMap");
        graphics::unbind(mLtcColorMap);

        constexpr GLsizei width = std::size(scenic::sdr::gLtcColorMap1D_srgb);
        glTextureStorage1D(mLtcColorMap, 1, GL_RGB8, width);
        glTextureSubImage1D(mLtcColorMap, 0, 0, width,
                            GL_RGB, GL_UNSIGNED_BYTE, scenic::sdr::gLtcColorMap1D_srgb.data());

        glTextureParameteri(mLtcColorMap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(mLtcColorMap, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mLtcColorMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtcColorMap, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtcColorMap, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Ltc 1 and 2
        graphics::bind(mLtc_1);
        glObjectLabel(GL_TEXTURE, mLtc_1, -1, "Ltc_1");
        graphics::bind(mLtc_2);
        glObjectLabel(GL_TEXTURE, mLtc_2, -1, "Ltc_2");
        graphics::unbind(mLtc_2);

        glTextureParameteri(mLtc_1, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(mLtc_1, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mLtc_1, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtc_1, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtc_1, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTextureParameteri(mLtc_2, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(mLtc_2, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(mLtc_2, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtc_2, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(mLtc_2, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }


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
    mLightProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gLightProgramPath });
    mSurfaceProgram =
        mEngine.loadProgram(renderer::ReferencePath{ gProgramPath });
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

    mEntities.mEntities.resize(objectsCount + mLights.mPlanarCount);

    for (std::size_t lightIdx = 0; lightIdx != mLights.mPlanarCount; ++lightIdx)
    {
        const auto& light = mLights.mPlanarLights[lightIdx];
        auto& entity = mEntities.mEntities[objectsCount + lightIdx];
        entity.mLocalToWorld =
            math::trans3d::scale(
                light.mRect.mDimension.width(),
                1.f,
                light.mRect.mDimension.height())
            * math::trans3d::translate<GLfloat>({
                light.mRect.mPosition.x(),
                light.mHeight,
                light.mRect.y()});
        entity.mColorFactor = light.mColors.mSpecularColor;
    }
    loadToBuffer(mEntities, mEntitiesBlockBuffer, graphics::BufferHint::StreamDraw);

    //
    // Materials
    // 
    graphics::loadSingle(mMaterialsBlockBuffer, mMaterials, graphics::BufferHint::StreamDraw);

    //
    // Lights
    //
    graphics::loadSingle(mLightsBlockBuffer, mLights, graphics::BufferHint::StreamDraw);

    //
    // Camera
    //
    mOrbitalCamera.setRatio(math::getRatio<GLfloat>(aRenderResolution));
    graphics::loadSingle(mViewProjectionBuffer,
                         mOrbitalCamera.getViewProjectionBlock(),
                         graphics::BufferHint::StreamDraw);

    // 
    // Textures
    //
    {
        GLint unitIdx = 1;
        glBindTextureUnit(unitIdx, mLtcColorMap);
        graphics::setUniform(mSurfaceProgram, "u_LtcColorMap", unitIdx);

        ++unitIdx;
        glBindTextureUnit(unitIdx, mLtc_1);
        graphics::setUniform(mSurfaceProgram, "u_Ltc_1", unitIdx);

        ++unitIdx;
        glBindTextureUnit(unitIdx, mLtc_2);
        graphics::setUniform(mSurfaceProgram, "u_Ltc_2", unitIdx);
    }

    //
    // LTC
    //
    {
        graphics::setUniform(mSurfaceProgram, "u_alpha", mLtcControl.mAlpha);
        graphics::setUniform(mSurfaceProgram, "u_thetaViewDir", mLtcControl.mViewAngle.data());
    }

    //
    // Pipeline state
    // 
    glPolygonMode(GL_FRONT_AND_BACK, *mPipelineControl.mPolygonMode);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(mSphereVertexSpecification.mVertexArray);
    
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


    glDisable(GL_CULL_FACE);
    glBindVertexArray(mCardLightVertexSpecification.mVertexArray);
    // TODO: should be done only once for each pair of VAO-program
    validateVertexAttributes(mLightProgram);
    glUseProgram(mLightProgram);
    glDrawArraysInstancedBaseInstance(
        GL_TRIANGLE_STRIP,
        0,
        std::size(scenic::quad::gVertices),
        mLights.mPlanarCount,
        sphereCount);
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
        mPipelineControl.mPolygonMode,
        PipelineControl::gPolygonModes.begin(),
        PipelineControl::gPolygonModes.end(),
        [](auto aModeIt){return graphics::to_string(*aModeIt);});

    DearImguiWitness witness;

    ImGui::Spacing();
    describe(witness, mTessControl);

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Ltc"))
    {
        ImGui::DragFloat("Alpha", &mLtcControl.mAlpha, 0.005, 0.f, 1.f);
        ImGui::SliderAngle("View polar angle", &mLtcControl.mViewAngle.data(), 0.f, 90.f);
    }

    //ImGui::Spacing();
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
