#include "HybridNitroCache.hpp"

#include "HybridNitroCacheFolderSpec.hpp"
#include <NitroModules/HybridObjectRegistry.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdio.h>

#include <NitroModules/ArrayBuffer.hpp>
#include <NitroModules/Promise.hpp>

#if defined(__ANDROID__)
  #include <android/log.h>
  #define NC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "NitroCache", __VA_ARGS__)
#else
  #include <cstdio>
  #define NC_LOG(...) do { printf("[NitroCache] " __VA_ARGS__); printf("\n"); } while (0)
#endif

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
                    NC_LOG("format_version.dat file not found, creating new one");
                    // create format_version.dat file and save the cache format version
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    fclose(file);
                    NC_LOG("saved new format version to format_version.dat file");
                } else {
                    // read the cache format version from the format_version.dat file
                    fread(&loadedFormatVersion, sizeof(loadedFormatVersion), 1, format_version_file);
                    NC_LOG("read cache format version from format_version.dat file: %d", (int)loadedFormatVersion);
                    fclose(format_version_file);
                }

                if (loadedFormatVersion != kCacheFormatVersion) {
                    NC_LOG("cache format version is not supported");
                    // clear all the cache files and start fresh
                    folderManager->clearCache();
                    // save new format version to the format_version.dat file
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    NC_LOG("updated new format version to format_version.dat file");
                    fclose(file);
                    return;
                }



                // get entries.dat file
                std::filesystem::path entries_path = kPlatformCachePath.string() + "/" + kCacheFileName;
                NC_LOG("entries_path is %s", entries_path.c_str());
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
                    NC_LOG("entry is expired, removing from cache");
                    CacheStorage::cache.erase(hash);
                    lock.unlock();
                    folderManager->deleteFile(value.url);
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

        NC_LOG("android getOrFetching ");

        auto startDownload = [this, hash, url, options]() -> CacheEntryResult {
            auto outPromise = CacheEntryPromise::create();
            auto downloadPromise = folderManager->downloadFile(url);
        
            downloadPromise->addOnResolvedListener(
                [this, hash, options, outPromise](const DownloadResult& result) {
                    try {
                        if (result.statusCode < 200.0 || result.statusCode >= 300.0) {
                            NC_LOG("download non-2xx: %d", (int)result.statusCode);
                            outPromise->resolve(nitro::null);
                            return;
                        }
        
                        CacheEntry entry;
                        entry.expiresAt = 0;
                        if (options.has_value() && options->ttl.value_or(0) > 0) {
                            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count();
                            entry.expiresAt = static_cast<double>(now) + options->ttl.value() * 1000;
                        }
                        entry.url = result.filePath;
                        entry.size = result.byteCount;
                        entry.contentType = result.contentType.empty()
                            ? std::string("application/octet-stream")
                            : result.contentType;
        
                        const std::string relativePath =
                            result.filePath.substr(result.filePath.find_last_of('/') + 1);
        
                        CacheEntry new_entry(entry);
                        new_entry.url = relativePath;
        
                        {
                            std::unique_lock<std::mutex> async_lock(mutex);
                            CacheStorage::cache[hash] = new_entry;
                        }
        
                        saveEntryToDisk(hash, new_entry);
        
                        // Return entry with the full absolute path
                        outPromise->resolve(std::variant<nitro::NullType, CacheEntry>(entry));
                    } catch (const std::exception& e) {
                        NC_LOG("download post-processing failed: %s", e.what());
                        outPromise->resolve(nitro::null);
                    } catch (...) {
                        NC_LOG("download post-processing failed: unknown");
                        outPromise->resolve(nitro::null);
                    }
                }
            );
        
            downloadPromise->addOnRejectedListener(
                [outPromise](const std::exception_ptr& err) {
                    try {
                        if (err) std::rethrow_exception(err);
                    } catch (const std::exception& e) {
                        NC_LOG("download rejected: %s", e.what());
                    } catch (...) {
                        NC_LOG("download rejected: unknown");
                    }
                    outPromise->resolve(nitro::null);
                }
            );
        
            return outPromise;
        };

        auto iterator = CacheStorage::cache.find(hash);

        if (iterator != CacheStorage::cache.end()) {
            auto value = iterator->second;
            // if force refresh is true, download the file
            if (options.has_value() && options->forceRefresh.value_or(false)) {
                NC_LOG("force refreshing %s", url.c_str());
                CacheStorage::cache.erase(hash);
                lock.unlock();
                folderManager->deleteFile(value.url);
                saveAllEntriesToDisk();
                return startDownload();
            }
            // if the entry has an expiration time and it is expired, download the file
            if (value.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (value.expiresAt < static_cast<double>(now)) {
                    NC_LOG("entry is expired, downloading new file");
                    CacheStorage::cache.erase(hash);
                    lock.unlock();
                    folderManager->deleteFile(value.url);
                    saveAllEntriesToDisk();
                    return startDownload();
                }
            }
            value.url = kPlatformCachePath.string() + "/" + value.url;
            // if the entry has no expiration time or it is not expired, return the entry
            return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(value));
        }

        // if the entry is not in the cache, download the file
        lock.unlock();
        return startDownload();
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
            // we need to rewrite the format version since clearCache deletes all the files
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
        std::unique_lock<std::mutex> lock(mutex);
        std::cout << "checking if hash is in the cache " << std::endl;
        auto it = CacheStorage::cache.find(hash);
        if (it != CacheStorage::cache.end()) {
            if (it->second.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (it->second.expiresAt < static_cast<double>(now)) {
                    folderManager->deleteFile(it->second.url);
                    CacheStorage::cache.erase(hash);
                    lock.unlock();
                    saveAllEntriesToDisk();
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    std::shared_ptr<Promise<CacheStats>> HybridNitroCache::getStats()
    {
        // lock the mutex to prevent multiple threads from reading the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        double totalEntries = 0;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        double totalSize = 0;
        for (auto it: CacheStorage::cache) {
            if (it.second.expiresAt > 0) {
                if (it.second.expiresAt < static_cast<double>(now)) {
                  continue;
                }
            }
            totalSize += it.second.size;
            totalEntries += 1;
        };
        return Promise<CacheStats>::resolved(CacheStats{ totalEntries, totalSize });
    };

    std::shared_ptr<Promise<std::vector<CacheEntry>>> HybridNitroCache::getEntries()
    {
        // lock the mutex to prevent multiple threads from reading the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<CacheEntry> entries;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (auto it: CacheStorage::cache) {
            if (it.second.expiresAt > 0) {
                if (it.second.expiresAt < static_cast<double>(now)) {
                    continue;
                }
            }
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
