plugins {
    id("com.android.application")
}

val sdkRoot = System.getenv("ANDROID_HOME")
    ?: System.getenv("ANDROID_SDK_ROOT")
    ?: "${System.getProperty("user.home")}/Android/Sdk"
val ndkVersionPinned = "28.2.13676358"

// Single source of truth for the release version: the VERSION file at the repo root
// ("MAJOR.MINOR.PATCH"). versionCode = MAJOR*10000 + MINOR*100 + PATCH, so 0.1.0 -> 100.
val nextQuakeVersion = file("${rootDir}/VERSION").readText().trim()
val nextQuakeVersionCode = nextQuakeVersion.split(".").let { parts ->
    parts[0].toInt() * 10000 + parts[1].toInt() * 100 + parts[2].takeWhile { it.isDigit() }.toInt()
}
val ndkRoot = System.getenv("ANDROID_NDK_HOME")
    ?: "$sdkRoot/ndk/$ndkVersionPinned"
val releaseStoreFile = providers.gradleProperty("NEXTQUAKE_STORE_FILE").orNull
val releaseStorePassword = providers.gradleProperty("NEXTQUAKE_STORE_PASSWORD").orNull
val releaseKeyAlias = providers.gradleProperty("NEXTQUAKE_KEY_ALIAS").orNull
val releaseKeyPassword = providers.gradleProperty("NEXTQUAKE_KEY_PASSWORD").orNull
val hasReleaseSigning = listOf(releaseStoreFile, releaseStorePassword, releaseKeyAlias, releaseKeyPassword)
    .all { !it.isNullOrBlank() }

android {
    namespace = "org.ezquake.android"
    compileSdk = 36
    ndkVersion = ndkVersionPinned

    defaultConfig {
        applicationId = "org.nextquake.android"
        minSdk = 29
        targetSdk = 36
        versionCode = nextQuakeVersionCode
        versionName = nextQuakeVersion

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_TOOLCHAIN_FILE=${rootDir}/vcpkg/scripts/buildsystems/vcpkg.cmake",
                    "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ndkRoot/build/cmake/android.toolchain.cmake",
                    "-DVCPKG_TARGET_TRIPLET=arm64-android",
                    "-DUSE_SYSTEM_LIBS=OFF",
                    "-DANDROID_ABI=arm64-v8a",
                    "-DANDROID_PLATFORM=android-29",
                    "-DRENDERER_CLASSIC_OPENGL=OFF",
                    "-DRENDERER_MODERN_OPENGL=OFF",
                    "-DRENDERER_VULKAN=ON",
                    // check_ipo_supported() fails for this NDK/Clang toolchain under
                    // AGP's externalNativeBuild (CMakeLists.txt's ENABLE_LTO default is
                    // ON and turns that failure into a hard configure-time
                    // FATAL_ERROR): confirmed by removing this line and rebuilding --
                    // "Link Time Optimization requested, but not available." before a
                    // single .c file compiles. Needs a real toolchain/CMake-version fix
                    // to use IPO here, not a one-line flag flip; leaving LTO off for
                    // Android until that's investigated separately.
                    "-DENABLE_LTO=OFF",
                    // AGP's externalNativeBuild only auto-maps CMAKE_BUILD_TYPE for
                    // build types it considers "debuggable" vs not; without this, our
                    // usual `assembleDebug` loop was silently compiling every .c/.cpp
                    // file with no -O flag at all (confirmed via compile_commands.json:
                    // no -O0/-O2/-O3, no -DNDEBUG) -- i.e. every perf number measured
                    // on-device this session was against an unoptimized build. Forcing
                    // RelWithDebInfo here matches the desktop Windows build's own
                    // convention (-O2 -g -DNDEBUG) while keeping debug symbols and the
                    // debuggable APK/manifest from the Gradle "debug" variant intact.
                    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
                )
            }
        }
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = file(releaseStoreFile!!)
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        release {
            isDebuggable = false
            externalNativeBuild {
                cmake {
                    arguments += "-DCMAKE_BUILD_TYPE=Release"
                }
            }
            signingConfig = if (hasReleaseSigning) signingConfigs.getByName("release") else null
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
