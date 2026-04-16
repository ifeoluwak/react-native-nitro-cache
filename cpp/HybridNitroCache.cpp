#include "HybridNitroCache.hpp"

#include "HybridNitroCacheFolderSpec.hpp"
#include <NitroModules/HybridObjectRegistry.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdio.h>

#include "ArrayBuffer.hpp"

namespace margelo::nitro::nitrocache
{
    using CacheMap = std::unordered_map<std::string, CacheEntry>;
    using CacheEntryResult = std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>>;
    using FileBuffer = std::shared_ptr<Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>>;

    class CacheStorage
    {
    public:
        static CacheMap cache;
    };

    CacheMap CacheStorage::cache;

    namespace
    {
        constexpr const char *kNitroFetchHybridName = "NitroFetch";
        constexpr const char *kNitroCacheFolderHybridName = "NitroCacheFolder";

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

    void saveMapToDisk(const std::string &url, const DownloadResult &res)
    {
        std::string path = kPlatformCachePath.string() + "/jj.dat";
        uint32_t url_len = static_cast<uint32_t>(url.length());

        std::string relative_path = res.filePath.substr(res.filePath.find_last_of('/') + 1);
        uint32_t path_len = static_cast<uint32_t>(relative_path.length());

        uint32_t mime_len = static_cast<uint32_t>(res.contentType.length());
        if (url_len < 0 || path_len < 0 || mime_len < 0)
        {
            return;
        }

        FILE *file = fopen(path.c_str(), "wb");

        std::cout << "Saving url == " << url << "of length " << url_len << std::endl;

        fwrite(reinterpret_cast<const char *>(&url_len), sizeof(uint32_t), 1, file);
        if (url_len > 0)
        {
            fwrite(url.c_str(), 1, url_len, file);
        }

        fwrite(reinterpret_cast<const char *>(&path_len), sizeof(path_len), 1, file);
        if (path_len > 0)
        {
            fwrite(relative_path.c_str(), 1, path_len, file);
        }
        fwrite(reinterpret_cast<const char *>(&mime_len), sizeof(mime_len), 1, file);
        if (path_len > 0)
        {
            fwrite(res.contentType.c_str(), 1, mime_len, file);
        }

        const uint32_t size = static_cast<uint32_t>(res.byteCount);
        fwrite(reinterpret_cast<const char *>(&size), sizeof(size), 1, file);

        fclose(file);
    }

    void readMapToMemomry()
    {
        uint32_t url_len;
        uint32_t path_len;
        uint32_t mime_len;
        uint32_t size_len;

        std::string path = kPlatformCachePath.string() + "/jj.dat";

        FILE *file = fopen(path.c_str(), "rb");

        while (true)
        {
            fread(&url_len, sizeof(url_len), 1, file);
            if (url_len < 1)
            {
                fclose(file);
                break;
            }
            char url[url_len + 1];
            url[url_len] = '\0';
            fread(&url, 1, url_len, file);

            fread(&path_len, sizeof(path_len), 1, file);
            if (path_len < 1)
            {
                fclose(file);
                break;
            }
            char file_path[path_len + 1];
            file_path[path_len] = '\0';
            fread(&file_path, 1, path_len, file);

            fread(&mime_len, sizeof(url_len), 1, file);
            if (mime_len < 0)
            {
                fclose(file);
                break;
            }
            char mime_type[mime_len + 1];
            mime_type[mime_len] = '\0';
            fread(&mime_type, 1, mime_len, file);

            fread(&size_len, sizeof(size_len), 1, file);

            std::cout << "FOund url ---" << std::string(url) << std::endl;
            std::cout << "FOund file path ---" << std::string(file_path) << std::endl;
            std::cout << "FOund mime type ---" << std::string(mime_type) << std::endl;
            std::cout << "FOund size ---" << size_len << std::endl;

            std::string full_path = kPlatformCachePath.string() + "/" + std::string(file_path);

            CacheStorage::cache.emplace(std::string(url), CacheEntry{
                full_path, (double)size_len, std::string(mime_type)
            });

            break;
        };

        fclose(file);
    }

