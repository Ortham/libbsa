# Settings passed on the command line:
#
# PROJECT_LIBS_DIR = the directory which all external libraries may be referenced from.
# PROJECT_ARCH = the build architecture
# PROJECT_LINK = whether to build a static or dynamic library.

##############################
# General Settings
##############################

cmake_minimum_required (VERSION 2.8.9)
project (libbsa)

set (PROJECT_SRC "${CMAKE_SOURCE_DIR}/src/genericbsa.cpp" "${CMAKE_SOURCE_DIR}/src/helpers.cpp" "${CMAKE_SOURCE_DIR}/src/libbsa.cpp" "${CMAKE_SOURCE_DIR}/src/tes3bsa.cpp" "${CMAKE_SOURCE_DIR}/src/tes4bsa.cpp")

# Include source and library directories.
include_directories ("${PROJECT_LIBS_DIR}/boost" "${PROJECT_LIBS_DIR}/utf8" "${PROJECT_LIBS_DIR}/zlib" "${CMAKE_SOURCE_DIR}/src")

link_directories ("${PROJECT_LIBS_DIR}/zlib" )

##############################
# Platform-Specific Settings
##############################

# Settings when compiling for Windows.
IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    IF (${PROJECT_LINK} MATCHES "STATIC")
        add_definitions (-DLIBBSA_STATIC)
    ELSE ()
        add_definitions (-DLIBBSA_EXPORT)
    ENDIF ()
ENDIF ()

# Settings when compiling on Windows.
IF (CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    set (PROJECT_LIBS libboost_locale-vc110-mt-1_52 libboost_filesystem-vc110-mt-1_52 libboost_system-vc110-mt-1_52 libboost_regex-vc110-mt-1_52 zlibstatic)
    set (CMAKE_CXX_FLAGS "/EHsc")
ENDIF ()

# Settings when not cross-compiling.
IF (CMAKE_SYSTEM_NAME MATCHES CMAKE_HOST_SYSTEM_NAME)
    link_directories ("${PROJECT_LIBS_DIR}/boost/stage-${PROJECT_ARCH}/lib")
ENDIF ()

# Settings when compiling and cross-compiling on Linux.
IF (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    set (CMAKE_C_FLAGS  "-m${PROJECT_ARCH}")
    set (CMAKE_CXX_FLAGS "-m${PROJECT_ARCH}")
    set (CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
    set (CMAKE_SHARED_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
    set (CMAKE_MODULE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")

    IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
        link_directories ("${PROJECT_LIBS_DIR}/boost/stage-mingw-${PROJECT_ARCH}/lib")
    ENDIF ()

    set (PROJECT_LIBS boost_locale boost_filesystem boost_regex boost_system zlibstatic)
ENDIF ()

##############################
# Actual Building
##############################

# Build libbsa library.
add_library           (bsa ${PROJECT_LINK} ${PROJECT_SRC})
target_link_libraries (bsa ${PROJECT_LIBS})

# Build libbsa tester.
add_executable        (libbsa-tester "${CMAKE_SOURCE_DIR}/src/tester.cpp")
target_link_libraries (libbsa-tester bsa ${PROJECT_LIBS})
