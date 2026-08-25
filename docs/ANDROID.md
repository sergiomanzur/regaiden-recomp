# Android Porting & Build Guide for Resident Evil Gaiden

`gb-recompiled` includes built-in support for generating an Android NDK/Gradle project scaffold.

---

## 1. Generating the Android Project

To emit an Android project scaffold for Resident Evil Gaiden:

```bash
tools/gb-recompiled/build/bin/gbrecomp "rom/Resident Evil Gaiden (USA).gbc" -o android_project --android --android-package com.capcom.regaiden --android-app-name "Resident Evil Gaiden"
```

This generates:
```
android_project/
├── android/
│   ├── app/
│   │   ├── build.gradle
│   │   ├── jni/
│   │   │   └── src/
│   │   │       └── CMakeLists.txt
│   │   └── src/
│   │       └── main/
│   │           ├── AndroidManifest.xml
│   │           └── java/
```

---

## 2. Building the APK

### Prerequisites:
- Android SDK & NDK (r25+ recommended)
- Gradle 8+
- SDL2 source directory (e.g. `SDL2-2.30.x`)

### Build Command:
```bash
export SDL2_SOURCE_DIR=/path/to/SDL2
cd android_project/android
./gradlew :app:assembleDebug
```

The APK will be generated at `android/app/build/outputs/apk/debug/app-debug.apk`.

You can also open the `android/` directory directly in Android Studio.
