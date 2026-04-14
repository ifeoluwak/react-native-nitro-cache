#include "HybridNitroCache.hpp"

#include "HybridNitroCacheFolderSpec.hpp"
#include <NitroModules/HybridObjectRegistry.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <ctime>

#include "CacheEntry.hpp"
#include "DownloadResult.hpp"
#include "Promise.hpp"
#include "ArrayBuffer.hpp"

using CacheMap = std::unordered_map<std::string, std::filesystem::path>;

class CacheStorage
{
    public:
    static CacheMap cache;
};

CacheMap CacheStorage::cache;

// std::shared_ptr<margelo::nitro::ArrayBuffer> toArrayBuffer(
//     const margelo::nitro::nitrofetch::NitroResponse& response) {
//   const std::optional<std::string>& raw = response.bodyBytes.has_value()
//       ? response.bodyBytes
//       : response.bodyString; // or only bodyBytes if you insist on binary
//   if (!raw.has_value() || raw->empty()) {
//     return margelo::nitro::ArrayBuffer::copy(
//         reinterpret_cast<const uint8_t*>(""),
//         0); // or handle empty differently
//   }
//   const std::string& s = *raw;
//   return margelo::nitro::ArrayBuffer::copy(
//       reinterpret_cast<const uint8_t*>(s.data()),
//       s.size());
// }

namespace margelo::nitro::nitrocache
{

    namespace
    {
        constexpr const char *kNitroFetchHybridName = "NitroFetch";
        constexpr const char *kNitroCacheFolderHybridName = "NitroCacheFolder";

        /** Stable relative path under cache dir for a remote URL (no query string in extension). */
        std::string cacheRelativeFileKey(const std::string &url)
        {
            const size_t q = url.find('?');
            const std::string pathOnly = (q == std::string::npos) ? url : url.substr(0, q);
            std::hash<std::string> hashUrl;
            std::ostringstream name;
            name << "downloads/dl-" << hashUrl(url);
            const size_t dot = pathOnly.find_last_of('.');
            if (dot != std::string::npos && dot + 1 < pathOnly.size())
            {
                std::string ext = pathOnly.substr(dot);
                if (ext.size() <= 8 && ext.find('/') == std::string::npos)
                {
                    name << ext;
                }
                else
                {
                    name << ".bin";
                }
            }
            else
            {
                name << ".bin";
            }
            return name.str();
        }
    }

    namespace
    {
        std::once_flag kPlatformCachePathOnce;
        std::filesystem::path kPlatformCachePath;
        bool kPlatformCachePathValid = false;
    }

    std::filesystem::path HybridNitroCache::resolveCacheRoot() const
    {
        if (_cacheDirectoryFromConfig.has_value() && !_cacheDirectoryFromConfig->empty())
        {
            return std::filesystem::path(*_cacheDirectoryFromConfig);
        }
        std::call_once(kPlatformCachePathOnce, [&]()
                       {
            if (folderManager != nullptr) {
              kPlatformCachePath = std::filesystem::path(folderManager->getCacheDirectory());
              kPlatformCachePathValid = true;
            } });
        if (kPlatformCachePathValid)
        {
            std::cout << "resolveCacheRoot is" << kPlatformCachePath << std::endl;
            return kPlatformCachePath;
        }
        return {};
    }

