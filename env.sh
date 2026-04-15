#!/bin/bash
# Source this to set up FirePlay build environment in Git Bash.
export JAVA_HOME="/c/android/jdk/jdk-17.0.14+7"
export ANDROID_HOME="/c/android/sdk"
export ANDROID_NDK_HOME="/c/android/android-ndk-r27d"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"
echo "FirePlay env loaded. java=$(java -version 2>&1 | head -1)"
