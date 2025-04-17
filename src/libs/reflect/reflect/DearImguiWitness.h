#pragma once

#include "ReflectHelpers.h"

#include <math/Color.h>
#include <math/Vector.h>

#include <imgui.h>

#include <span>
#include <tuple>


namespace ad {


struct DearImguiWitness
{};


// Catch-all
template <class T_value>
void give(DearImguiWitness & aV, T_value & aValue, const char * aName)
{
    ImGui::TextUnformatted(aName);
    describe(aV, aValue);
}


template <class T, std::size_t Extent>
void give(DearImguiWitness & aV, const std::span<T, Extent> & aSpan, const char * aName)
{
    ImGui::Indent();
    for(std::size_t idx = 0; idx != aSpan.size(); ++idx)
    {
        std::string label = aName + (" #" + std::to_string(idx));
        ImGui::SeparatorText(label.c_str());
        // We have to push an explicit ID on the stack, to distinguish below widgets.
        ImGui::PushID(label.c_str());
        describe(aV, aSpan[idx]);
        ImGui::PopID();
    }
    ImGui::Unindent();
}

// Note: To get each element of the tuple, we need compile-time indices
// so a runtime loop cannot work.
// We rely on an index sequence to get the parameter pack VN_indices.
template <class... VT, std::size_t Extent, std::size_t... VN_indices>
void interleaveSpans(DearImguiWitness & aV,
                     const std::tuple<std::span<VT, Extent>...> & aSpans,
                     std::size_t aElementIdx,
                     std::index_sequence<VN_indices...>)
{
    // Fold expression, to invoke r for each value of the VN_indices pack
    (describe(aV, std::get<VN_indices>(aSpans)[aElementIdx]), ...);
}


/// @brief Handles struct-of-arrays by interleaving elements of all spans.
/// @param aSpans is a tuple of N spans, which must all have the same length.
template <class... VT, std::size_t Extent>
void give(DearImguiWitness & aV,
          const std::tuple<std::span<VT, Extent>...> & aSpans,
          const char * aName)
{
    ImGui::Indent();
    const std::size_t elementCount = std::get<0>(aSpans).size();
    for(std::size_t idx = 0; idx != elementCount; ++idx)
    {
        std::string label = aName + (" #" + std::to_string(idx));
        ImGui::SeparatorText(label.c_str());
        // We have to push an explicit ID on the stack, to distinguish below widgets.
        ImGui::PushID(label.c_str());

        interleaveSpans(
            aV,
            aSpans,
            idx,
            std::index_sequence_for<VT...>{});

        ImGui::PopID();
    }
    ImGui::Unindent();
}


inline void give(DearImguiWitness &, bool & aBool, const char * aName)
{
    ImGui::Checkbox(aName, &aBool);
}


template <class T>
struct ImguiDataType;

template <>
struct ImguiDataType<unsigned int>
{ constexpr static ImGuiDataType_ value = ImGuiDataType_U32; };

template <>
struct ImguiDataType<int>
{ constexpr static ImGuiDataType_ value = ImGuiDataType_S32; };


template <std::integral T>
void give(DearImguiWitness & aV, const Clamped<T> & aClamped, const char * aName)
{
    ImGui::InputScalar(aName, ImguiDataType<T>::value, &aClamped.mValue);
    aClamped.mValue = std::clamp(aClamped.mValue, aClamped.mMin, aClamped.mMax);
}

template <std::floating_point T>
void give(DearImguiWitness & aV, const Clamped<T> & aClamped, const char * aName)
{
    ImGui::SliderFloat(aName, &aClamped.mValue, aClamped.mMin, aClamped.mMax);
}


inline void give(DearImguiWitness & aV, float  & aFloat, const char * aName)
{
    ImGui::InputFloat(aName, &aFloat);
}




inline void give(DearImguiWitness & aV, math::Size<2, float> & aSize, const char * aName)
{
    ImGui::InputFloat2(aName, aSize.data());
}


inline void give(DearImguiWitness & aV, math::Position<2, float> & aPos, const char * aName)
{
    ImGui::InputFloat2(aName, aPos.data());
}


inline void give(DearImguiWitness & aV, math::Position<3, float> & aPos, const char * aName)
{
    ImGui::InputFloat3(aName, aPos.data());
}


inline void give(DearImguiWitness & aV, math::Position<4, float> & aPos, const char * aName)
{
    ImGui::InputFloat4(aName, aPos.data());
}


inline void give(DearImguiWitness & aV, math::Vec<2, float> & aVec, const char * aName)
{
    ImGui::InputFloat2(aName, aVec.data());
}


inline void give(DearImguiWitness & aV, math::Vec<3, float> & aVec, const char * aName)
{
    ImGui::InputFloat3(aName, aVec.data());
}


inline void give(DearImguiWitness & aV, math::Vec<4, float> & aVec, const char * aName)
{
    ImGui::InputFloat4(aName, aVec.data());
}


inline void give(DearImguiWitness & aV, math::UnitVec<3, float> & aVec, const char * aName)
{
    ImGui::InputFloat3(aName, aVec.data());
    aVec.normalize();
}


inline void give(DearImguiWitness & aV, math::hdr::Rgb<float> & aRgb, const char * aName)
{
    aRgb.r() = std::pow(aRgb.r(), 1.f / 2.2f);
    aRgb.g() = std::pow(aRgb.g(), 1.f / 2.2f);
    aRgb.b() = std::pow(aRgb.b(), 1.f / 2.2f);
    ImGui::ColorEdit3(aName, aRgb.data(), ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    aRgb.r() = std::pow(aRgb.r(), 2.2f);
    aRgb.g() = std::pow(aRgb.g(), 2.2f);
    aRgb.b() = std::pow(aRgb.b(), 2.2f);
}


inline void give(DearImguiWitness & aV, math::hdr::Rgba<float> & aRgba, const char * aName)
{
    aRgba.r() = std::pow(aRgba.r(), 1.f / 2.2f);
    aRgba.g() = std::pow(aRgba.g(), 1.f / 2.2f);
    aRgba.b() = std::pow(aRgba.b(), 1.f / 2.2f);
    ImGui::ColorEdit4(aName, aRgba.data(), ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
    aRgba.r() = std::pow(aRgba.r(), 2.2f);
    aRgba.g() = std::pow(aRgba.g(), 2.2f);
    aRgba.b() = std::pow(aRgba.b(), 2.2f);
}



} // namespace ad