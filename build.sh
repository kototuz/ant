#!/bin/sh

# From https://github.com/raysan5/raylib/wiki/Working-for-Android-(on-Linux)

# stop on error and display each command as it gets executed. Optional step but helpful in catching where errors happen if they do.
set -xe

# NOTE: If you excluded any ABIs in the previous steps, remove them from this list too
ABIS="arm64-v8a armeabi-v7a x86 x86_64"

NDK="/home/i0ne/Programming/3rd-party/android/ndk"
SDK="/home/i0ne/Programming/3rd-party/android/sdk"

BUILD_TOOLS=$SDK/build-tools/29.0.3
TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
NATIVE_APP_GLUE=$NDK/sources/android/native_app_glue

FLAGS="-ffunction-sections -funwind-tables -fstack-protector-strong -fPIC -Wall \
    -Wformat -Werror=format-security -no-canonical-prefixes \
    -DANDROID -DPLATFORM_ANDROID -D__ANDROID_API__=29"

INCLUDES="-I$NATIVE_APP_GLUE -I$TOOLCHAIN/sysroot/usr/include"

# ______________________________________________________________________________
#
#  Compile
# ______________________________________________________________________________
#
for ABI in $ABIS; do
    case "$ABI" in
        "armeabi-v7a")
            CCTYPE="armv7a-linux-androideabi"
            ARCH="arm"
            LIBPATH="arm-linux-androideabi"
            ABI_FLAGS="-std=c99 -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16"
            ;;

        "arm64-v8a")
            CCTYPE="aarch64-linux-android"
            ARCH="aarch64"
            LIBPATH="aarch64-linux-android"
            ABI_FLAGS="-std=c99 -target aarch64 -mfix-cortex-a53-835769"
            ;;

        "x86")
            CCTYPE="i686-linux-android"
            ARCH="i386"
            LIBPATH="i686-linux-android"
            ABI_FLAGS=""
            ;;

        "x86_64")
            CCTYPE="x86_64-linux-android"
            ARCH="x86_64"
            LIBPATH="x86_64-linux-android"
            ABI_FLAGS=""
            ;;
    esac
    CC="$TOOLCHAIN/bin/${CCTYPE}29-clang"

    # Compile native app glue
    # .c -> .o
    $CC -c $NATIVE_APP_GLUE/android_native_app_glue.c -o $NATIVE_APP_GLUE/native_app_glue.o \
        $INCLUDES -I$TOOLCHAIN/sysroot/usr/include/$CCTYPE $FLAGS $ABI_FLAGS

    mkdir -p build/lib/$ABI

    # .o -> .a
    $TOOLCHAIN/bin/llvm-ar rcs build/lib/$ABI/libnative_app_glue.a $NATIVE_APP_GLUE/native_app_glue.o

    # Compile utils.c
    $CC -c src/c/utils.c -o build/utils.o $INCLUDES -I$TOOLCHAIN/sysroot/usr/include/$CCTYPE $FLAGS $ABI_FLAGS

    # Link the project with toolchain specific linker to avoid relocations issue.
    $TOOLCHAIN/bin/ld.lld build/utils.o -o build/lib/$ABI/libmain.so -shared \
        --exclude-libs libatomic.a --build-id \
        -z noexecstack -z relro -z now \
        --warn-shared-textrel --fatal-warnings \
        -L$TOOLCHAIN/sysroot/usr/lib/$LIBPATH/29 \
        -L$TOOLCHAIN/lib/clang/18/lib/linux/$ARCH \
        -L. -Landroid/build/obj -Lbuild/lib/$ABI \
        -lnative_app_glue -llog -landroid -lEGL -lGLESv2 -lOpenSLES -latomic -lc -lm -ldl
done

# ______________________________________________________________________________
#
#  Build APK
# ______________________________________________________________________________

$BUILD_TOOLS/aapt package -f -m \
    -S res -J src/java -M AndroidManifest.xml \
    -I $SDK/platforms/android-29/android.jar

# Compile NativeLoader.java
mkdir -p build/obj
javac -verbose -source 1.8 -target 1.8 -d build/obj \
    -bootclasspath jre/lib/rt.jar \
    -classpath $SDK/platforms/android-29/android.jar:build/obj \
    -sourcepath src/java src/java/com/kototuz/ant/R.java \
    src/java/com/kototuz/ant/NativeLoader.java

mkdir -p build/dex
$BUILD_TOOLS/dx --verbose --dex --output=build/dex/classes.dex build/obj

# Add resources and assets to APK
$BUILD_TOOLS/aapt package -f \
    -M AndroidManifest.xml -S res \
    -I $SDK/platforms/android-29/android.jar -F build/ant.apk build/dex

# Add libraries to APK
cd build
for ABI in $ABIS; do
    $BUILD_TOOLS/aapt add ant.apk lib/$ABI/libmain.so
done
cd ..

# Zipalign APK and sign
# NOTE: If you changed the storepass and keypass in the setup process, change them here too
$BUILD_TOOLS/zipalign -f 4 build/ant.apk build/ant.final.apk
mv -f build/ant.final.apk build/ant.apk

# Install apksigner with `sudo apt install apksigner`
apksigner sign --ks build/kototuz.keystore --out build/my-app-release.apk --ks-pass pass:kototuz build/ant.apk
mv build/my-app-release.apk build/ant.apk

# Install to device or emulator
$SDK/platform-tools/adb install -r build/ant.apk
