#pragma once


#include "ReflectHelpers.h"

#include <math/Rectangle.h> 

namespace ad::math {


template <class T_witness, class T_number>
void describe(T_witness& aWitness, Rectangle<T_number>& aValue)
{
    GIVE(Position);
    GIVE(Dimension);
}


} // namespace ad::math