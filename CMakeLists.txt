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
include(ExternalProject)

option(BUILD_SHARED_LIBS "Build a shared library" ON)
option(PROJECT_STATIC_RUNTIME "Build with static runtime libs (/MT)" ON)

ExternalProject_Add(zlib
                    PREFIX "external"
                    URL "http://zlib.net/zlib-1.2.8.tar.gz"
                    INSTALL_COMMAND "")
ExternalProject_Get_Property(zlib SOURCE_DIR BINARY_DIR)
set (ZLIB_INCLUDE_DIRS ${SOURCE_DIR} ${BINARY_DIR})
set (ZLIB_LIBRARIES optimized "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}zlibstatic${CMAKE_STATIC_LIBRARY_SUFFIX}"
debug "${BINARY_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}zlibstaticd${CMAKE_STATIC_LIBRARY_SUFFIX}")

set (Boost_USE_STATIC_LIBS ${PROJECT_STATIC_RUNTIME})
set (Boost_USE_MULTITHREADED ON)
set (Boost_USE_STATIC_RUNTIME ${PROJECT_STATIC_RUNTIME})

find_package(Boost REQUIRED COMPONENTS iostreams filesystem locale)

set (PROJECT_SRC "${CMAKE_SOURCE_DIR}/src/genericbsa.cpp"
                 "${CMAKE_SOURCE_DIR}/src/helpers.cpp"
                 "${CMAKE_SOURCE_DIR}/src/libbsa.cpp"
                 "${CMAKE_SOURCE_DIR}/src/tes3bsa.cpp"
                 "${CMAKE_SOURCE_DIR}/src/tes4bsa.cpp")

set (PROJECT_HEADERS "${CMAKE_SOURCE_DIR}/src/error.h"
                     "${CMAKE_SOURCE_DIR}/src/genericbsa.h"
                     "${CMAKE_SOURCE_DIR}/src/helpers.h"
                     "${CMAKE_SOURCE_DIR}/src/libbsa.h"
                     "${CMAKE_SOURCE_DIR}/src/tes3bsa.h"
                     "${CMAKE_SOURCE_DIR}/src/tes4bsa.h")

source_group("Header Files" FILES ${PROJECT_HEADERS})

set (TEST_SRC "${CMAKE_SOURCE_DIR}/src/tester.cpp")

include_directories("${CMAKE_SOURCE_DIR}/src"
                    ${Boost_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIRS})

##############################
# Platform-Specific Settings
##############################

# Settings when compiling and cross-compiling on Linux.
IF (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -std=c++11")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11")
    IF (PROJECT_STATIC_RUNTIME)
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
    ENDIF ()
ENDIF ()

# Settings when compiling with MSVC.
IF (MSVC)
    # Update compiler flags.
    IF (PROJECT_STATIC_RUNTIME)
        FOREACH(flag
            CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG_INIT
            CMAKE_CXX_FLAGS_RELEASE  CMAKE_CXX_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS_DEBUG  CMAKE_CXX_FLAGS_DEBUG_INIT)
            STRING(REPLACE "/MD"  "/MT" "${flag}" "${${flag}}")
            SET("${flag}" "${${flag}} /EHsc ")
        ENDFOREACH()
    ENDIF ()

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
    set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
ENDIF ()

##############################
# Targets
##############################

# Build libbsa library.
add_library           (bsa ${PROJECT_SRC} ${PROJECT_HEADERS})
add_dependencies      (bsa zlib)
target_link_libraries (bsa ${Boost_LIBRARIES} ${ZLIB_LIBRARIES})

# Build libbsa tester.
add_executable        (tester ${TEST_SRC})
target_link_libraries (tester bsa)


##############################
# Set Target-Specific Flags
##############################

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    IF (BUILD_SHARED_LIBS)
        set_target_properties (bsa PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LIBBSA_EXPORT")
    ELSE ()
        set_target_properties (bsa PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LIBBSA_STATIC")
        set_target_properties (tester PROPERTIES COMPILE_DEFINITIONS "${COMPILE_DEFINITIONS} LIBBSA_STATIC")
    ENDIF ()
ENDIF ()
