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
    using CacheEntryPromise = Promise<std::variant<nitro::NullType, CacheEntry>>;

    std::mutex mutex;

    class CacheStorage
    {
    public:
        // the unordered map of the cache entries
        static CacheMap cache;
    };

    CacheMap CacheStorage::cache;

    namespace
    {
        constexpr const char *kNitroFetchHybridName = "NitroFetch";
        constexpr const char *kNitroCacheFolderHybridName = "NitroCacheFolder";
        constexpr const char *kCacheFileName = "entries.dat";
        constexpr const char *kFormatVersionFileName = "format_version.dat";
        constexpr uint8_t kCacheFormatVersion = 1;

        uint8_t loadedFormatVersion = 0;

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
            return kPlatformCachePath;
        }
        return {};
    }

    void saveEntryToDisk(const std::string &hash, const CacheEntry &entry)
    {
        // get the entries.dat full file path
        std::string path = kPlatformCachePath.string() + "/" + kCacheFileName;
        
        uint32_t hash_len = static_cast<uint32_t>(hash.length());

        std::string relative_path = entry.url;
        uint32_t path_len = static_cast<uint32_t>(relative_path.length());

        uint32_t mime_len = static_cast<uint32_t>(entry.contentType.length());
        uint64_t expires_at = static_cast<uint64_t>(entry.expiresAt);
        uint32_t size = static_cast<uint32_t>(entry.size);

        if (hash_len < 1 || path_len < 1 || mime_len < 1)
        {
            return;
        }

        FILE *file = fopen(path.c_str(), "ab");
        if (file == NULL) {
            return;
        }

        auto hash_len_written = fwrite(reinterpret_cast<const char *>(&hash_len), sizeof(hash_len), 1, file);
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

        fwrite(reinterpret_cast<const char *>(&size), sizeof(size), 1, file);
        fwrite(reinterpret_cast<const char *>(&expires_at), sizeof(expires_at), 1, file);

        fclose(file);
    }

    void saveAllEntriesToDisk()
    {
        // lock the file mutex to prevent multiple threads from writing to the cache file at the same time
        std::lock_guard<std::mutex> lock(mutex);
        
        // get the entries.dat full file path
        std::string path = kPlatformCachePath.string() + "/" + kCacheFileName;
        FILE *file = fopen(path.c_str(), "wb");
        if (file == NULL) {
            return;
        }

        // write the entries
        for (const auto &it: CacheStorage::cache) {
            const std::string& hash = it.first;
            const CacheEntry& entry = it.second;

            
            uint32_t hash_len = static_cast<uint32_t>(hash.length());
            uint32_t path_len = static_cast<uint32_t>(entry.url.length());
            uint32_t mime_len = static_cast<uint32_t>(entry.contentType.length());
            uint32_t size = static_cast<uint32_t>(entry.size);
            uint64_t expires_at = static_cast<uint64_t>(entry.expiresAt);

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
            fwrite(reinterpret_cast<const char *>(&expires_at), sizeof(expires_at), 1, file);
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
        uint64_t expires_at;

        std::string path = kPlatformCachePath.string() + "/" + kCacheFileName;

        FILE *file = fopen(path.c_str(), "rb");

        if (file == NULL) {
            return;
        }

        while (true)
        {
            const auto read_count = fread(&hash_len, sizeof(hash_len), 1, file);
            if (read_count < 1 || feof(file))
            {
                fclose(file);
                break;
            }
            std::vector<char> hash(hash_len + 1);
            hash[hash_len] = '\0';
            fread(hash.data(), 1, hash_len, file);

            fread(&path_len, sizeof(path_len), 1, file);
            if (path_len < 1)
            {
                fclose(file);
                break;
            }
            std::vector<char> file_path(path_len + 1);
            file_path[path_len] = '\0';
            fread(file_path.data(), 1, path_len, file);

            fread(&mime_len, sizeof(mime_len), 1, file);
            if (mime_len < 1)
            {
                fclose(file);
                break;
            }
            std::vector<char> mime_type(mime_len + 1);
            mime_type[mime_len] = '\0';
            fread(mime_type.data(), 1, mime_len, file);

            fread(&size_len, sizeof(size_len), 1, file);
            fread(&expires_at, sizeof(expires_at), 1, file);

            std::string full_path = kPlatformCachePath.string() + "/" + std::string(file_path.data());

            std::cout << "hash path is " << hash.data() << std::endl;

            CacheStorage::cache.emplace(std::string(hash.data()), CacheEntry{
                std::string(file_path.data()), (double)size_len, std::string(mime_type.data()), (double)expires_at
            });
        };

        std::cout << "finished reading map to memory .. " << std::endl;
        fclose(file);
    }

    HybridNitroCache::HybridNitroCache() : HybridObject(TAG)
    {
        if (nitro::HybridObjectRegistry::hasHybridObject(kNitroCacheFolderHybridName))
        {
            // Initialize the folder manager nitro module
            std::shared_ptr<nitro::HybridObject> hybrid =
                nitro::HybridObjectRegistry::createHybridObject(kNitroCacheFolderHybridName);
            folderManager = std::dynamic_pointer_cast<nitro::nitrocache::HybridNitroCacheFolderSpec>(hybrid);

            // Resolve the cache root path - `nitro-cache` folder in the user's cache directory
            const std::filesystem::path path = resolveCacheRoot();
            if (!path.empty() && std::filesystem::exists(path))
            {
                // get format_version.dat file
                std::string format_version_path = kPlatformCachePath.string() + "/" + kFormatVersionFileName;
                FILE *format_version_file = fopen(format_version_path.c_str(), "rb");
                // is this the first time the app is running?
                if (format_version_file == NULL) {
                    std::cout << "format_version.dat file not found, creating new one" << std::endl;
                    // create format_version.dat file and save the cache format version
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    fclose(file);
                    std::cout << "saved new format version to format_version.dat file" << std::endl;
                } else {
                    // read the cache format version from the format_version.dat file
                    fread(&loadedFormatVersion, sizeof(loadedFormatVersion), 1, format_version_file);
                    std::cout << "read cache format version from format_version.dat file: " << (int)loadedFormatVersion << std::endl;
                    fclose(format_version_file);
                }

                if (loadedFormatVersion != kCacheFormatVersion) {
                    std::cout << "cache format version is not supported" << std::endl;
                    // clear all the cache files and start fresh
                    folderManager->clearCache();
                    // save new format version to the format_version.dat file
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    std::cout << "updated new format version to format_version.dat file" << std::endl;
                    fclose(file);
                    return;
                }



                // get entries.dat file
                std::filesystem::path entries_path = kPlatformCachePath.string() + "/" + kCacheFileName;
                std::cout << "entries_path is " << entries_path << std::endl;
                if (std::filesystem::exists(entries_path)) {
                    readMapToMemomry();
                }
            }
            else
            {
                std::cout << "NitroCache: cache root not set (call configure({ directory }) or ensure NitroCacheFolder is linked)"
                          << std::endl;
                          throw std::runtime_error("NitroCache: cache root not set");
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
        // check if the url is in the cache
        std::string hash = folderManager->hashURL(url);
        std::unique_lock<std::mutex> lock(mutex);
        CacheMap::iterator iterator = CacheStorage::cache.find(hash);
        if (iterator != CacheStorage::cache.end())
        {
            CacheEntry value = iterator->second;
            if (value.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (value.expiresAt < static_cast<double>(now)) {
                    std::cout << "entry is expired, removing from cache" << std::endl;
                    CacheStorage::cache.erase(hash);
                    // unlock to allow saveAllEntriesToDisk to safely lock the mutex
                    lock.unlock();

                    saveAllEntriesToDisk();
                    return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
                }
            }
            value.url = kPlatformCachePath.string() + "/" + value.url;
            return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(value));
        }
        return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
    }

    CacheEntryResult HybridNitroCache::getOrFetch(const std::string &url, const std::optional<CacheOptions> &options)
    {
        std::string hash = folderManager->hashURL(url);

        std::unique_lock<std::mutex> lock(mutex);

       auto download = [=, folderManagerAsync = folderManager](){
            try
            {
                std::shared_ptr<Promise<DownloadResult>> downloadPromise = folderManagerAsync->downloadFile(url);
                DownloadResult result = downloadPromise->await().get();
                if (result.statusCode < 200.0 || result.statusCode >= 300.0)
                {
                    return std::variant<nitro::NullType, CacheEntry>(nitro::null);
                }
                std::cout << "Downloaded file successfully, saving to cache" << std::endl;
                CacheEntry entry;
                entry.expiresAt = 0;
                if (options.has_value() && options->ttl.value_or(0) > 0) {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    entry.expiresAt = static_cast<double>(now) + options->ttl.value() * 1000;
                }
                entry.url = result.filePath;
                entry.size = result.byteCount;
                entry.contentType = result.contentType.empty() ? std::string("application/octet-stream") : result.contentType;
                const std::string relativePath = result.filePath.substr(result.filePath.find_last_of('/') + 1);
                
                 // new entry to use relative path e.g 2939djdej39.jpeg
                CacheEntry new_entry(entry);
                new_entry.url = relativePath;

                std::unique_lock<std::mutex> async_lock(mutex);
                CacheStorage::cache[hash] = new_entry;
                async_lock.unlock();

                saveEntryToDisk(hash, new_entry);

                // std::cout << "downloaded new file " << std::endl;

                // return the entry with the full path
                return std::variant<nitro::NullType, CacheEntry>(entry);
            }
            catch (...)
            {
              std::cout << "in cpp errr" << std::endl;
                return std::variant<nitro::NullType, CacheEntry>(nitro::null);
            }
        };

        // if force refresh is true, download the file
        if (options.has_value() && options->forceRefresh.value_or(false)) {
            std::cout << "force refreshing " << url << std::endl;
            CacheStorage::cache.erase(hash);
            lock.unlock();
            saveAllEntriesToDisk();
            return CacheEntryPromise::async(download);
        }

        auto iterator = CacheStorage::cache.find(hash);
        if (iterator != CacheStorage::cache.end()) {
            auto value = iterator->second;
            // if the entry has an expiration time and it is expired, download the file
            if (value.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (value.expiresAt < static_cast<double>(now)) {
                    std::cout << "entry is expired, downloading new file" << std::endl;
                    CacheStorage::cache.erase(hash);
                    lock.unlock();
                    saveAllEntriesToDisk();
                    return CacheEntryPromise::async(download);
                }
            }
            value.url = kPlatformCachePath.string() + "/" + value.url;
            // if the entry has no expiration time or it is not expired, return the entry
            return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(value));
        }

        // if the entry is not in the cache, download the file
        return CacheEntryPromise::async(download);
    };

    FileBuffer HybridNitroCache::getBuffer(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);

        std::string filePath;
        size_t size;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = CacheStorage::cache.find(hash);
            if (it == CacheStorage::cache.end()) {
                return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(nitro::null);
            }
            filePath = kPlatformCachePath.string() + "/" + it->second.url;
            size = static_cast<size_t>(it->second.size);
        }   // lock released
        // now read the file without the lock
        FILE *file = fopen(filePath.c_str(), "rb");
        if (file != NULL) {
            std::vector<char> buffer(size);
            fread(buffer.data(), 1, size, file);
            fclose(file);
            auto _buffer = margelo::nitro::ArrayBuffer::copy(reinterpret_cast<const uint8_t *>(buffer.data()), size);
            return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>(_buffer));
        }
        return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(nitro::null);
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::clear()
    {
      if (!kPlatformCachePath.empty())
        {
          std::unique_lock<std::mutex> lock(mutex);
          auto result = folderManager->clearCache();
          if (result) {
            CacheStorage::cache.clear();
            lock.unlock();
            saveAllEntriesToDisk();
            std::string format_version_path = kPlatformCachePath.string() + "/" + kFormatVersionFileName;
            FILE *file = fopen(format_version_path.c_str(), "wb");
            if (file == NULL) {
              return Promise<void>::resolved();
            }
            fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
            fclose(file);
          }
        } else {
            std::cout << "Folder is empty" << std::endl;
        }
      return Promise<void>::resolved();
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::remove(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);
        std::unique_lock<std::mutex> lock(mutex);
        CacheMap::iterator iterator = CacheStorage::cache.find(hash);
        if (iterator == CacheStorage::cache.end()) {
            return Promise<void>::resolved();
        }
        folderManager->deleteFile(iterator->second.url);
        CacheStorage::cache.erase(hash);
        lock.unlock();
        saveAllEntriesToDisk();
        return Promise<void>::resolved();
    }

    bool HybridNitroCache::has(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "checking if hash is in the cache " << std::endl;
        auto it = CacheStorage::cache.find(hash);
        return it != CacheStorage::cache.end();
    };

    std::shared_ptr<Promise<CacheStats>> HybridNitroCache::getStats()
    {
        // lock the mutex to prevent multiple threads from reading the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        double totalEntries = CacheStorage::cache.size();
        double totalSize = 0;
        for (auto it: CacheStorage::cache) {
            totalSize += it.second.size;
        };
        return Promise<CacheStats>::resolved(CacheStats{ totalEntries, totalSize });
    };

    std::shared_ptr<Promise<std::vector<CacheEntry>>> HybridNitroCache::getEntries()
    {
        // lock the mutex to prevent multiple threads from reading the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<CacheEntry> entries;
        for (auto it: CacheStorage::cache) {
            CacheEntry entry;
            entry.expiresAt = it.second.expiresAt;
            entry.url = kPlatformCachePath.string() + "/" + std::string(it.second.url);
            entry.contentType = it.second.contentType;
            entry.size = it.second.size;
            entries.emplace_back(entry);
        };

        return Promise<std::vector<CacheEntry>>::resolved(std::move(entries));
    };

}
