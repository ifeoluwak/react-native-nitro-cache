package com.margelo.nitro.nitrocache

import android.app.Application
import android.webkit.MimeTypeMap
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

@Suppress("PrivateApi")
private fun currentApplication(): Application {
  val activityThread = Class.forName("android.app.ActivityThread")
  val app = activityThread.getMethod("currentApplication").invoke(null) as? Application
  return app ?: error("NitroCacheFolder: no Application (ActivityThread.currentApplication is null)")
}

private object DownloadGate {
  private var maxParallel = 6
  private var running = 0
  private val lock = Object()

  fun setMaxParallel(n: Int) {
    synchronized(lock) {
      maxParallel = n.coerceIn(1, 64)
      lock.notifyAll()
    }
  }

  fun acquire() {
    synchronized(lock) {
      while (running >= maxParallel) {
        lock.wait()
      }
      running++
    }
  }

  fun release() {
    synchronized(lock) {
      if (running > 0) {
        running--
      }
      lock.notifyAll()
    }
  }
}

@DoNotStrip
class NitroCacheFolder : HybridNitroCacheFolderSpec() {
  override fun getCacheDirectory(): String {
    val dir = File(currentApplication().cacheDir, "nitro-cache")
    dir.mkdirs()
    return dir.absolutePath
  }

  override fun setMaxConcurrentDownloads(max: Double) {
    DownloadGate.setMaxParallel(max.toInt())
  }

  override fun deleteFile(relativePath: String): Boolean {
    if (!isSafeRelativePath(relativePath)) return false
    return try {
      val f = File(getCacheDirectory(), relativePath)
      if (!f.exists()) true else f.deleteRecursively()
    } catch (_: Throwable) {
      false
    }
  }

  override fun clearCache(): Boolean {
    return try {
      val root = File(getCacheDirectory())
      val children = root.listFiles() ?: return true
      children.all { it.deleteRecursively() }
    } catch (_: Throwable) {
      false
    }
  }

  @Suppress("UNUSED_PARAMETER")
  override fun downloadFile(url: String): Promise<DownloadResult> {
    val promise = Promise<DownloadResult>()
    val parsed = try {
      URL(url)
    } catch (e: Throwable) {
      promise.reject(e)
      return promise
    }
    if (parsed.protocol != "http" && parsed.protocol != "https") {
      promise.reject(IllegalArgumentException("Invalid HTTP(S) URL"))
      return promise
    }

    Thread {
      var acquired = false
      try {
        DownloadGate.acquire()
        acquired = true
        val result = downloadBlocking(url)
        promise.resolve(result)
      } catch (e: Throwable) {
        promise.reject(e)
      } finally {
        if (acquired) {
          DownloadGate.release()
        }
      }
    }.start()

    return promise
  }

  override fun hashURL(url: String): String {
    val bytes = url.toByteArray(Charsets.UTF_8)
    val md = MessageDigest.getInstance("SHA-256")
    val digest = md.digest(bytes)
    return digest.joinToString("") { b -> "%02x".format(b.toInt() and 0xFF) }
  }

  private fun normalizedMimeType(contentType: String): String {
    val primary = contentType.substringBefore(';', missingDelimiterValue = contentType).trim().lowercase()
    return primary.ifEmpty { "application/octet-stream" }
  }

  private fun fileExtensionFromContentType(contentType: String): String {
    val mime = normalizedMimeType(contentType)
    val ext = MimeTypeMap.getSingleton().getExtensionFromMimeType(mime)
    return if (ext.isNullOrEmpty()) "bin" else ext.lowercase()
  }

  private fun downloadBlocking(url: String): DownloadResult {
    val root = File(getCacheDirectory())

    val conn = (URL(url).openConnection() as HttpURLConnection).apply {
      instanceFollowRedirects = true
      connectTimeout = 120_000
      readTimeout = 120_000
      requestMethod = "GET"
    }
    try {
      conn.connect()
      val code = conn.responseCode
      if (code !in 200..299) {
        throw RuntimeException("HTTP $code")
      }
      val contentType = conn.contentType ?: "application/octet-stream"
      val ext = fileExtensionFromContentType(contentType)
      val hash = hashURL(url)
      val relative = "$hash.$ext"
      if (!isSafeRelativePath(relative)) {
        throw RuntimeException("Derived relative path is invalid")
      }
      val outFile = File(root, relative)
      outFile.parentFile?.mkdirs()
      conn.inputStream.use { input ->
        FileOutputStream(outFile, false).use { output ->
          val buf = ByteArray(64 * 1024)
          while (true) {
            val n = input.read(buf)
            if (n <= 0) break
            output.write(buf, 0, n)
          }
        }
      }
      val size = outFile.length().toDouble()
      return DownloadResult(
        filePath = outFile.absolutePath,
        byteCount = size,
        statusCode = code.toDouble(),
        contentType = contentType
      )
    } finally {
      conn.disconnect()
    }
  }

  private fun isSafeRelativePath(path: String): Boolean {
    if (path.isEmpty() || path.startsWith("/")) return false
    if (path.contains("..")) return false
    return true
  }
}
