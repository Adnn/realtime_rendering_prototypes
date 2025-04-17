#pragma once


#include "Environment.h"

#include <engine/IntrospectProgram.h>


namespace ad::scenic {


void passSkyboxBase(const renderer::IntrospectProgram& aProgram,
                    const EnvironmentMap& aMap,
                    GLenum aCulledFace,
                    GLenum aPolygonMode = GL_FILL);


} // namespce ad::scenic
