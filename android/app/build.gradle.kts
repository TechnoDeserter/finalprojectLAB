plugins {
    id("com.android.application")
    id("kotlin-android")
    id("dev.flutter.flutter-gradle-plugin")
}

android {
    namespace = "com.example.michaelesp32"
    compileSdk = 35
    ndkVersion = "27.0.12077973"

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_11.toString()
    }

    defaultConfig {
        applicationId = "com.example.michaelesp32"
        minSdk = 21 // Explicitly set to ensure compatibility
        targetSdk = 35 // Matches compileSdk for modern APIs
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    buildTypes {
        release {
            signingConfig = signingConfigs.getByName("debug") // Retained for testing
        }
    }

    configurations {
        all {
            resolutionStrategy {
                force("androidx.core:core:1.9.0") // Ensures compatibility with lStar
            }
        }
    }
}

flutter {
    source = "../.."
}