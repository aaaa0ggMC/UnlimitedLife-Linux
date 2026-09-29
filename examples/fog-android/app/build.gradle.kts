plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

import java.util.Properties

android {
    namespace = "com.example.fogandroid"
    compileSdk = 36
    buildToolsVersion = "35.0.0"

    defaultConfig {
        applicationId = "com.example.fogandroid"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
    }

    // 原生库由自定义任务用 GCC-16 Android 交叉工具链直接构建（含 alib6 全量交叉编译），
    // 产物放 src/main/cpp/out/<abi>/，作为 jniLibs 源目录。
    sourceSets {
        getByName("main") {
            jniLibs.srcDir("src/main/cpp/out")
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

//// local.properties: pino.dir(GCC工具链) + alib.dir(aaaa0ggmcLib 源码) ////
fun localProp(key: String, fallback: String): String {
    val props = Properties().apply {
        val f = rootProject.file("local.properties")
        if (f.exists()) f.inputStream().use { load(it) }
    }
    return (props.getProperty(key) ?: fallback).trimEnd('/')
}

val buildNative by tasks.registering(Exec::class) {
    group = "build"
    description = "Cross-compile alib6 + app with GCC-16 Android toolchain (C++26 modules)"
    workingDir = file("src/main/cpp")
    commandLine("bash", "build-native.sh")
    environment("PINO_DIR", localProp("pino.dir", "/opt/android-gcc-cross"))
    environment("ALIB_DIR", localProp("alib.dir", "../../../aaaa0ggmcLib"))
    inputs.files(fileTree("src/main/cpp") { include("*.cppm", "*.cpp", "*.sh") })
    inputs.dir(localProp("alib.dir", "../../../aaaa0ggmcLib"))
    outputs.dir("src/main/cpp/out")
}

tasks.named("preBuild") {
    dependsOn(buildNative)
}
