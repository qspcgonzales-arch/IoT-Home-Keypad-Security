plugins {
    id("com.android.application")
    id("com.google.gms.google-services")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

android {
    namespace = "com.example.smart_home_security"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "IoT_SmartHome.Security"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    flavorDimensions += "firebaseEnvironment"
    productFlavors {
        create("production") {
            dimension = "firebaseEnvironment"
        }
        create("staging") {
            dimension = "firebaseEnvironment"
        }
    }

    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

val stagingProjectId = "iothomekeypadsecurity-staging"
val stagingProjectNumber = "637464771451"
val stagingApplicationId = "IoT_SmartHome.Security"
val stagingDatabaseUrl =
    "https://iothomekeypadsecurity-staging-default-rtdb.asia-southeast1.firebasedatabase.app"
val stagingGoogleServicesFile = file("src/staging/google-services.json")
val stagingDartOptionsFile = rootProject.file("../staging_firebase_options.json")
val validateStagingGoogleServices = tasks.register("validateStagingGoogleServices") {
    doLast {
        if (!stagingGoogleServicesFile.isFile) {
            throw GradleException(
                "Staging APK blocked: add the staging project's Android " +
                    "google-services.json at android/app/src/staging/."
            )
        }
        if (!stagingDartOptionsFile.isFile) {
            throw GradleException(
                "Staging APK blocked: create staging_firebase_options.json from its example."
            )
        }
        val root = groovy.json.JsonSlurper().parse(stagingGoogleServicesFile)
            as? Map<*, *>
        val projectInfo = root?.get("project_info") as? Map<*, *>
        if (projectInfo?.get("project_id") != stagingProjectId) {
            throw GradleException(
                "Staging google-services.json does not match the configured staging project ID."
            )
        }
        if (projectInfo["project_number"]?.toString() != stagingProjectNumber) {
            throw GradleException(
                "Staging google-services.json does not match the confirmed staging project number."
            )
        }
        val clients = root?.get("client") as? List<*>
        val stagingClient = clients?.firstOrNull { client ->
            val clientInfo = (client as? Map<*, *>)?.get("client_info") as? Map<*, *>
            val androidInfo =
                clientInfo?.get("android_client_info") as? Map<*, *>
            androidInfo?.get("package_name") == stagingApplicationId
        } as? Map<*, *>
        if (stagingClient == null) {
            throw GradleException(
                "Staging Firebase Android app must be registered for $stagingApplicationId."
            )
        }
        val clientInfo = stagingClient["client_info"] as? Map<*, *>
        val dartOptions = groovy.json.JsonSlurper().parse(stagingDartOptionsFile)
            as? Map<*, *>
        val nativeAppId = clientInfo?.get("mobilesdk_app_id")?.toString()
        val nativeApiKey = ((stagingClient["api_key"] as? List<*>)
            ?.firstOrNull() as? Map<*, *>)?.get("current_key")?.toString()
        val projectNumber = projectInfo?.get("project_number")?.toString()
        if (dartOptions == null ||
            dartOptions["FIREBASE_ENV"] != "staging" ||
            dartOptions["FIREBASE_PROJECT_ID"] != stagingProjectId ||
            dartOptions["FIREBASE_APP_ID"] != nativeAppId ||
            dartOptions["FIREBASE_MESSAGING_SENDER_ID"] != projectNumber ||
            dartOptions["FIREBASE_API_KEY"] != nativeApiKey
        ) {
            throw GradleException(
                "Staging Dart options must match the staging Android Firebase app."
            )
        }
        val nativeDatabaseUrl =
            projectInfo?.get("firebase_url")?.toString()?.trimEnd('/')
        val dartDatabaseUrl =
            dartOptions["FIREBASE_DATABASE_URL"]?.toString()?.trimEnd('/')
        if (nativeDatabaseUrl != stagingDatabaseUrl ||
            dartDatabaseUrl != stagingDatabaseUrl
        ) {
            throw GradleException(
                "Staging Dart and Android Realtime Database URLs must match the confirmed staging endpoint."
            )
        }
    }
}

tasks.matching {
    it.name.startsWith("processStaging") && it.name.endsWith("GoogleServices")
}.configureEach {
    dependsOn(validateStagingGoogleServices)
}

dependencies {
    implementation(platform("com.google.firebase:firebase-bom:35.0.2"))
    implementation("com.google.firebase:firebase-analytics")
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}
