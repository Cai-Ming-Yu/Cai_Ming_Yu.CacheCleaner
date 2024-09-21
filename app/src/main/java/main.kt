package Cai_Ming_Yu.CacheCleaner

import android.content.Context
import com.highcapable.yukihookapi.YukiHookAPI.encase
import com.highcapable.yukihookapi.annotation.xposed.InjectYukiHookWithXposed
import com.highcapable.yukihookapi.hook.factory.configs
import com.highcapable.yukihookapi.hook.xposed.proxy.IYukiHookXposedInit
import de.robv.android.xposed.XposedHelpers
import java.io.File

class Runner {
    companion object {
        fun doHook() {
            encase {
                loadApp {
                    val context = XposedHelpers.callStaticMethod(
                        XposedHelpers.findClass("android.app.ActivityThread", null), "currentApplication"
                    ) as? Context
                    context?.let { nonNullContext ->
                        cacheCleaner(nonNullContext)
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
}

@InjectYukiHookWithXposed
object Main : IYukiHookXposedInit {
    override fun onInit() = configs { isDebug = BuildConfig.DEBUG }
    override fun onHook() {
        Runner.doHook()
    }
}