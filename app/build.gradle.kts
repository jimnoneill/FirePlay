plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "org.fireplay"
    compileSdk = 34
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "org.fireplay"
        minSdk = 28        // Fire OS 7 = Android 9
        targetSdk = 28     // targeting Fire OS 7 specifically for AFTGAZL
        versionCode = 5
        versionName = "0.1.4"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
                cFlags += listOf("-Wall", "-O2")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        prefab = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Sign release builds with the debug keystore so the APK is
            // installable via sideload. F-Droid rebuilds from source and
            // signs with its own key; Play Store is not a target.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    // targetSdk = 28 is intentional for Fire OS 7 (AFTGAZL). The Google
    // Play API-level lint check does not apply to this project.
    lint {
        disable += setOf("ExpiredTargetSdkVersion", "OldTargetApi")
        checkReleaseBuilds = false
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.leanback:leanback:1.0.0")  // Android TV UI
    implementation("org.jmdns:jmdns:3.5.9")              // Apple-compatible mDNS
    implementation("androidx.work:work-runtime-ktx:2.9.0") // Persistent boot-start
}
