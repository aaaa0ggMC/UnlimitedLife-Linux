plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

import java.util.Properties

val fogBuildName = providers.gradleProperty("fogBuildName").orNull
    ?: throw GradleException("每次构建必须指定总结名，例如 -PfogBuildName=directport")
require(Regex("[a-z][a-z0-9_]*").matches(fogBuildName)) {
    "fogBuildName 必须以小写字母开头，且仅包含小写字母、数字或下划线"
}

val nativeAbis = providers.environmentVariable("FOG_ANDROID_ABIS")
    .orElse("arm64-v8a armeabi-v7a x86_64").get().trim().split(Regex("\\s+"))
val nativeApi = providers.environmentVariable("FOG_ANDROID_API").orElse("24").get().toInt()
require(nativeApi >= 24) { "FOG_ANDROID_API must be >= 24" }
require(nativeAbis.isNotEmpty() && nativeAbis.all { it in setOf("arm64-v8a", "armeabi-v7a", "x86_64") })

android {
    namespace = "com.example.fogandroid"
    compileSdk = 36
    buildToolsVersion = "35.0.0"

    defaultConfig {
        applicationId = "com.example.fogandroid.$fogBuildName"
        manifestPlaceholders["fogAppLabel"] = "$fogBuildName-fogandroid"
        minSdk = nativeApi
        ndk { abiFilters += nativeAbis }
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
    }

    // 原生库由自定义任务用 GCC-16 Android 交叉工具链直接构建（含 alib5 头文件 + 实现单元交叉编译），
    // 产物放 src/main/cpp/out/<abi>/，作为 jniLibs 源目录。
    sourceSets {
        getByName("main") {
            jniLibs.srcDir("src/main/cpp/out")
            assets.srcDir(layout.buildDirectory.dir("generated/fogAssets"))
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlin {
        compilerOptions {
            jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
        }
    }
}

dependencies {
    testImplementation("junit:junit:4.13.2")
}

//// local.properties: pino.dir(GCC工具链) + alib.dir(aaaa0ggmcLib 源码) ////
fun localProp(key: String, fallback: String): String {
    val props = Properties().apply {
        val f = rootProject.file("local.properties")
        if (f.exists()) f.inputStream().use { load(it) }
    }
    return (props.getProperty(key) ?: fallback).trimEnd('/')
}

// Resolve local.properties paths relative to the app project before changing
// the Exec working directory to src/main/cpp.
val alibDir = file(localProp("alib.dir", "../../../../aaaa0ggmcLib")).canonicalFile
val pinoDir = file(localProp("pino.dir", "/opt/android-gcc-cross")).canonicalFile
val engineDir = rootProject.file("../..").canonicalFile
val fogDir = file(localProp("fog.dir", "../../fog")).canonicalFile
val fogAssetsDir = file(localProp("fog.assets.dir", "../../../assets")).canonicalFile
val dependencyDirs = mapOf(
    "RAPIDJSON_DIR" to "rapidjson.dir", "TOMLPLUS_DIR" to "tomlplus.dir",
    "GLM_DIR" to "glm.dir", "VULKAN_DIR" to "vulkan.dir",
    "STB_DIR" to "stb.dir", "VMA_DIR" to "vma.dir", "IMGUI_DIR" to "imgui.dir"
).mapValues { (env, property) ->
    val fallback = if (env == "IMGUI_DIR") "../.deps/imgui-1.92.9-docking" else ""
    localProp(property, providers.environmentVariable(env).orNull ?: fallback).let {
        if (it.isEmpty()) "" else file(it).canonicalPath
    }
}

val prepareFogAssets by tasks.registering(Exec::class) {
    group = "build"
    description = "Compile fog shaders and package only scene runtime assets"
    workingDir = file("src/main/cpp")
    commandLine("bash", "prepare-assets.sh", layout.buildDirectory.dir("generated/fogAssets").get().asFile.absolutePath)
    environment("FOG_DIR", fogDir.absolutePath)
    environment("FOG_ASSET_DIR", fogAssetsDir.absolutePath)
    inputs.files(file("src/main/cpp/prepare-assets.sh"), file("src/main/cpp/config.android.json"))
    inputs.files(fileTree(fogDir) { include("shaders/**", "textures/**", "build_shaders.sh") })
    inputs.files(file("$fogAssetsDir/scene.json"), file("$fogAssetsDir/test_data/dragon.obj"),
        file("$fogAssetsDir/test_data/wqy-microhei.ttf"), file("$fogAssetsDir/test_data/imgs/ice.png"))
    outputs.dir(layout.buildDirectory.dir("generated/fogAssets"))
}

val buildNative by tasks.registering(Exec::class) {
    group = "build"
    description = "Cross-compile alib5 + AVE Android + shared fog with GCC-16"
    workingDir = file("src/main/cpp")
    commandLine("bash", "build-native.sh")
    environment("PINO_DIR", pinoDir.absolutePath)
    environment("ALIB_DIR", alibDir.absolutePath)
    environment("FOG_DIR", fogDir.absolutePath)
    dependencyDirs.forEach { (env, directory) ->
        if (directory.isNotEmpty()) {
            environment(env, directory)
            inputs.dir(directory)
        }
    }
    inputs.property("dependencyDirs", dependencyDirs)
    inputs.property("pinoDir", pinoDir.absolutePath)
    inputs.property("nativeAbis", nativeAbis)
    inputs.property("nativeApi", nativeApi)
    // App 侧源/脚本输入（去 modules：不再有 *.cppm）。
    inputs.files(fileTree("src/main/cpp") { include("*.cpp", "*.h", "*.sh") })
    // alib5 头与实现单元作为输入（对齐 build-native.sh 实际使用的 include/alib5 与 modules/alib5）。
    inputs.dir(file("$alibDir/include/alib5"))
    inputs.dir(file("$alibDir/modules/alib5"))
    inputs.dir(file("$engineDir/include/AVE"))
    inputs.dir(file("$engineDir/modules/AVE"))
    inputs.files(fileTree(fogDir) { include("*.cpp", "*.h") })
    outputs.dir("src/main/cpp/out")
    // Header depfiles in build-native.sh also track default system-header inputs.
    // Always run the cheap incremental driver, even when Gradle's inputs are unchanged.
    outputs.upToDateWhen { false }
}

tasks.named("preBuild") {
    dependsOn(buildNative)
    dependsOn(prepareFogAssets)
}
