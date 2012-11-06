cmake_minimum_required (VERSION 2.8)
project (libstrings)

# Set some vars.
set (libbsa_VERSION_MAJOR 1)
set (libbsa_VERSION_MINOR 0)
set (libbsa_VERSION_PATCH 0)

set (LIBBSA_SRC "${CMAKE_SOURCE_DIR}/src")
set (LIBBSA_LIBS boost_filesystem boost_regex boost_system zlibstatic)
IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set (LIBBSA_LIBS ${LIBBSA_LIBS} pthread)
ENDIF ()
set (LIBBSA_LIB_SRC "${LIBBSA_SRC}/exception.cpp" "${LIBBSA_SRC}/genericbsa.cpp" "${LIBBSA_SRC}/helpers.cpp" "${LIBBSA_SRC}/libbsa.cpp" "${LIBBSA_SRC}/tes3bsa.cpp" "${LIBBSA_SRC}/tes4bsa.cpp")

# Set compiler flags.
set (CMAKE_C_FLAGS  "-m${LIBBSA_ARCH}  -static-libgcc")
set (CMAKE_CXX_FLAGS "-m${LIBBSA_ARCH} -static-libstdc++ -static-libgcc")
set (CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
set (CMAKE_SHARED_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
set (CMAKE_MODULE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")

# Include source and library directories.
include_directories     ("${CMAKE_SOURCE_DIR}/externals/boost" "${CMAKE_SOURCE_DIR}/externals/utf8" "${CMAKE_SOURCE_DIR}/externals/zlib" ${LIBBSA_SRC})
# Depending on platform and architecture, the libs are stored in different places.
IF (CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    link_directories    ("${CMAKE_SOURCE_DIR}/externals/boost/stage-mingw-${LIBBSA_ARCH}/lib" "${CMAKE_SOURCE_DIR}/externals/zlib")
ELSE ()
    link_directories    ("${CMAKE_SOURCE_DIR}/externals/boost/stage-${LIBBSA_ARCH}/lib"  "${CMAKE_SOURCE_DIR}/externals/zlib")
ENDIF ()

# Build libstrings library.
add_library         (bsa ${LIBBSA_LINK} ${LIBBSA_LIB_SRC})
target_link_libraries   (bsa ${LIBBSA_LIBS})

# Build libstrings tester.
add_executable          (libbsa-tester ${LIBBSA_SRC}/tester.cpp)
target_link_libraries   (libbsa-tester bsa)
