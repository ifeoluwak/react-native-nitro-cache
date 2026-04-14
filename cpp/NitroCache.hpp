#pragma once

#include "HybridNitroCacheSpec.hpp"

namespace margelo::nitro::nitrocache {
    class NitroCache: public HybridNitroCacheSpec {
        public:
            NitroCache(): HybridObject(TAG) {};
            std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> get(const std::string& url) override;
            std::shared_ptr<Promise<std::variant<nitro::NullType, CacheEntry>>> getOrFetch(const std::string& url, const std::optional<CacheOptions>& options) override;
            std::shared_ptr<Promise<std::variant<nitro::NullType, std::shared_ptr<ArrayBuffer>>>> getBuffer(const std::string& url) override;
            std::shared_ptr<Promise<void>> set(const std::string& url, const std::shared_ptr<ArrayBuffer>& buffer, const std::string& contentType, const std::optional<CacheOptions>& options) override;
            std::shared_ptr<Promise<void>> remove(const std::string& url) override;
            std::shared_ptr<Promise<void>> clear() override;
            std::shared_ptr<Promise<void>> configure(const CacheConfig& config) override;
            bool has(const std::string& url) override;
            std::shared_ptr<Promise<CacheStats>> getStats() override;
            std::shared_ptr<Promise<std::vector<CacheEntry>>> getEntries() override;
    };
};