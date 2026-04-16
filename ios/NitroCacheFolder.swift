import Foundation
import NitroModules

/// Limits parallel downloads (FlatList-safe). Tunable via `setMaxConcurrentDownloads`.
private final class NitroCacheDownloadGate: @unchecked Sendable {
  static let shared = NitroCacheDownloadGate()
  private let lock = NSLock()
  private let condition = NSCondition()
  private var maxParallel = 4
  private var running = 0

  func setMaxParallel(_ value: Int) {
    lock.lock()
    maxParallel = max(1, min(value, 64))
    condition.broadcast()
    lock.unlock()
  }

  /// Blocks the calling thread until a slot is available.
  func acquire() {
    lock.lock()
    while running >= maxParallel {
      condition.wait()
    }
    running += 1
    lock.unlock()
  }

  func release() {
    lock.lock()
    running -= 1
    condition.broadcast()
    lock.unlock()
  }
}

final class NitroCacheFolder: HybridNitroCacheFolderSpec {
  private static let urlSession: URLSession = {
    let cfg = URLSessionConfiguration.ephemeral
    cfg.httpMaximumConnectionsPerHost = 8
    cfg.timeoutIntervalForRequest = 120
    cfg.timeoutIntervalForResource = 0
    cfg.urlCache = nil
    return URLSession(configuration: cfg)
  }()

  func getCacheDirectory() throws -> String {
    let fm = FileManager.default
    guard let caches = fm.urls(for: .cachesDirectory, in: .userDomainMask).first else {
      throw NSError(
        domain: "NitroCache",
        code: 1,
        userInfo: [NSLocalizedDescriptionKey: "Could not resolve Caches directory"]
      )
    }
    let url = caches.appendingPathComponent("nitro-cache", isDirectory: true)
    try fm.createDirectory(at: url, withIntermediateDirectories: true)
    return url.path
  }

  func deleteFile(relativePath: String) throws -> Bool {
    guard Self.isSafeRelativePath(relativePath) else { return false }
    let rootPath: String
    do {
      rootPath = try getCacheDirectory()
    } catch {
      return false
    }
    let rootURL = URL(fileURLWithPath: rootPath, isDirectory: true)
    let targetURL = rootURL.appendingPathComponent(relativePath, isDirectory: false)
    let fm = FileManager.default
    guard fm.fileExists(atPath: targetURL.path) else { return true }
    do {
      try fm.removeItem(at: targetURL)
      return true
    } catch {
      return false
    }
  }

  func clearCache() throws -> Bool {
    let rootPath: String
    do {
      rootPath = try getCacheDirectory()
    } catch {
      return false
    }
    let fm = FileManager.default
    let names: [String]
    do {
      names = try fm.contentsOfDirectory(atPath: rootPath)
    } catch {
      return false
    }
    for name in names {
      let path = (rootPath as NSString).appendingPathComponent(name)
      do {
        try fm.removeItem(atPath: path)
      } catch {
        return false
      }
    }
    return true
  }

  func setMaxConcurrentDownloads(max: Double) throws {
    NitroCacheDownloadGate.shared.setMaxParallel(Int(max))
  }

  func downloadFile(url: String, relativePath: String) throws -> Promise<DownloadResult> {
    let promise = Promise<DownloadResult>()
    Task {
      do {
        let result = try await Self.downloadFileAsync(
          folder: self,
          urlString: url,
          relativePath: relativePath
        )
        promise.resolve(withResult: result)
      } catch {
        promise.reject(withError: error)
      }
    }
    return promise
  }

  private static func isSafeRelativePath(_ path: String) -> Bool {
    if path.isEmpty || path.hasPrefix("/") || path.hasPrefix("\\") {
      return false
    }
    if path.contains("..") {
      return false
    }
    return true
  }

  private static func downloadFileAsync(
    folder: NitroCacheFolder,
    urlString: String,
    relativePath: String
  ) async throws -> DownloadResult {
    guard isSafeRelativePath(relativePath) else {
      throw NSError(
        domain: "NitroCache",
        code: 10,
        userInfo: [NSLocalizedDescriptionKey: "relativePath must be relative and cannot contain '..'"]
      )
    }
    guard let remote = URL(string: urlString),
          let scheme = remote.scheme?.lowercased(),
          scheme == "http" || scheme == "https" else {
      throw NSError(
        domain: "NitroCache",
        code: 11,
        userInfo: [NSLocalizedDescriptionKey: "Invalid HTTP(S) URL"]
      )
    }

    await withCheckedContinuation { (cont: CheckedContinuation<Void, Never>) in
      DispatchQueue.global(qos: .utility).async {
        NitroCacheDownloadGate.shared.acquire()
        cont.resume()
      }
    }
    defer {
      NitroCacheDownloadGate.shared.release()
    }

    let rootPath = try folder.getCacheDirectory()
    let rootURL = URL(fileURLWithPath: rootPath, isDirectory: true)
    let targetURL = rootURL.appendingPathComponent(relativePath, isDirectory: false)
    let parent = targetURL.deletingLastPathComponent()
    let fm = FileManager.default
    try fm.createDirectory(at: parent, withIntermediateDirectories: true)

    let request = URLRequest(url: remote)
    let (tmpURL, response) = try await urlSession.download(for: request)
    guard let http = response as? HTTPURLResponse else {
      try? fm.removeItem(at: tmpURL)
      throw NSError(
        domain: "NitroCache",
        code: 12,
        userInfo: [NSLocalizedDescriptionKey: "Invalid HTTP response"]
      )
    }
    let status = http.statusCode
    guard (200...299).contains(status) else {
      try? fm.removeItem(at: tmpURL)
      throw NSError(
        domain: "NitroCache",
        code: status,
        userInfo: [NSLocalizedDescriptionKey: "HTTP \(status)"]
      )
    }

    if fm.fileExists(atPath: targetURL.path) {
      try fm.removeItem(at: targetURL)
    }
    try fm.moveItem(at: tmpURL, to: targetURL)

    let attrs = try fm.attributesOfItem(atPath: targetURL.path)
    let size = (attrs[.size] as? NSNumber)?.int64Value ?? 0
    let contentType = http.value(forHTTPHeaderField: "Content-Type")
      ?? "application/octet-stream"

    return DownloadResult(
      filePath: targetURL.path,
      byteCount: Double(size),
      statusCode: Double(status),
      contentType: contentType
    )
  }
}
