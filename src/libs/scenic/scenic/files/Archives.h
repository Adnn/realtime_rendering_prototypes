#pragma once

#include <fstream>
#include <filesystem>
#include <span>
#include <unordered_map>

#include <handy/Guard.h>

#include <renderer/BufferBase.h>
#include <renderer/ScopeGuards.h>


namespace ad::scenic {

// TODO #serial: move implementations (and even serial() declarations) to a separate file
// it should only be needed to include it from the Serializer implementation.

//
// Output
//
struct FileOutput
{
    FileOutput(const std::filesystem::path& aDestinationFile) :
        mOut{ std::ofstream{aDestinationFile, std::ios_base::binary} }
    {}

    std::ofstream mOut;
};


template <class T> requires std::is_arithmetic_v<T>
FileOutput & serial(FileOutput & aFile, T aData)
{
    aFile.mOut.write((const char *)&aData, sizeof(T));
    return aFile;
}


template <class T>
FileOutput & serial(FileOutput & aFile, std::span<T> aData)
{
    aFile.mOut.write((const char *)aData.data(), aData.size_bytes());
    return aFile;
}


inline FileOutput & serial(FileOutput & aFile, const std::string & aData)
{
    serial(aFile, aData.size());
    serial(aFile, std::span{ aData.data(), aData.size() });
    return aFile;
}


template <class T>
FileOutput & serial(FileOutput & aFile, const std::vector<T> & aData)
{
    // Not sure the size should be serialized here instead of in span? Done for symmetry with read
    serial(aFile, aData.size());
    serial(aFile, std::span{ aData });
    return aFile;
}


template <class T_value, class T_serializer>
FileOutput & serialEach(FileOutput & aFile,
                        const std::vector<T_value> & aData,
                        T_serializer & aSerializer)
{
    serial(aFile, aData.size());
    for (const T_value & value : aData)
    {
        aSerializer.serial(aFile, value);
    }
    return aFile;
}


template <class T_key, class T_value, class T_serializer>
FileOutput & serialEach(FileOutput & aFile,
                        const std::unordered_map<T_key, T_value> & aData,
                        T_serializer & aSerializer)
{
    serial(aFile, aData.size());
    for (const auto & [key, value] : aData)
    {
        // TODO #serial: handle with static polymorphism wether the low-level serial
        // or the serialize is invoked.
        // At the moment: hardcode that only values need the serializer
        serial(aFile, key);
        aSerializer.template serial<FileOutput>(aFile, value);
    }
    return aFile;
}


inline FileOutput & serial(FileOutput & aFile, const graphics::BufferAny & aData)
{
    {
        GLint usage;
        glGetNamedBufferParameteriv(aData, GL_BUFFER_USAGE, &usage);
        serial(aFile, usage);
    }

    GLint byteSize;
    glGetNamedBufferParameteriv(aData, GL_BUFFER_SIZE, &byteSize);
    serial(aFile, byteSize);

    char * content = (char *)glMapNamedBuffer(aData, GL_READ_ONLY);
    Guard mapScope{[&aData](){ glUnmapNamedBuffer(aData); }};
    serial(aFile, std::span{ content, (std::size_t)byteSize });
     
    return aFile;
}


//
// Input
//
struct FileInput
{
    FileInput(const std::filesystem::path& aSourceFile) :
        mIn{ std::ifstream{aSourceFile, std::ios_base::binary} }
    {}

    std::ifstream mIn;
};


template <class T> requires std::is_arithmetic_v<T>
FileInput & serial(FileInput & aFile, T & aData)
{
    aFile.mIn.read((char *)&aData, sizeof(T));
    return aFile;
}


template <class T>
FileInput & serial(FileInput & aFile, std::span<T> aData)
{
    aFile.mIn.read((char *)aData.data(), aData.size_bytes());
    return aFile;
}


inline FileInput & serial(FileInput & aFile, std::string & aData)
{
    std::string::size_type size;
    serial(aFile, size);
    aData.resize(size);
    serial(aFile, std::span{ aData });
    return aFile;
}


template <class T>
FileInput & serial(FileInput & aFile, std::vector<T> & aData)
{
    typename std::vector<T>::size_type size;
    serial(aFile, size);
    // Will only work with default constructible
    aData.resize(size);
    serial(aFile, std::span{ aData });
    return aFile;
}


template <class T_value, class T_serializer>
FileInput & serialEach(FileInput & aFile,
                        std::vector<T_value> & aData,
                        T_serializer & aSerializer)
{
    typename std::vector<T_value>::size_type size;
    serial(aFile, size);
    for (std::size_t i = 0; i != size; ++i)
    {
        // Will only work with default constructible
        T_value value;
        aSerializer.serial(aFile, value);
        aData.push_back(std::move(value));
    }
    return aFile;
}


template <class T_key, class T_value, class T_serializer>
FileInput & serialEach(FileInput & aFile,
                       std::unordered_map<T_key, T_value> & aData,
                       T_serializer & aSerializer)
{
    typename std::unordered_map<T_key, T_value>::size_type size;
    serial(aFile, size);
    for (std::size_t i = 0; i != size; ++i)
    {
        // Will only work with default constructible
        // We remove reference on the key type, which might be const in the map (such as const Semantic)
        std::remove_const_t<T_key> key;
        static_assert(std::is_same_v<std::string, decltype(key)>
                      || std::is_same_v<std::size_t, decltype(key)>);
        T_value value;
        // TODO #serial: handle with static polymorphism wether the low-level serial
        // or the serialize is invoked.
        // At the moment: hardcode that only values need the serializer
        serial(aFile, key);
        aSerializer.serial(aFile, value);
        aData.emplace(std::move(key), std::move(value));
    }
    return aFile;
}


inline FileInput & serial(FileInput & aFile, const graphics::BufferAny & aData)
{
    GLint usage;
    serial(aFile, usage);

    GLint byteSize;
    serial(aFile, byteSize);

    {
        // bind to create the object
        //graphics::ScopedBind{ aData, GL_ARRAY_BUFFER };
        glBindBuffer(GL_ARRAY_BUFFER, aData);
    }
    glNamedBufferData(aData, (GLsizeiptr)byteSize, nullptr, (GLenum)usage);
    char * content = (char *)glMapNamedBuffer(aData, GL_WRITE_ONLY);
    Guard mapScope{[&aData](){ glUnmapNamedBuffer(aData); }};
    serial(aFile, std::span{ content, (std::size_t)byteSize });
     
    return aFile;
}


} // namespce ad::scenic
