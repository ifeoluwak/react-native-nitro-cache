#pragma once

#include "HybridNitroCacheSpec.hpp"
#include "HybridNitroFetchSpec.hpp"
#include "HybridNitroCacheFolderSpec.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace margelo::nitro::nitrocache {
    class HybridNitroCache: public HybridNitroCacheSpec {
        public:
            HybridNitroCache();
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
    
        private:
            std::filesystem::path resolveCacheRoot() const;

            std::shared_ptr<margelo::nitro::nitrofetch::HybridNitroFetchSpec> nitroFetch;
            std::optional<std::string> _cacheDirectoryFromConfig;
            std::shared_ptr<nitro::nitrocache::HybridNitroCacheFolderSpec> folderManager;
    };
};
