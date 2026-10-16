plugins { id("com.android.application") }
android {
    namespace = "com.scrcpyandroid.rkplaybackfix"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.scrcpyandroid.rkplaybackfix"
        minSdk = 26
        targetSdk = 35
        versionCode = 9
        versionName = "0.3.5"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures { buildConfig = true }
    sourceSets.getByName("main").assets.srcDir(layout.buildDirectory.dir("generated/openSourceAssets"))
}

// Documentation only: no runtime hook or player code is changed by this task.
val stageOpenSourceNotices by tasks.registering(Sync::class) {
    from(rootProject.file("../")) {
        include("LICENSE", "NOTICE", "OPEN_SOURCE.md", "THIRD_PARTY_NOTICES.md",
                "SOURCE_CODE.md", "SOURCE_CODE.zh-CN.md")
    }
    from(rootProject.file("../lsposed/OPEN_SOURCE.json"))
    into(layout.buildDirectory.dir("generated/openSourceAssets/open-source"))
}
tasks.named("preBuild") { dependsOn(stageOpenSourceNotices) }
// The framework supplies this API at runtime; do not package it in the APK.
dependencies {
    compileOnly("de.robv.android.xposed:api:82")
    testImplementation("junit:junit:4.13.2")
}
