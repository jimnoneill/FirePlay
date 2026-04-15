cmake_minimum_required(VERSION 3.22.1)
project(fireplay LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# For now: stub JNI only. Full protocol wiring added once OpenSSL/libplist
# are vendored and UxPlay lib/ is compiled.
add_library(fireplay SHARED
    fireplay_jni.cpp
)

find_library(log-lib log)
target_link_libraries(fireplay ${log-lib})
