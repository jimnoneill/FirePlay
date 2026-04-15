#!/bin/bash
set -euo pipefail

cd "$HOME/openssl-3.0.13"

export ANDROID_NDK_ROOT="$HOME/ndk"
TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
export PATH="$TOOLCHAIN/bin:/usr/bin:/bin:/usr/sbin:/sbin"

./Configure android-arm \
    -D__ANDROID_API__=28 \
    --prefix="$HOME/android-prefix" \
    no-shared no-tests no-engine no-dso

make -j"$(nproc)" build_libs
make install_dev
echo "OPENSSL_BUILD_DONE"
