#include "Serializer.h"

#include "Archives.h"

#include "../Model.h"


#define SERIAL(type)            \
    template <class T_archive>  \
    void serialTop(T_archive & aArchive, Serializer::Param_t<T_archive, type> aValue)

#define SERIAL_INST(type)   \
    template void serialTop<FileOutput>(FileOutput &, Serializer::Param_t<FileOutput, type>);  \
    template void serialTop<FileInput>(FileInput &, Serializer::Param_t<FileInput, type>);

#define SERIAL_MEM(type)        \
    template <class T_archive>  \
    void Serializer::serial(T_archive & aArchive, Param_t<T_archive, type> aValue)


#define SERIAL_MEM_INST(type)   \
    template void Serializer::serial<FileOutput>(FileOutput &, Param_t<FileOutput, type>); \
    template void Serializer::serial<FileInput>(FileInput &, Param_t<FileInput, type>);

#define SERIAL_RAW(data)        \
    scenic::serial(aArchive, std::span{&(data), 1})


namespace ad::scenic {


SERIAL_MEM(SceneTree)
{
    serialTop(aArchive, aValue.mTree);
    serialEach(aArchive, aValue.mObjectsMap, *this);
}


SERIAL(NodeTree<Pose>)
{
    serial(aArchive, aValue.mHierarchy);
    serial(aArchive, aValue.mLocalPose);
    serial(aArchive, aValue.mGlobalPose);
    serial(aArchive, aValue.mFirstRoot);
}


SERIAL_MEM(Object)
{
    serialEach(aArchive, aValue.mParts, *this);
    SERIAL_RAW(aValue.mAabb);
}


SERIAL_MEM(MeshPart_Naive)
{
    serialEach(aArchive, aValue.mSemanticToAttribute, *this);
    scenic::serial(aArchive, aValue.mIndexBuffer);
    scenic::serial(aArchive, aValue.mIndicesType);
    scenic::serial(aArchive, aValue.mPrimitiveMode);
    scenic::serial(aArchive, aValue.mVertexFirst);
    scenic::serial(aArchive, aValue.mVertexCount);
    scenic::serial(aArchive, aValue.mIndexFirst);
    scenic::serial(aArchive, aValue.mIndicesCount);
    SERIAL_RAW(aValue.mAabb);
}


SERIAL_MEM(MeshPart_Naive::Accessor_Naive)
{
    scenic::serial(aArchive, aValue.mBuffer);
    SERIAL_RAW(aValue.mClientDataFormat);
}

// SceneTree will cascade-instantiate the rest
SERIAL_MEM_INST(SceneTree)


} // namespce ad::scenic