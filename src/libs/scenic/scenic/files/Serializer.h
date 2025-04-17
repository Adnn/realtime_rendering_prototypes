#pragma once

// TODO: should be fully forward declared instead, or including this will become too costly
// For quick prototyping, we include (there are nested types)
#include "../Model.h"


namespace ad::scenic {


    struct MeshPart_Naive;
    struct Object;
    struct SceneTree;

    struct FileOutput;
    struct FileInput;

    template<class T_archive, class T_data> 
    struct Serializable
    {
        using Param_t = const T_data &;
    };

    // Partial specialization for Input archives
    template<class T_data>
    struct Serializable<FileInput, T_data>
    {
        using Param_t = T_data &;
    };


// TODO: template the archive at the struct level
struct Serializer
{
    template<class T_archive, class T_data> 
    using Param_t = typename Serializable<T_archive, T_data>::Param_t;

#define PARAM(type) Param_t<T_archive, type>

    // Note: Templates are explicitly instantiated in the implementation file
    // TODO: handle constness of reference with some templatery
    // TODO: place limits on types acceptable for T_archive
    template <class T_archive>
    void serial(T_archive& aArchive, PARAM(SceneTree) aValue);

    template <class T_archive>
    void serial(T_archive& aArchive, PARAM(Object) aValue);

    template <class T_archive>
    void serial(T_archive& aArchive, PARAM(MeshPart_Naive) aValue);

    template <class T_archive>
    void serial(T_archive& aArchive, PARAM(MeshPart_Naive::Accessor_Naive) aValue);

#undef PARAM

};


} // namespce ad::scenic
