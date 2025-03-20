#pragma once


#include <concepts>


#define DESCRIBE(type) \
template <class T_witness> \
void describe(T_witness & aWitness, type & aValue)

#define GIVE(member) give(aWitness, aValue.m ## member, #member)
#define GIVE_EX(value, name) give(aWitness, value, #name)


namespace ad {

 
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T_value>
struct Clamped
{
    T_value & mValue;
    T_value mMin;
    T_value mMax;
};

template <Numeric T_value>
struct Interval
{
    T_value mMin = 0;
    T_value mMax = std::numeric_limits<T_value>::max();
};

template <Numeric T_value>
Clamped<T_value> make_Clamped(T_value& aValue, Interval<T_value> aInterval)
{
    return {
        .mValue = aValue,
        .mMin = aInterval.mMin,
        .mMax = aInterval.mMax,
    };
}


} // namespace ad