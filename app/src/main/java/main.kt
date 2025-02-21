package Cai_Ming_Yu.CacheCleaner

import android.content.Context
import com.highcapable.yukihookapi.YukiHookAPI.encase
import com.highcapable.yukihookapi.annotation.xposed.InjectYukiHookWithXposed
import com.highcapable.yukihookapi.hook.factory.configs
import com.highcapable.yukihookapi.hook.xposed.proxy.IYukiHookXposedInit
import java.io.File

@InjectYukiHookWithXposed
object Main : IYukiHookXposedInit {
    override fun onInit() = configs { isDebug = BuildConfig.DEBUG }
    override fun onHook() {
        encase {
            loadApp {
                onAppLifecycle {
                    onCreate {
                        cacheCleaner(this)
                    }
                }
            }
        }
    }

    private fun cacheCleaner(context: Context) {
        delCaches(context.externalCacheDir)
        delCaches(context.cacheDir)
        delCaches(context.codeCacheDir)
    }

    private fun delCaches(cacheDir: File?) {
        if (cacheDir != null && cacheDir.exists() && cacheDir.isDirectory) {
            cacheDir.deleteRecursively()
            cacheDir.mkdirs()
        }
    }
}