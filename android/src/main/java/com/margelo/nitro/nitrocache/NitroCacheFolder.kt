package com.margelo.nitro.nitrocache

import android.app.Application
import com.facebook.proguard.annotations.DoNotStrip
import com.margelo.nitro.core.Promise
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL

@Suppress("PrivateApi")
private fun currentApplication(): Application {
  val activityThread = Class.forName("android.app.ActivityThread")
  val app = activityThread.getMethod("currentApplication").invoke(null) as? Application
  return app ?: error("NitroCacheFolder: no Application (ActivityThread.currentApplication is null)")
}

private object DownloadGate {
  private var maxParallel = 4
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
      running--
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

  override fun downloadFile(url: String, relativePath: String): Promise<DownloadResult> {
    val promise = Promise<DownloadResult>()
    if (!isSafeRelativePath(relativePath)) {
      promise.reject(RuntimeException("relativePath must be relative and cannot contain '..'"))
      return promise
    }
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
      try {
        DownloadGate.acquire()
        val result = downloadBlocking(url, relativePath)
        promise.resolve(result)
      } catch (e: Throwable) {
        promise.reject(e)
      } finally {
        DownloadGate.release()
      }
    }.start()

    return promise
  }

  private fun downloadBlocking(url: String, relativePath: String): DownloadResult {
    val root = File(getCacheDirectory())
    val outFile = File(root, relativePath)
    outFile.parentFile?.mkdirs()

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
