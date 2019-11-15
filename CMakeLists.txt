project(rx-switcher)

cmake_minimum_required(VERSION 3.4.0)
#   https://cmake.org/cmake/help/v3.12/policy/CMP0074.html
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

# enforce C++11
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# help to find cuda on systems with a software module system
list(APPEND CMAKE_PREFIX_PATH "$ENV{CUDA_ROOT}")

# help to find AMD OCL SDK Light (replaced APP SDK)
list(APPEND CMAKE_PREFIX_PATH "$ENV{OCL_ROOT}")

# help to find AMD app SDK on systems with a software module system
list(APPEND CMAKE_PREFIX_PATH "$ENV{AMDAPPSDKROOT}")

# allow user to extent CMAKE_PREFIX_PATH via environment variable
list(APPEND CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")

################################################################################
# CMake user options
################################################################################

# gcc 5.1 is the first GNU version without CoW strings
# https://github.com/fireice-uk/xmr-stak-nvidia/pull/10#issuecomment-290821792
# If you remove this guard to compile with older gcc versions the miner will produce
# a high rate of wrong shares.
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.1)
        message(FATAL_ERROR "g++ version must be at least 5.1!")
    endif()
endif()

set(BUILD_TYPE "Release;Debug")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${BUILD_TYPE}")

