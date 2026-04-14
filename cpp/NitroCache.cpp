#include "NitroCache.hpp"

#include <NitroModules/HybridObjectRegistry.hpp>

#include <iostream>
#include <filesystem>

#include "CacheEntry.hpp"
#include "Promise.hpp"
#include "ArrayBuffer.hpp"

class CacheStorage
{
    static std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<margelo::nitro::ArrayBuffer>>> cache;
};

std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<margelo::nitro::ArrayBuffer>>> CacheStorage::cache;



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
    }

    NitroCache::NitroCache() : HybridObject(TAG)
    {
        if (nitro::HybridObjectRegistry::hasHybridObject(kNitroFetchHybridName))
        {
            std::shared_ptr<nitro::HybridObject> hybrid = nitro::HybridObjectRegistry::createHybridObject(kNitroFetchHybridName);
            nitroFetch = std::dynamic_pointer_cast<nitro::nitrofetch::HybridNitroFetchSpec>(hybrid);
        }
    }

    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> NitroCache::get(const std::string &url)
    {
        if (!nitroFetch)
        {
            return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(
                std::variant<nitro::NullType, CacheEntry>(nitro::null));
        }
      
      
        std::filesystem::path path = std::filesystem::current_path() / "Library/Caches";

        std::filesystem::directory_iterator it(path);
        for (const auto& entry: it) {
            std::cout << "Entry: " << entry.path() << std::endl;
        }
      
        
      
      std::cout << "Heeeee --" << std::filesystem::current_path() << std::endl;
        auto request = nitro::nitrofetch::NitroRequest();
        request.url = url;
        Promise<nitro::NullType>::async([this, request]()
                                        {
            auto response = this->nitroFetch->createClient()->requestSync(request);
            if (response.status == 200) {
                // std::cout << "Response is: " << response.headers << std::endl;
                for (const auto& header: response.headers) {
                    std::cout << "Header: " << header.key << " " << header.value << std::endl;
                }
            } else {
                std::cout << "Error is: " << response.statusText << std::endl;
            }
            return nitro::null; })
            ->await();
        return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(
            std::variant<nitro::NullType, CacheEntry>(nitro::null));
    }

    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> NitroCache::getOrFetch(const std::string &url, const std::optional<CacheOptions> &options)
    {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };

    std::shared_ptr<Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>> NitroCache::getBuffer(const std::string &url)
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::set(const std::string &url, const std::shared_ptr<ArrayBuffer> &buffer, const std::string &contentType, const std::optional<CacheOptions> &options)
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::clear()
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::configure(const CacheConfig &config)
    {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };

    std::shared_ptr<Promise<void>> NitroCache::remove(const std::string &url)
    {
        std::cout << "fff" << std::endl;
        return nullptr;
    }

    bool NitroCache::has(const std::string &url)
    {
        std::cout << "Heeeee" << std::endl;
        return true;
    };
    std::shared_ptr<Promise<CacheStats>> NitroCache::getStats()
    {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<std::vector<CacheEntry>>> NitroCache::getEntries()
    {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };

}
