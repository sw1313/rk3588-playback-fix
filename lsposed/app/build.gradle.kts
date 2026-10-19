plugins { id("com.android.application") }
android {
    namespace = "com.scrcpyandroid.rkplaybackfix"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.scrcpyandroid.rkplaybackfix"
        minSdk = 26
        targetSdk = 35
        versionCode = 18
        versionName = "0.3.7"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures { buildConfig = true }
    sourceSets.getByName("main").assets.srcDir(layout.buildDirectory.dir("generated/openSourceAssets"))
}

// Source-built native client payload is staged separately by the guarded builder.
val stageOpenSourceNotices by tasks.registering(Sync::class) {
    from(rootProject.file("../")) {
        include("LICENSE", "NOTICE", "OPEN_SOURCE.md", "THIRD_PARTY_NOTICES.md",
                "SOURCE_CODE.md", "SOURCE_CODE.zh-CN.md")
    }
    from(rootProject.file("../lsposed/OPEN_SOURCE.json"))
    into(layout.buildDirectory.dir("generated/openSourceAssets/open-source"))
    doLast {
        destinationDir.walkTopDown().filter { it.isFile }.forEach {
            it.writeText(it.readText(Charsets.UTF_8).replace("\r\n", "\n"), Charsets.UTF_8)
        }
    }
}
tasks.named("preBuild") { dependsOn(stageOpenSourceNotices) }
val p010Payload = providers.gradleProperty("p010PayloadDir")
val stageP010Payload by tasks.registering(Sync::class) {
    if (p010Payload.isPresent) {
        from(p010Payload.get())
        doFirst {
            require(file(p010Payload.get() + "/contract.json").isFile &&
                    file(p010Payload.get() + "/libavcodec.so").isFile &&
                    file(p010Payload.get() + "/dav1d-COPYING").isFile) { "Verified P010 payload, contract and notices required" }
        }
    } else {
        // Keep Sync nonempty so Gradle removes stale payload files instead of
        // skipping the task as NO-SOURCE when this opt-in is removed.
        from(rootProject.file("../NOTICE")) { rename { "payload-disabled" } }
    }
    // An absent opt-in must clear the previous build's native payload as well.
    into(layout.buildDirectory.dir("generated/openSourceAssets/p010"))
    includeEmptyDirs = false
}
tasks.named("preBuild") { dependsOn(stageP010Payload) }
// The framework supplies this API at runtime; do not package it in the APK.
dependencies {
    compileOnly("de.robv.android.xposed:api:82")
    testImplementation("junit:junit:4.13.2")
}
