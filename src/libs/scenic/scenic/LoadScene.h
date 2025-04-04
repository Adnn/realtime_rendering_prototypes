#pragma once


#include "Model.h"

#include <filesystem>


namespace ad::scenic {


struct Context
{
    ModelStorage mStorage;
};


void loadModel(SceneTree & aAppendedScene, const std::filesystem::path & aModelFile, Context& aContext, float aGlobalScale = 1.0f);


} // namespce ad::scenic