    HybridNitroCache::HybridNitroCache() : HybridObject(TAG)
    {
        if (nitro::HybridObjectRegistry::hasHybridObject(kNitroCacheFolderHybridName))
        {
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

                    std::string filename = entry.path().filename();
                    std::cout << "file name is " << filename << std::endl;
                    if (filename == "jj.dat")
                    {
                        readMapToMemomry();
                    }
                }
            }
            else
            {
                std::cout << "NitroCache: cache root not set (call configure({ directory }) or ensure NitroCacheFolder is linked)"
                          << std::endl;
            }
        }
        else
        {
            // panic and exit
            throw std::runtime_error("NitroCacheFolder is not linked");
        }
    }

    CacheEntryResult HybridNitroCache::get(const std::string &url)
    {
        std::cout << "get is" << url << std::endl;
        // check if the url is in the cache
        CacheMap::iterator iterator = CacheStorage::cache.find(url);
        if (iterator != CacheStorage::cache.end())
        {
            auto value = iterator->second;
            std::cout << "url is in the cache" << value.url << std::endl;
            CacheEntry entry;
            entry.url = kPlatformCachePath.string() + "/" + value.url;
            entry.size = value.size;
            entry.contentType = value.contentType;
            return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(std::variant<nitro::NullType, CacheEntry>(entry));
        }
        return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
    }

    CacheEntryResult HybridNitroCache::getOrFetch(const std::string &url, const std::optional<CacheOptions> &options)
    {

        auto entry = get(url);
        if (entry->isResolved()) {
            return entry;
        }

        const std::string relativePath = url.substr(url.find_last_of('/') + 1);
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
                entry.url = result.filePath;
                entry.size = result.byteCount;
                entry.contentType = result.contentType
                .empty() ? std::string("application/octet-stream") : result.contentType;
                CacheStorage::cache[url] = CacheEntry{result.filePath, result.byteCount, std::string(result.contentType)};

                saveMapToDisk(url, result);

                return std::variant<nitro::NullType, CacheEntry>(entry);
            }
            catch (...)
            {
                return std::variant<nitro::NullType, CacheEntry>(nitro::null);
            } });
    };

    FileBuffer HybridNitroCache::getBuffer(const std::string &url)
    {
        auto iterator = CacheStorage::cache.find(url);
        if (iterator != CacheStorage::cache.end()) {
            auto path = iterator->second.url;
            std::cout << "path is " << path << std::endl;
            FILE *file = fopen(path.c_str(), "rb");
          if (file != NULL) {
              std::cout << "file is found" << std::endl;
              size_t size = static_cast<size_t>(iterator->second.size);
              std::cout << "size is " << size << std::endl;
              char buffer[size];
              fread(&buffer, 1, size, file);
              fclose(file);
              auto _buffer = margelo::nitro::ArrayBuffer::copy(reinterpret_cast<const uint8_t *>(buffer), size);
              return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>(_buffer));
          }
            fclose(file);
        }
        return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>(nitro::null));
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::clear()
    {
      if (!kPlatformCachePath.empty())
        {
          auto result = folderManager->clearCache();
          if (result) {
            CacheStorage::cache.clear();
          }
        } else {
            std::cout << "Foler is empty" << std::endl;
        }
      return Promise<void>::resolved();
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::remove(const std::string &url)
    {
        CacheMap::iterator iterator = CacheStorage::cache.find(url);
        if (iterator == CacheStorage::cache.end()) {
            return Promise<void>::resolved();
        }
        folderManager->deleteFile(url);
        CacheStorage::cache.erase(iterator);
        return Promise<void>::resolved();
    }

    bool HybridNitroCache::has(const std::string &url)
    {
        auto it = CacheStorage::cache.find(url);
        return it != CacheStorage::cache.end();
    };

    std::shared_ptr<Promise<CacheStats>> HybridNitroCache::getStats()
    {
        double totalEntries = CacheStorage::cache.size();
        double totalSize = 0;
        for (auto it: CacheStorage::cache) {
            totalSize += it.second.size;
        };
        return Promise<CacheStats>::resolved(CacheStats{ totalEntries, totalSize });
    };

    std::shared_ptr<Promise<std::vector<CacheEntry>>> HybridNitroCache::getEntries()
    {
        std::vector<CacheEntry> entries;
        for (auto it: CacheStorage::cache) {
            CacheEntry entry;
            entry.url = std::string(it.second.url);
            entry.contentType = it.second.contentType;
            entry.size = it.second.size;
            entries.emplace_back(entry);
        };

        return Promise<std::vector<CacheEntry>>::resolved(std::move(entries));
    };

}