set(RX_SWITCHER_COMPILE "native" CACHE STRING "select CPU compute architecture")
set_property(CACHE RX_SWITCHER_COMPILE PROPERTY STRINGS "native;generic;dev_release")
if(RX_SWITCHER_COMPILE STREQUAL "native")
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        set(CMAKE_CXX_FLAGS "-march=native -mtune=native ${CMAKE_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "-march=native -mtune=native ${CMAKE_C_FLAGS}")
    endif()
elseif(RX_SWITCHER_COMPILE STREQUAL "generic" OR RX_SWITCHER_COMPILE STREQUAL "dev_release")
    add_definitions("-DCONF_ENFORCE_OpenCL_1_2=1")
else()
    message(FATAL_ERROR "RX_SWITCHER_COMPILE is set to an unknown value '${RX_SWITCHER_COMPILE}'")
endif()

# option to add static libgcc and libstdc++
option(CMAKE_LINK_STATIC "link as much as possible libraries static" OFF)


################################################################################
# Find PThreads
################################################################################

if(NOT WIN32)
    SET(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif()

###############################################################################
# Find OpenSSL
###############################################################################

option(OpenSSL_ENABLE "Enable or disable the requirement of OpenSSL" ON)
if(OpenSSL_ENABLE)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        include_directories(${OPENSSL_INCLUDE_DIR})
        set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
    else()
        message(FATAL_ERROR "OpenSSL NOT found: use `-DOpenSSL_ENABLE=OFF` to build without SSL support")
    endif()
else()
    add_definitions("-DCONF_NO_TLS")
endif()

################################################################################
# Find hwloc
################################################################################

option(HWLOC_ENABLE "Enable or disable the requirement of hwloc" ON)
if(HWLOC_ENABLE)
    find_path(HWLOC_INCLUDE_DIR
        NAMES
            hwloc.h
        PATHS
            /opt/local
            /usr/local
            /usr
            ENV "PROGRAMFILES(X86)"
            ENV "HWLOC_ROOT"
        PATH_SUFFIXES
            include)

    find_library(HWLOC
        NAMES
            libhwloc.lib
            hwloc
        PATHS
            ENV "HWLOC_ROOT"
        PATH_SUFFIXES
            lib)

    if(HWLOC STREQUAL "HWLOC-NOTFOUND" OR ${HWLOC_INCLUDE_DIR} STREQUAL "HWLOC_INCLUDE_DIR-NOTFOUND")
        message(FATAL_ERROR "hwloc NOT found: use `-DHWLOC_ENABLE=OFF` to build without hwloc support")
    else()
        set(LIBS ${LIBS} ${HWLOC})
        include_directories(AFTER ${HWLOC_INCLUDE_DIR})
    endif()
else()
    add_definitions("-DCONF_NO_HWLOC")
endif()

################################################################################
# Windows Sockets
################################################################################

if(WIN32)
    set(LIBS ${LIBS} wsock32 ws2_32)
endif()

################################################################################
# Versioning
################################################################################

# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND git log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GIT_COMMIT_HASH STREQUAL "")
    add_definitions("-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}")
endif()
if(NOT GIT_BRANCH STREQUAL "")
    add_definitions("-DGIT_BRANCH=${GIT_BRANCH}")
endif()

# generate backend string
string(REPLACE ";" "-" STR_BACKEND_TYPES "${BACKEND_TYPES}")
add_definitions("-DBACKEND_TYPE=${STR_BACKEND_TYPES}")

################################################################################
# Compile & Link
################################################################################

include_directories(BEFORE .)

set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # remove warnings that f_open() is not save and f_open_s should be used
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    # disable min define to allow usage of std::min
    add_definitions(-DNOMINMAX)
endif()

# add -Wall for debug builds with gcc
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    endif()
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "-Wl,-z,noexecstack ${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "-Wl,-z,noexecstack ${CMAKE_C_FLAGS}")
endif()

if(RX_SWITCHER_COMPILE STREQUAL "dev_release")
    add_definitions(-DXMRSTAK_DEV_RELEASE)
endif()

# activate static libgcc and libstdc++ linking
if(CMAKE_LINK_STATIC)
    set(BUILD_SHARED_LIBRARIES OFF)
    set(DL_LIB ${CMAKE_DL_LIBS})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "GNU")
        set(LIBS "${DL_LIB} -static-libgcc -static-libstdc++ ${LIBS}")
    endif()
endif()


# compile generic backend files
file(GLOB BACKEND_CPP
    "rxswitcher/*.cpp"
    "rxswitcher/misc/*.cpp"
    "rxswitcher/net/*.cpp")

add_library(rx-switcher-backend
    STATIC
    ${BACKEND_CPP}
)
target_link_libraries(rx-switcher-backend ${CMAKE_DL_LIBS} ${LIBS})


# compile final binary
file(GLOB SRCFILES_CPP "rxswitcher/cli/*.cpp")
set_source_files_properties(${SRCFILES_CPP} PROPERTIES LANGUAGE CXX)

add_executable(rx-switcher ${SRCFILES_CPP})


set(EXECUTABLE_OUTPUT_PATH "bin" CACHE STRING "Path to place executables relative to ${CMAKE_INSTALL_PREFIX}")
set(LIBRARY_OUTPUT_PATH "bin" CACHE STRING "Path to place libraries relative to ${CMAKE_INSTALL_PREFIX}")

target_link_libraries(rx-switcher ${CMAKE_DL_LIBS})
target_link_libraries(rx-switcher rx-switcher-backend)
target_link_libraries(rx-switcher ${LIBS})

################################################################################
# Install
################################################################################

# do not install the binary if the project and install are equal
if( NOT CMAKE_INSTALL_PREFIX STREQUAL PROJECT_BINARY_DIR )
    install(TARGETS rx-switcher
            RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${EXECUTABLE_OUTPUT_PATH}")
    if(CUDA_FOUND)
        if(WIN32)
            install(TARGETS xmrstak_cuda_backend
                RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${LIBRARY_OUTPUT_PATH}")
        else()
            install(TARGETS xmrstak_cuda_backend
                LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/${LIBRARY_OUTPUT_PATH}")
        endif()
    endif()
    if(OpenCL_FOUND)
        if(WIN32)
            install(TARGETS xmrstak_opencl_backend
                RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/${LIBRARY_OUTPUT_PATH}")
        else()
            install(TARGETS xmrstak_opencl_backend
                LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/${LIBRARY_OUTPUT_PATH}")
        endif()
    endif()
else()
    # this rule is used if the install prefix is the build directory
    install(CODE "MESSAGE(\"rx-switcher installed to folder 'bin'\")")
endif()
