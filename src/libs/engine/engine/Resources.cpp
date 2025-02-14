#include "Resources.h"

#include <arte/detail/Json.h>

#include <platform/Path.h>

#include <fstream>


namespace ad::renderer {

resource::ResourceFinder makeResourceFinder()
{
    filesystem::path assetConfig = platform::getExecutableFileDirectory() / "assets.json";
    if(exists(assetConfig))
    {
        Json config = Json::parse(std::ifstream{assetConfig});
        
        // This leads to an ambibuity on the path ctor, I suppose because
        // the iterator value_t is equally convertible to both filesystem::path and filesystem::path::string_type
        //return resource::ResourceFinder(config.at("prefixes").begin(),
        //                                config.at("prefixes").end());

        // Take the silly long way
        std::vector<std::string> prefixes{
            config.at("prefixes").begin(),
            config.at("prefixes").end()
        };
        std::vector<std::filesystem::path> prefixPathes;
        prefixPathes.reserve(prefixes.size());
        for (auto & prefix : prefixes)
        {
            prefixPathes.push_back(std::filesystem::canonical(prefix));
        }
        return resource::ResourceFinder(prefixPathes.begin(),
                                        prefixPathes.end());
    }
    else
    {
        return resource::ResourceFinder{platform::getExecutableFileDirectory()};
    }
}


} // namespace ad::renderer
