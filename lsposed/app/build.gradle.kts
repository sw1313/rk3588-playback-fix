plugins { id("com.android.application") }
android {
    namespace = "com.scrcpyandroid.rkplaybackfix"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.scrcpyandroid.rkplaybackfix"
        minSdk = 26
        targetSdk = 35
        versionCode = 3
        versionName = "0.3.0"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
// The framework supplies this API at runtime; do not package it in the APK.
dependencies { compileOnly("de.robv.android.xposed:api:82") }
