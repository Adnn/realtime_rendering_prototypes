#include "Resources.h"

#include "log/Logging.h"

#include <arte/detail/Json.h>

#include <platform/Path.h>

#include <fmt/ranges.h>
#include <fmt/std.h>

#include <fstream>


namespace ad::renderer {

resource::ResourceFinder makeResourceFinder()
{
    const std::filesystem::path prefixFile{ "assets.json" };
    filesystem::path assetConfig = platform::getExecutableFileDirectory() / prefixFile;
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

        ADLOG(debug)("Initialize resource finder from '{}':\n\t{}",
            prefixFile.string(), fmt::join(prefixPathes, "\n\t"));

        return resource::ResourceFinder(prefixPathes.begin(),
                                        prefixPathes.end());
    }
    else
    {
        ADLOG(debug)("No prefix file '{}', initialize resource finder to executable path: {}",
            prefixFile.string(), platform::getExecutableFileDirectory());

        return resource::ResourceFinder{platform::getExecutableFileDirectory()};
    }
}


} // namespace ad::renderer
