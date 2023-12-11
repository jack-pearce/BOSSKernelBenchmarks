cmake_minimum_required(VERSION 3.10)
project(BOSSBenchmarks)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
endif()

# Configure CCache if available
find_program(CCACHE_EXECUTABLE ccache)
mark_as_advanced(CCACHE_EXECUTABLE)
if(CCACHE_EXECUTABLE)
    foreach(LANG C CXX)
        if(NOT DEFINED CMAKE_${LANG}_COMPILER_LAUNCHER AND NOT CMAKE_${LANG}_COMPILER MATCHES ".*/ccache")
            message(STATUS "Enabling ccache for ${LANG}")
            set(CMAKE_${LANG}_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "")
        endif()
    endforeach()
endif()

############################## Custom build modes ###############################

# Default flags for all build types
set(COMMON_FLAGS "-Wall -Wextra -Wpedantic")

# Debug build type
set(CMAKE_CXX_FLAGS_DEBUG "${COMMON_FLAGS} -Og -g"
        CACHE STRING
        "Flags used by the C++ compiler during Debug builds."
        FORCE)
set(CMAKE_C_FLAGS_DEBUG "${COMMON_FLAGS} -Og -g"
        CACHE STRING
        "Flags used by the C compiler during Debug builds."
        FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG
        ${CMAKE_EXE_LINKER_FLAGS_DEBUG} CACHE STRING
        "Flags used for linking binaries during Debug builds."
        FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG
        ${CMAKE_SHARED_LINKER_FLAGS_DEBUG} CACHE STRING
        "Flags used by the shared libraries linker during Debug builds."
        FORCE)

# Release build type
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS} -O3"
        CACHE STRING
        "Flags used by the C++ compiler during Release builds."
        FORCE)
set(CMAKE_C_FLAGS_RELEASE "${COMMON_FLAGS} -O3"
        CACHE STRING
        "Flags used by the C compiler during Release builds."
        FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE
        ${CMAKE_EXE_LINKER_FLAGS_RELEASE} CACHE STRING
        "Flags used for linking binaries during Release builds."
        FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE
        ${CMAKE_SHARED_LINKER_FLAGS_RELEASE} CACHE STRING
        "Flags used by the shared libraries linker during Release builds."
        FORCE)

# Sanitize build type
set(CMAKE_CXX_FLAGS_SANITIZE "${COMMON_FLAGS} -fno-omit-frame-pointer -fsanitize=address,signed-integer-overflow,null,alignment,bounds,function,return,vla-bound -O0 -g"
        CACHE STRING
        "Flags used by the C++ compiler during Sanitize builds."
        FORCE)
set(CMAKE_C_FLAGS_SANITIZE "${COMMON_FLAGS} -fno-omit-frame-pointer -fsanitize=address,signed-integer-overflow,null,alignment,bounds,function,return,vla-bound -O0 -g"
        CACHE STRING
        "Flags used by the C compiler during Sanitize builds."
        FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SANITIZE
        ${CMAKE_EXE_LINKER_FLAGS_DEBUG} CACHE STRING
        "Flags used for linking binaries during Sanitize builds."
        FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SANITIZE
        ${CMAKE_SHARED_LINKER_FLAGS_DEBUG} CACHE STRING
        "Flags used by the shared libraries linker during Sanitize builds."
        FORCE)
mark_as_advanced(
        CMAKE_CXX_FLAGS_DEBUG		    CMAKE_EXE_LINKER_FLAGS_DEBUG
        CMAKE_C_FLAGS_DEBUG		        CMAKE_SHARED_LINKER_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE		    CMAKE_EXE_LINKER_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELEASE		    CMAKE_SHARED_LINKER_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_SANITIZE		CMAKE_EXE_LINKER_FLAGS_SANITIZE
        CMAKE_C_FLAGS_SANITIZE		    CMAKE_SHARED_LINKER_FLAGS_SANITIZE
)

set(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel Sanitize."
        FORCE)

############################### External Projects ###############################

include(ExternalProject)

