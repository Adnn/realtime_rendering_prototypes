#pragma once

#include <reflect/ReflectHelpers.h>

#include <renderer/Shading.h>
#include <renderer/Uniforms.h>

#include <fmt/core.h>


namespace ad {


struct UniformSetterWitness
{
    const graphics::Program & mProgram;
};


// Catch-all
template <class T_value>
void give(UniformSetterWitness & aV, T_value & aValue, const char * aName)
{
    graphics::setUniform(aV.mProgram,
                         fmt::format("u_{}", aName),
                         aValue);
}


template <Numeric T>
void give(UniformSetterWitness & aV, const Clamped<T> & aClamped, const char * aName)
{
    give(aV, aClamped.mValue, aName);
}



} // namespace ad