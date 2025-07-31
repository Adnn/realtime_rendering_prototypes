#pragma once


#include <handy/AtomicVariations.h>

#include <atomic>
#include <span>
#include <string_view>


namespace ad {
namespace imguiui {


template <class T_iterator, class F_stringify>
bool addCombo(const char *aLabel,
              T_iterator & aValue,
              T_iterator aFirst, T_iterator aLast,
              F_stringify aToString);

template <class T_enumeration, std::size_t N_spanExtent>
void addCombo(const char *aLabel,
              T_enumeration & aValue,
              std::span<const T_enumeration, N_spanExtent> aAvailableValues);


template <class T_enumeration, std::size_t N_spanExtent>
void addCombo(const char * aLabel,
              std::atomic<T_enumeration> & aValue,
              const std::span<const T_enumeration, N_spanExtent> & aAvailableValues)
{
    T_enumeration value = aValue.load();
    addCombo<T_enumeration, N_spanExtent>(aLabel, value, aAvailableValues);
    aValue.store(value);
}


template <class T_enumeration, std::size_t N_spanExtent>
void addCombo(const char * aLabel,
              MovableAtomic<T_enumeration> & aValue,
              const std::span<const T_enumeration, N_spanExtent> & aAvailableValues)
{
    addCombo<T_enumeration, N_spanExtent>
            (aLabel, static_cast<std::atomic<T_enumeration> &>(aValue), aAvailableValues);
}


/// @brief Implement a combo over a **continous** enumeration, from [0, E_end[.
/// @tparam E_end The end enumerator (usually named "_End", and kept last),
/// or alternatively the size of the enum.
template <auto E_end, class T_enumeration>
void addComboContinuousEnum(const char* aLabel,
                            T_enumeration& aValue);


/// @brief Implement a combo over a numeric (builtins) values
template <class T_numeric, std::size_t N_extent>
    requires std::is_arithmetic_v<T_numeric>
bool addComboNumeric(const char * aLabel,
                     T_numeric & aValue,
                     std::span<const T_numeric, N_extent> aCandidates);


} // namespace imguiui
} // namespace ad
