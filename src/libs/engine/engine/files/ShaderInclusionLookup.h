#pragma once

#include <renderer/Shading.h> // for ShaderCompilationError

#include <resource/ResourceFinder.h> 

#include <filesystem>
#include <fstream>
#include <stack>


namespace ad {
namespace renderer {


// Note: this could be more generic than the graphics scope

class ShaderInclusionLookup
{
    using path = std::filesystem::path;

    class PopGuard : public std::ifstream
    {
    public:
        PopGuard(std::ifstream && aIn, std::stack<path> & aStack) :
            std::ifstream{std::move(aIn)},
            mStack{aStack}
        {}

        ~PopGuard() override
        {
            mStack.pop();
        }

    private:
        std::stack<path> & mStack;
    };

public:
    ShaderInclusionLookup(const path & aRootFile, const resource::ResourceFinder * aFinder) :
        mFinder{aFinder}
    {
        mPathes.push(canonical(aRootFile));
    }
    
    std::pair<std::unique_ptr<PopGuard>, std::string> operator()(const std::string & aRelativePath)
    {
        path filePath = mPathes.top().parent_path() / aRelativePath;
        if (!is_regular_file(filePath))
        {
            if (auto optionalPath = mFinder->find(aRelativePath))
            {
                filePath = *optionalPath;
            }
            else
            {
                throw graphics::ShaderCompilationError {
                    "GLSL inclusion error",
                    "Cannot include '" + filePath.make_preferred().string() 
                        + "', requested from '" + mPathes.top().string() + "'."};
            }
        }
        // Cannot be called if the file does not exist
        filePath = canonical(filePath);
        mPathes.push(filePath);
        return {
            std::make_unique<PopGuard>(std::ifstream{filePath}, mPathes),
            filePath.string(),
        };

    }

    std::string top() const
    {
        return mPathes.top().string();
    }

private:
    std::stack<path> mPathes;
    const resource::ResourceFinder * mFinder;
};


} // namespace renderer
} // namespace ad