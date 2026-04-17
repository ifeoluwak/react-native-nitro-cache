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
            // std::cout << "resolveCacheRoot is" << kPlatformCachePath << std::endl;
            return kPlatformCachePath;
        }
        return {};
    }

    void saveEntryToDisk(const std::string &hash, const CacheEntry &entry)
    {
        std::string path = kPlatformCachePath.string() + "/jj.dat";
        
        uint32_t hash_len = static_cast<uint32_t>(hash.length());

        std::string relative_path = entry.url;
        uint32_t path_len = static_cast<uint32_t>(relative_path.length());

        uint32_t mime_len = static_cast<uint32_t>(entry.contentType.length());
        if (hash_len < 0 || path_len < 0 || mime_len < 0)
        {
            return;
        }

        FILE *file = fopen(path.c_str(), "ab");

        auto hash_len_written = fwrite(reinterpret_cast<const char *>(&hash_len), sizeof(uint32_t), 1, file);
        if (hash_len_written > 0)
        {
            fwrite(hash.c_str(), 1, hash_len, file);
        }

        auto path_len_written = fwrite(reinterpret_cast<const char *>(&path_len), sizeof(path_len), 1, file);
        if (path_len_written > 0)
        {
            fwrite(relative_path.c_str(), 1, path_len, file);
        }
        auto mime_len_written = fwrite(reinterpret_cast<const char *>(&mime_len), sizeof(mime_len), 1, file);
        if (mime_len_written > 0)
        {
            fwrite(entry.contentType.c_str(), 1, mime_len, file);
        }

        const uint32_t size = static_cast<uint32_t>(entry.size);
        fwrite(reinterpret_cast<const char *>(&size), sizeof(size), 1, file);

        fclose(file);
    }

    void saveAllEntriesToDisk()
    {
        std::string path = kPlatformCachePath.string() + "/jj.dat";
        FILE *file = fopen(path.c_str(), "wb");
        if (file == NULL) {
            return;
        }
        for (const auto &it: CacheStorage::cache) {
            const std::string& hash = it.first;
            const CacheEntry& entry = it.second;

            
            uint32_t hash_len = static_cast<uint32_t>(hash.length());
            uint32_t path_len = static_cast<uint32_t>(entry.url.length());
            uint32_t mime_len = static_cast<uint32_t>(entry.contentType.length());
            uint32_t size = static_cast<uint32_t>(entry.size);

            const auto write_result = fwrite(reinterpret_cast<const char *>(&hash_len), sizeof(uint32_t), 1, file);
            if (write_result > 0) {
                fwrite(hash.c_str(), 1, hash_len, file);
            } else {
                continue;
            }

            fwrite(reinterpret_cast<const char *>(&path_len), sizeof(path_len), 1, file);
            fwrite(entry.url.c_str(), 1, path_len, file);
            fwrite(reinterpret_cast<const char *>(&mime_len), sizeof(mime_len), 1, file);
            fwrite(entry.contentType.c_str(), 1, mime_len, file);
            fwrite(reinterpret_cast<const char *>(&size), sizeof(size), 1, file);
        }
        fflush(file);
        fclose(file);
    }

    void readMapToMemomry()
    {
        uint32_t hash_len;
        uint32_t path_len;
        uint32_t mime_len;
        uint32_t size_len;

        std::string path = kPlatformCachePath.string() + "/jj.dat";

        FILE *file = fopen(path.c_str(), "rb");

        while (true)
        {
            const auto read_count = fread(&hash_len, sizeof(hash_len), 1, file);
            if (read_count < 1 || feof(file))
            {
                fclose(file);
                break;
            }
            char hash[hash_len + 1];
            hash[hash_len] = '\0';
            fread(&hash, 1, hash_len, file);

            fread(&path_len, sizeof(path_len), 1, file);
            if (path_len < 1)
            {
                fclose(file);
                break;
            }
            char file_path[path_len + 1];
            file_path[path_len] = '\0';
            fread(&file_path, 1, path_len, file);

            fread(&mime_len, sizeof(mime_len), 1, file);
            if (mime_len < 0)
            {
                fclose(file);
                break;
            }
            char mime_type[mime_len + 1];
            mime_type[mime_len] = '\0';
            fread(&mime_type, 1, mime_len, file);

            fread(&size_len, sizeof(size_len), 1, file);

            std::string full_path = kPlatformCachePath.string() + "/" + std::string(file_path);

            std::cout << "hash path is " << hash << std::endl;

            CacheStorage::cache.emplace(std::string(hash), CacheEntry{
                std::string(file_path), (double)size_len, std::string(mime_type)
            });
        };

        std::cout << "finished reading map to memory .. " << std::endl;
        fclose(file);
    }

    HybridNitroCache::HybridNitroCache() : HybridObject(TAG)
    {
        if (nitro::HybridObjectRegistry::hasHybridObject(kNitroCacheFolderHybridName))
        {
            std::shared_ptr<nitro::HybridObject> hybrid =
                nitro::HybridObjectRegistry::createHybridObject(kNitroCacheFolderHybridName);
            folderManager = std::dynamic_pointer_cast<nitro::nitrocache::HybridNitroCacheFolderSpec>(hybrid);

            const std::filesystem::path path = resolveCacheRoot();
            if (!path.empty() && std::filesystem::exists(path))
            {
                for (const auto &entry : std::filesystem::directory_iterator(path))
                {
                    // std::cout << "Entry: " << entry.path() << std::endl;

                    std::string filename = entry.path().filename();
                    // std::cout << "file name is " << filename << std::endl;
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

    CacheEntryResult HybridNitroCache::get(const std::string &hash)
    {
        // check if the url is in the cache
        CacheMap::iterator iterator = CacheStorage::cache.find(hash);
        if (iterator != CacheStorage::cache.end())
        {
            auto value = iterator->second;
            std::cout << "hash is in the cache " << hash << std::endl;
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
        std::string hash = folderManager->hashURL(url);
        if (has(hash)) {
            std::cout << "returning from cache " << hash << std::endl;
            return get(hash);
        }

        return Promise<std::variant<nitro::NullType, CacheEntry>>::async([=]()
                                                                         {
            try
            {
                std::shared_ptr<Promise<DownloadResult>> downloadPromise = folderManager->downloadFile(url);
                DownloadResult result = downloadPromise->await().get();
                if (result.statusCode < 200.0 || result.statusCode >= 300.0)
                {
                    return std::variant<nitro::NullType, CacheEntry>(nitro::null);
                }
                CacheEntry entry;
                entry.url = result.filePath;
                entry.size = result.byteCount;
                entry.contentType = result.contentType.empty() ? std::string("application/octet-stream") : result.contentType;
                const std::string relativePath = result.filePath.substr(result.filePath.find_last_of('/') + 1);
                
                 // new entry to use relative path e.g 2939djdej39.jpeg
                CacheEntry new_entry(entry);
                new_entry.url = relativePath;
                CacheStorage::cache[hash] = new_entry;
                saveEntryToDisk(hash, new_entry);

                // return the old entry with the full path
                return std::variant<nitro::NullType, CacheEntry>(entry);
            }
            catch (...)
            {
              std::cout << "in cpp errr" << std::endl;
                return std::variant<nitro::NullType, CacheEntry>(nitro::null);
        } });
    };

    FileBuffer HybridNitroCache::getBuffer(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);
        auto iterator = CacheStorage::cache.find(hash);
        if (iterator != CacheStorage::cache.end()) {
            auto path = kPlatformCachePath.string() + "/" + iterator->second.url;
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
            std::cout << "Folder is empty" << std::endl;
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
        saveAllEntriesToDisk();
        return Promise<void>::resolved();
    }

    bool HybridNitroCache::has(const std::string &hash)
    {
        std::cout << "checking if hash is in the cache " << std::endl;
        auto it = CacheStorage::cache.find(hash);
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
            entry.url = kPlatformCachePath.string() + "/" + std::string(it.second.url);
            entry.contentType = it.second.contentType;
            entry.size = it.second.size;
            entries.emplace_back(entry);
        };

        return Promise<std::vector<CacheEntry>>::resolved(std::move(entries));
    };

}