    HybridNitroCache::HybridNitroCache() : HybridObject(TAG)
    {
        // if (nitro::HybridObjectRegistry::hasHybridObject(kNitroFetchHybridName))
        // {
        //     std::shared_ptr<nitro::HybridObject> hybrid = nitro::HybridObjectRegistry::createHybridObject(kNitroFetchHybridName);
        //     nitroFetch = std::dynamic_pointer_cast<nitro::nitrofetch::HybridNitroFetchSpec>(hybrid);

        //     // check if the cache directory exists
        //     const std::filesystem::path path = resolveCacheRoot();
        //     if (!path.empty() && std::filesystem::exists(path))
        //     {
        //         for (const auto &entry : std::filesystem::directory_iterator(path))
        //         {
        //             std::cout << "Entry: " << entry.path() << std::endl;
        //         }
        //     }
        //     else
        //     {
        //         std::cout << "NitroCache: cache root not set (call configure({ directory }) or ensure NitroCacheFolder is linked)"
        //                   << std::endl;
        //     }
        // }
        if (nitro::HybridObjectRegistry::hasHybridObject(kNitroCacheFolderHybridName)) {
            std::shared_ptr<nitro::HybridObject> hybrid =
                nitro::HybridObjectRegistry::createHybridObject(kNitroCacheFolderHybridName);
            folderManager = std::dynamic_pointer_cast<nitro::nitrocache::HybridNitroCacheFolderSpec>(hybrid);

            std::cout << "NitroCacheFolder is linked" << std::endl;
            const std::filesystem::path path = resolveCacheRoot();
            if (!path.empty() && std::filesystem::exists(path))
            {
                for (const auto &entry : std::filesystem::directory_iterator(path))
                {
                    std::cout << "Entry: " << entry.path() << std::endl;
                }
            }
            else
            {
                std::cout << "NitroCache: cache root not set (call configure({ directory }) or ensure NitroCacheFolder is linked)"
                          << std::endl;
            }
        } else {
            // panic and exit
            throw std::runtime_error("NitroCacheFolder is not linked");
        }
    }

    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> HybridNitroCache::get(const std::string &url)
    {
        std::cout << "get is" << url << std::endl;
        // check if the url is in the cache
        CacheMap::iterator iterator = CacheStorage::cache.find(url);
        if (iterator != CacheStorage::cache.end()) {
            std::cout << "url is in the cache" << std::endl;
            auto value = iterator->second;
            CacheEntry entry;
            entry.url = url;
            entry.buffer = nullptr;
            entry.size = 0;
            entry.expiry = std::time(nullptr);
            entry.contentType = "text/plain";
            return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(std::variant<nitro::NullType, CacheEntry>(entry));
        }
        return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
    }

    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> HybridNitroCache::getOrFetch(const std::string &url, const std::optional<CacheOptions> &options)
    {

        // auto entry = get(url);
        // std::cout << "entry is" << entry->isResolved() << std::endl;
        // if (entry->isResolved()) {
        //     return entry;
        // }

        const std::string relativePath = url.substr(url.find_last_of('/') + 1); //cacheRelativeFileKey(url);
        std::cout << "relativePath is" << relativePath << std::endl;
        return Promise<std::variant<nitro::NullType, CacheEntry>>::async([=]()
                                        {
            try
            {
                std::shared_ptr<Promise<DownloadResult>> downloadPromise = folderManager->downloadFile(url, relativePath);
                DownloadResult result = downloadPromise->await().get();
                std::cout << "DownloadResult is" << result.statusCode << std::endl;
                if (result.statusCode < 200.0 || result.statusCode >= 300.0)
                {
                    return std::variant<nitro::NullType, CacheEntry>(nitro::null);
                }
                CacheEntry entry;
                entry.url = url;
                entry.buffer = margelo::nitro::ArrayBuffer::allocate(0);
                entry.size = result.byteCount;
                entry.expiry = static_cast<double>(std::time(nullptr));
                entry.contentType = result.contentType.empty() ? std::string("application/octet-stream") : result.contentType;
                CacheStorage::cache[url] = std::filesystem::path(result.filePath);
                return std::variant<nitro::NullType, CacheEntry>(entry);
            }
            catch (...)
            {
                return std::variant<nitro::NullType, CacheEntry>(nitro::null);
            }
        });
    };

    std::shared_ptr<Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>> HybridNitroCache::getBuffer(const std::string &url)
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> HybridNitroCache::set(const std::string &url, const std::shared_ptr<ArrayBuffer> &buffer, const std::string &contentType, const std::optional<CacheOptions> &options)
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> HybridNitroCache::clear()
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> HybridNitroCache::configure(const CacheConfig &config)
    {
        if (config.directory.has_value() && !config.directory->empty())
        {
            _cacheDirectoryFromConfig = *config.directory;
        }
        return Promise<void>::resolved();
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::remove(const std::string &url)
    {
        std::cout << "fff" << std::endl;
        return nullptr;
    }

    bool HybridNitroCache::has(const std::string &url)
    {
        std::cout << "Heeeee" << std::endl;
        return true;
    };
    std::shared_ptr<Promise<CacheStats>> HybridNitroCache::getStats()
    {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<std::vector<CacheEntry>>> HybridNitroCache::getEntries()
    {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };

}
