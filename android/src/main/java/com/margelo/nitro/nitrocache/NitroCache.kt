package com.margelo.nitro.nitrocache
  
import com.facebook.proguard.annotations.DoNotStrip

@DoNotStrip
class NitroCache : HybridNitroCacheSpec() {
  override fun multiply(a: Double, b: Double): Double {
    return a * b
  }
}
