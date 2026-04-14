#include "NitroCache.hpp"
#include <iostream>

#include <react-native-nitro-fetch/HybridNitroFetchSpec.hpp>

namespace margelo::nitro::nitrocache {

    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> NitroCache::get(const std::string& url) {
        std::cout << "Heeeee" << std::endl;
        return Promise<std::variant<nitro::NullType, CacheEntry>>::resolved(
            std::variant<nitro::NullType, CacheEntry>(nitro::null));;
    };
    std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> NitroCache::getOrFetch(const std::string& url, const std::optional<CacheOptions>& options) {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };

    std::shared_ptr<Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>> NitroCache::getBuffer(const std::string& url) {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::set(const std::string& url, const std::shared_ptr<ArrayBuffer>& buffer, const std::string& contentType, const std::optional<CacheOptions>& options) {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::clear() {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<void>> NitroCache::configure(const CacheConfig& config) {
        std::cout << "Nooo" << std::endl;
        return nullptr;
    };

    std::shared_ptr<Promise<void>> NitroCache::remove(const std::string& url) {
        std::cout << "fff" << std::endl;
        return nullptr;
    }
    
    bool NitroCache::has(const std::string& url) {
        std::cout << "Heeeee" << std::endl;
        return true;
    };
    std::shared_ptr<Promise<CacheStats>> NitroCache::getStats() {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };
    std::shared_ptr<Promise<std::vector<CacheEntry>>> NitroCache::getEntries() {
        std::cout << "Heeeee" << std::endl;
        return nullptr;
    };
}