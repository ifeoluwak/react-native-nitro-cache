#include "HybridNitroCache.hpp"

#include "HybridNitroCacheFolderSpec.hpp"
#include <NitroModules/HybridObjectRegistry.hpp>

#include <filesystem>
#include <stdio.h>
#include <list>

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

    namespace
    {
        constexpr const char *kNitroCacheFolderHybridName = "NitroCacheFolder";
        constexpr const char *kCacheFileName = "entries.dat";
        constexpr const char *kFormatVersionFileName = "format_version.dat";
        constexpr uint8_t kCacheFormatVersion = 1;

        size_t kMaxCacheByteSize = 1024 * 1024 * 500; // 500MB
        int kMaxCacheEntryCount = 300; // max number of cache entries

        size_t totalSize = 0;

        uint8_t loadedFormatVersion = 0;

        std::once_flag kPlatformCachePathOnce;
        std::filesystem::path kPlatformCachePath;
        bool kPlatformCachePathValid = false;

        CacheMap cache;
        std::list<std::string> cacheLRU;
        std::mutex mutex;
    }

    std::filesystem::path HybridNitroCache::resolveCacheRoot() const
    {
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

    bool shouldEvictEntries() {
        // eviction should happen first with byte size
        if (totalSize >= kMaxCacheByteSize) {
            return true;
        }
        // then with entry count
        if (cache.size() >= kMaxCacheEntryCount) {
            return true;
        }
        return false;
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


    void saveAllEntriesToDisk_noLock(CacheMap &cache)
    {
        // get the entries.dat full file path
        std::string path = kPlatformCachePath.string() + "/" + kCacheFileName;
        FILE *file = fopen(path.c_str(), "wb");
        if (file == NULL) {
            return;
        }

        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // write the entries
        for (const auto &it: cache) {
            const std::string& hash = it.first;
            const CacheEntry& entry = it.second;

            // check expiration time, skip if expired
            if (entry.expiresAt > 0 && entry.expiresAt < static_cast<double>(now)) {
                continue;
            }
            
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

    void saveAllEntriesToDisk(CacheMap &cache)
    {
        // lock the file mutex to prevent multiple threads from writing to the cache file at the same time
        std::lock_guard<std::mutex> lock(mutex);
        saveAllEntriesToDisk_noLock(cache);
    }

    void readMapToMemomry(CacheMap &cache)
    {
        uint32_t hash_len;
        uint32_t path_len;
        uint32_t mime_len;
        uint32_t size_len;
        uint64_t expires_at;

        // get the entries.dat full file path
        std::string path = kPlatformCachePath.string() + "/" + kCacheFileName;

        FILE *file = fopen(path.c_str(), "rb");

        if (file == NULL) {
            return;
        }

        // get the current time in milliseconds for checking expiration time
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        // read the entries.dat
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

            #ifndef NDEBUG
                NC_LOG("hash path is %s", hash.data());
            #endif

            // check expiration time, skip if expired
            if ((double)expires_at > 0 && (double)expires_at < static_cast<double>(now)) {
                continue;
            }

            if(cache.emplace(std::string(hash.data()), CacheEntry{
                std::string(file_path.data()), (double)size_len, std::string(mime_type.data()), (double)expires_at
            }).second) {
                // only add to the cache if the entry is not already in the cache
                totalSize += size_len;
                // just put the entry in the list, will self correct
                cacheLRU.emplace_front(hash.data());
            };
        };

        #ifndef NDEBUG
            NC_LOG("finished reading map to memory .. ");
        #endif
        fclose(file);
    }

    void HybridNitroCache::evictLeastRecentlyUsed() {
        // prune until the cache is at a reasonable level (480MB or 290 items)
        while (totalSize > 480 * 1024 * 1024 || cache.size() > kMaxCacheEntryCount - 10) {
            #ifndef NDEBUG
                NC_LOG("evicting least recently used entry, totalSize is %d, cache size is %d", totalSize, cache.size());
            #endif
            if (cacheLRU.empty()) {
                #ifndef NDEBUG
                    NC_LOG("cacheLRU is empty, breaking");
                #endif
                break;
            }
            std::string leastUsedHash = cacheLRU.back();
            auto iterator = cache.find(leastUsedHash);
            if (iterator == cache.end()) {
                #ifndef NDEBUG
                    NC_LOG("entry not found in cache, breaking");
                #endif
                break;
            }
            cacheLRU.pop_back();
            totalSize -= iterator->second.size;
            folderManager->deleteFile(iterator->second.url);
            cache.erase(iterator);
            #ifndef NDEBUG
                NC_LOG("evicted entry from cache, totalSize is %d", totalSize);
                NC_LOG("cache size is %d", cache.size());
            #endif
        }
        #ifndef NDEBUG
            NC_LOG("Pruned to reasonable level");
        #endif
        saveAllEntriesToDisk_noLock(cache);
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
                    #ifndef NDEBUG
                        NC_LOG("format_version.dat file not found, creating new one");
                    #endif
                    // create format_version.dat file and save the cache format version
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    fclose(file);
                    #ifndef NDEBUG
                        NC_LOG("saved new format version to format_version.dat file");
                    #endif
                } else {
                    // read the cache format version from the format_version.dat file
                    fread(&loadedFormatVersion, sizeof(loadedFormatVersion), 1, format_version_file);
                    
                    #ifndef NDEBUG
                        NC_LOG("read cache format version from format_version.dat file: %d", (int)loadedFormatVersion);
                    #endif
                    fclose(format_version_file);
                }

                // if the format version on disk is different from current format version, clear the cache and start fresh
                if (loadedFormatVersion != kCacheFormatVersion) {
                    #ifndef NDEBUG
                        NC_LOG("cache format version is not supported. clearing cache and starting fresh");
                    #endif
                    // clear all the cache files and start fresh
                    folderManager->clearCache();
                    // save new format version to the format_version.dat file
                    FILE *file = fopen(format_version_path.c_str(), "wb");
                    if (file == NULL) {
                        return;
                    }
                    fwrite(reinterpret_cast<const char *>(&kCacheFormatVersion), sizeof(kCacheFormatVersion), 1, file);
                    loadedFormatVersion = kCacheFormatVersion;
                    #ifndef NDEBUG
                        NC_LOG("updated new format version to format_version.dat file");
                    #endif
                    fclose(file);
                    return;
                }

                // get entries.dat file, where all the cache entries are stored
                std::filesystem::path entries_path = kPlatformCachePath.string() + "/" + kCacheFileName;
                #ifndef NDEBUG
                    NC_LOG("entries_path is %s", entries_path.c_str());
                #endif
                if (std::filesystem::exists(entries_path)) {
                    readMapToMemomry(cache);
                }
            }
            else
            {
                #ifndef NDEBUG
                    NC_LOG("NitroCache: cache root not set (call configure({ directory }) or ensure NitroCacheFolder is linked)");
                #endif
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
        // TODO: validate if valid url
        
        // check if the url is in the cache
        std::string hash = folderManager->hashURL(url);
        std::unique_lock<std::mutex> lock(mutex);
        CacheMap::iterator iterator = cache.find(hash);
        if (iterator != cache.end())
        {
            CacheEntry value = iterator->second;
            if (value.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (value.expiresAt < static_cast<double>(now)) {
                    #ifndef NDEBUG
                        NC_LOG("entry is expired, removing from cache");
                    #endif
                    cache.erase(iterator);
                    cacheLRU.remove(hash);
                    totalSize -= value.size;
                    lock.unlock();
                    folderManager->deleteFile(value.url);
                    saveAllEntriesToDisk(cache);
                    return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
                }
            }
            // remove the hash from the LRU list and add it to the front
            cacheLRU.remove(hash);
            cacheLRU.push_front(hash);
            // add the folder path with the file name
            value.url = kPlatformCachePath.string() + "/" + value.url;
            return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(value));
        }
        return CacheEntryPromise::resolved(std::variant<nitro::NullType, CacheEntry>(nitro::null));
    }

    CacheEntryResult HybridNitroCache::getOrFetch(const std::string &url, const std::optional<CacheOptions> &options)
    {
        std::string hash = folderManager->hashURL(url);

        std::unique_lock<std::mutex> lock(mutex);

        //check if we have reached the limit
        if (shouldEvictEntries()) {
            // evict the least recently used entry
            evictLeastRecentlyUsed();
        }

        // create a lambda function to start the download
        auto startDownload = [this, hash, url, options]() -> CacheEntryResult {
            auto outPromise = CacheEntryPromise::create();
            auto downloadPromise = folderManager->downloadFile(url);
        
            downloadPromise->addOnResolvedListener(
                [hash, options, outPromise](const DownloadResult& result) {
                    try {
                        // if the status code is not 200 or 300, return null
                        if (result.statusCode < 200.0 || result.statusCode >= 300.0) {
                            outPromise->resolve(nitro::null);
                            return;
                        }

                        {
                            // ensure the hash does not already exist in the cache after the download is complete
                            // this can happen if two threads download the same file almost at the same time
                            std::unique_lock<std::mutex> async_lock(mutex); // lock for reading
                            if (cache.find(hash) != cache.end()) {
                                CacheEntry entry = cache[hash];
                                entry.url = kPlatformCachePath.string() + "/" + entry.url;
                                outPromise->resolve(std::variant<nitro::NullType, CacheEntry>(entry));
                                return;
                            }
                            // auto unlock the mutex after the scope ends
                        }
        
                        // create a new cache entry
                        CacheEntry entry;
                        entry.expiresAt = 0;

                        // if the ttl is provided, set the expiration time
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
        
                        // get the relative path (file name) of the file, e.g 123456.pdf from /path/to/123456.pdf
                        const std::string relativePath =
                            result.filePath.substr(result.filePath.find_last_of('/') + 1);
        
                        // create a new cache entry with the relative path i.e file name
                        // this will be used to save the cache entry to disk and cache map
                        CacheEntry new_entry(entry);
                        new_entry.url = relativePath;
        
                        {
                            // lock the mutex to prevent multiple threads from editing the cache at the same time
                            std::unique_lock<std::mutex> async_lock(mutex);
                            cache[hash] = new_entry;
                            cacheLRU.push_front(hash);
                            totalSize += new_entry.size;
                            saveEntryToDisk(hash, new_entry);
                            // auto unlock the mutex after the scope ends
                        }
                
                        // Return entry with the full absolute path
                        outPromise->resolve(std::variant<nitro::NullType, CacheEntry>(entry));
                    } catch (const std::exception& e) {
                        #ifndef NDEBUG
                            NC_LOG("download post-processing failed: %s", e.what());
                        #endif
                        outPromise->resolve(nitro::null);
                    } catch (...) {
                        #ifndef NDEBUG
                            NC_LOG("download post-processing failed: unknown");
                        #endif
                        outPromise->resolve(nitro::null);
                    }
                }
            );
        
            // if the download fails, return null
            downloadPromise->addOnRejectedListener(
                [outPromise](const std::exception_ptr& err) {
                    try {
                        if (err) std::rethrow_exception(err);
                    } catch (const std::exception& e) {
                        #ifndef NDEBUG
                            NC_LOG("download rejected: %s", e.what());
                        #endif
                    } catch (...) {
                        #ifndef NDEBUG
                            NC_LOG("download rejected: unknown");
                        #endif
                    }
                    outPromise->resolve(nitro::null);
                }
            );
        
            return outPromise;
        };

        auto iterator = cache.find(hash);

        if (iterator != cache.end()) {
            auto value = iterator->second;
            // if force refresh is true, download the file
            if (options.has_value() && options->forceRefresh.value_or(false)) {
                #ifndef NDEBUG
                    NC_LOG("force refreshing %s", url.c_str());
                #endif
                cache.erase(iterator);
                cacheLRU.remove(hash);
                totalSize -= value.size;
                lock.unlock();
                folderManager->deleteFile(value.url);
                saveAllEntriesToDisk(cache);
                return startDownload();
            }
            // if the entry has an expiration time and it is expired, download the file
            if (value.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (value.expiresAt < static_cast<double>(now)) {
                    #ifndef NDEBUG
                        NC_LOG("entry is expired, downloading new file");
                    #endif
                    cache.erase(iterator);
                    cacheLRU.remove(hash);
                    totalSize -= value.size;
                    lock.unlock();
                    folderManager->deleteFile(value.url);
                    saveAllEntriesToDisk(cache);
                    return startDownload();
                }
            }
            // remove the hash from the LRU list and add it to the front
            cacheLRU.remove(hash);
            cacheLRU.push_front(hash);
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
        // try to get the size and file path quickly without locking for a long time
        {
            std::lock_guard<std::mutex> lock(mutex); // deconstructs after the scope ends
            auto it = cache.find(hash);
            if (it == cache.end()) {
                return Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>::resolved(nitro::null);
            }
            filePath = kPlatformCachePath.string() + "/" + it->second.url;
            size = static_cast<size_t>(it->second.size);

            // remove the hash from the LRU list and add it to the front
            cacheLRU.remove(hash);
            cacheLRU.push_front(hash);
        }
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
            cache.clear();
            cacheLRU.clear();
            totalSize = 0;
            lock.unlock();
            saveAllEntriesToDisk(cache);
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
            #ifndef NDEBUG
                NC_LOG("Folder is empty");
            #endif
        }
      return Promise<void>::resolved();
    };

    std::shared_ptr<Promise<void>> HybridNitroCache::remove(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);
        std::unique_lock<std::mutex> lock(mutex);
        CacheMap::iterator iterator = cache.find(hash);
        if (iterator == cache.end()) {
            return Promise<void>::resolved();
        }
        folderManager->deleteFile(iterator->second.url);
        totalSize -= iterator->second.size;
        cache.erase(iterator);
        cacheLRU.remove(hash);
        lock.unlock();
        saveAllEntriesToDisk(cache);
        return Promise<void>::resolved();
    }

    bool HybridNitroCache::has(const std::string &url)
    {
        std::string hash = folderManager->hashURL(url);
        std::unique_lock<std::mutex> lock(mutex);
        auto it = cache.find(hash);
        if (it != cache.end()) {
            if (it->second.expiresAt > 0) {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if (it->second.expiresAt < static_cast<double>(now)) {
                    folderManager->deleteFile(it->second.url);
                    totalSize -= it->second.size;
                    cache.erase(it);
                    cacheLRU.remove(hash);
                    lock.unlock();
                    saveAllEntriesToDisk(cache);
                    return false;
                }
            }
            // add to the LRU list
            cacheLRU.remove(hash);
            cacheLRU.push_front(hash);
            return true;
        }
        return false;
    };

    std::shared_ptr<Promise<CacheStats>> HybridNitroCache::getStats()
    {
        // lock the mutex to prevent multiple threads from editing the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        double totalEntries = 0;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        double _totalSize = 0;
        for (auto it: cache) {
            if (it.second.expiresAt > 0 && it.second.expiresAt < static_cast<double>(now)) {
                continue;
            }
            _totalSize += it.second.size;
            totalEntries += 1;
        };
        return Promise<CacheStats>::resolved(CacheStats{ totalEntries, _totalSize });
    };

    std::shared_ptr<Promise<std::vector<CacheEntry>>> HybridNitroCache::getEntries()
    {
        // lock the mutex to prevent multiple threads from editing the cache at the same time
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<CacheEntry> entries;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (auto it: cache) {
            if (it.second.expiresAt > 0 && it.second.expiresAt < static_cast<double>(now)) {
                continue;
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