ExternalProject_Add(googlebenchmark
        URL "https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip"
        DOWNLOAD_DIR $ENV{HOME}/.cmake-downloads
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${BOSSBenchmarks_BINARY_DIR}/deps
            -DBENCHMARK_DOWNLOAD_DEPENDENCIES=ON
            -DBENCHMARK_ENABLE_GTEST_TESTS=OFF
            $<IF:$<CONFIG:>,,-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_FLAGS=${DCMAKE_CXX_FLAGS}
        BUILD_BYPRODUCTS
            ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}benchmark${CMAKE_STATIC_LIBRARY_SUFFIX}
            ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}benchmark_main${CMAKE_STATIC_LIBRARY_SUFFIX}
)

ExternalProject_Add(BOSS
        GIT_REPOSITORY ${BOSS_SOURCE_REPOSITORY}
        GIT_TAG ${BOSS_SOURCE_BRANCH}
        GIT_SHALLOW ON
        CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE} -DCMAKE_INSTALL_PREFIX=${BOSSBenchmarks_BINARY_DIR}/deps
            $<IF:$<CONFIG:>,,-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}> -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        BUILD_BYPRODUCTS
            ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}BOSS${CMAKE_SHARED_LIBRARY_SUFFIX}
)


####################### Other Options and Library Dependencies #######################

#set(VTune_DIR "" CACHE PATH "Where to look for VTune installation")
#find_path(ITT_NOTIFY_INCLUDE_DIR "ittnotify.h" HINTS ${VTune_DIR}/include)

#set(THREADS_PREFER_PTHREAD_FLAG ON)
#find_package(Threads REQUIRED)

################################ Benchmark executable ################################

set(BenchmarkFiles Benchmarks/BOSSBenchmarks.cpp)

add_executable(Benchmarks ${BenchmarkFiles})
add_dependencies(Benchmarks BOSS)

#target_link_libraries(Benchmarks Threads::Threads)
if(WIN32)
    target_link_libraries(Benchmarks shlwapi.lib)
endif(WIN32)
add_dependencies(Benchmarks googlebenchmark)
#if(ITT_NOTIFY_INCLUDE_DIR)
#    message(VERBOSE "found itt notify header in ${ITT_NOTIFY_INCLUDE_DIR}")
#    target_include_directories(Benchmarks SYSTEM PUBLIC ${ITT_NOTIFY_INCLUDE_DIR})
#    target_link_libraries(Benchmarks ${ITT_NOTIFY_INCLUDE_DIR}/../lib64/libittnotify${CMAKE_STATIC_LIBRARY_SUFFIX})
#    if(NOT WIN32)
#        target_link_libraries(Benchmarks dl)
#    endif(NOT WIN32)
#    target_compile_options(Benchmarks PUBLIC -DWITH_ITT_NOTIFY)
#endif(ITT_NOTIFY_INCLUDE_DIR)
target_link_libraries(Benchmarks ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}benchmark${CMAKE_STATIC_LIBRARY_SUFFIX})
target_link_libraries(Benchmarks ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}benchmark_main${CMAKE_STATIC_LIBRARY_SUFFIX})

target_link_libraries(Benchmarks ${BOSSBenchmarks_BINARY_DIR}/deps/lib/${CMAKE_SHARED_LIBRARY_PREFIX}BOSS${CMAKE_SHARED_LIBRARY_SUFFIX})

#target_link_libraries(Benchmarks Threads::Threads)
set_target_properties(Benchmarks PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(Benchmarks PUBLIC "/Zc:__cplusplus")
    target_compile_options(Benchmarks PUBLIC "/EHsc")
endif()
target_include_directories(Benchmarks SYSTEM PUBLIC ${BOSSBenchmarks_BINARY_DIR}/deps/include)
if(UNIX AND NOT APPLE)
    target_link_libraries(Benchmarks rt)
    target_link_libraries(Benchmarks dl)
endif()

#################################### Install ###################################

set_target_properties(Benchmarks PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(TARGETS Benchmarks RUNTIME DESTINATION bin)