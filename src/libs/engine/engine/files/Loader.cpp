#include "Loader.h"

#include <engine/log/Logging.h>

#include <arte/detail/Json.h>

#include <fmt/ranges.h>

#include <fstream> 


namespace ad::renderer {


IntrospectProgram Loader::loadProgram(const ReferencePath & aProgFile,
                                      std::vector<graphics::MacroDefine> aDefines) const
{
    std::vector<std::pair<const GLenum, graphics::ShaderSource>> shaders;

    auto programPath = mFinder.pathFor(aProgFile.mPath);

    // TODO factorize and make more robust (e.g. test file existence)
    Json program;
    try 
    {
        program = Json::parse(std::ifstream{programPath});
    }
    catch (Json::parse_error &)
    {
        ADLOG(critical)("Rethrowing exception from attempt to parse Json from '{}'.", programPath.string());
        throw;
    }

    // TODO Copy the constant defines
    //aDefines.insert(aDefines.end(), gClientConstantDefines.begin(), gClientConstantDefines.end());

    // #resource_redesign There are 2 options for the shader path:
    // * relative to the prog file (with FileLookup)
    //graphics::FileLookup lookup{programPath};
    // * or in the "assets" prefixes of ResourceFinder. This is what we do here.

    // Handle the defines at the .prog level before we start preprocessing
    // the individual stages.
    for(std::string macro : program.value("defines", Json{}))
    {
        aDefines.push_back(std::move(macro));
    }

    for (auto [shaderStage, shaderFile] : program.items())
    {
        GLenum stageEnumerator;
        if(shaderStage == "defines")
        {
            // Handled before entering the loop
            continue;
        }
        else if(shaderStage == "vertex")
        {
            stageEnumerator = GL_VERTEX_SHADER;
        }
        else if(shaderStage == "fragment")
        {
            stageEnumerator = GL_FRAGMENT_SHADER;
        }
        else if(shaderStage == "geometry")
        {
            stageEnumerator = GL_GEOMETRY_SHADER;
        }
        else
        {
            ADLOG(critical)("Unable to map shader stage key '{}' to a program stage.", shaderStage);
            throw std::invalid_argument{"Unhandled shader stage key."};
        }
        
        shaders.emplace_back(
            stageEnumerator,
            graphics::ShaderSource::Preprocess(mFinder.pathFor(shaderFile), aDefines));
    }

    ADLOG(debug)("Compiling shader program from '{}', containing {} stages, {defines}.",
                 programPath.string(), shaders.size(),
                 fmt::arg("defines", aDefines.empty() ? 
                    "no defines" 
                    : fmt::format("with defines '{}'", fmt::join(aDefines, ", "))));

    return IntrospectProgram{shaders.begin(), shaders.end(), aProgFile.mPath.filename().string()};
}


graphics::ShaderSource Loader::loadShader(const ReferencePath & aShaderFile) const
{
    return graphics::ShaderSource::Preprocess(mFinder.pathFor(aShaderFile.mPath));
}


} // namespace ad::renderer
