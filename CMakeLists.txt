# python-libnest2d — Standalone CMake build (no Conan required)
# Downloads and builds all C++ dependencies via FetchContent.
cmake_minimum_required(VERSION 3.20)
project(python-libnest2d LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Fix for old CMakeLists.txt in dependencies
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
# Use modern FindPython instead of deprecated FindPythonInterp
set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)
# Build all deps as static libs
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

include(FetchContent)

# ── pybind11 ────────────────────────────────────────────────────────────────
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.13.6
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(pybind11)
endif()

# ── Clipper (polygon clipping — required by libnest2d) ──────────────────────
FetchContent_Declare(clipper
    GIT_REPOSITORY https://github.com/tamasmeszaros/libpolyclipping.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(clipper)

# ── NLopt (nonlinear optimization — required by libnest2d) ──────────────────
set(NLOPT_PYTHON   OFF CACHE BOOL "" FORCE)
set(NLOPT_OCTAVE   OFF CACHE BOOL "" FORCE)
set(NLOPT_MATLAB   OFF CACHE BOOL "" FORCE)
set(NLOPT_GUILE    OFF CACHE BOOL "" FORCE)
set(NLOPT_SWIG     OFF CACHE BOOL "" FORCE)
set(NLOPT_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlopt
    GIT_REPOSITORY https://github.com/stevengj/nlopt.git
    GIT_TAG        v2.9.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlopt)

# ── Boost (header-only geometry module) ─────────────────────────────────────
# Boost is header-only for our use case (boost::geometry).
# On CI: installed via system package manager (yum/apt).
# On Windows: install via scoop/vcpkg/chocolatey.
find_package(Boost QUIET)
if(NOT Boost_FOUND)
    # Fallback: check common scoop/vcpkg locations
    foreach(_hint "$ENV{USERPROFILE}/scoop/apps/boost/current"
                  "$ENV{BOOST_ROOT}"
                  "C:/local/boost_1_86_0"
                  "C:/boost")
        if(EXISTS "${_hint}/boost/geometry.hpp")
            set(BOOST_INCLUDEDIR "${_hint}" CACHE PATH "")
            find_package(Boost QUIET)
            if(Boost_FOUND)
                break()
            endif()
        endif()
    endforeach()
    if(NOT Boost_FOUND)
        message(FATAL_ERROR
            "Boost headers not found. Install Boost:\n"
            "  Windows: scoop install boost\n"
            "  Linux:   apt install libboost-dev\n"
            "  macOS:   brew install boost")
    endif()
endif()

# ── libnest2d (header-only nesting library) ─────────────────────────────────
FetchContent_Declare(libnest2d
    GIT_REPOSITORY https://github.com/tamasmeszaros/libnest2d.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)
FetchContent_GetProperties(libnest2d)
if(NOT libnest2d_POPULATED)
    FetchContent_Populate(libnest2d)
endif()

# ── Python extension module ─────────────────────────────────────────────────
pybind11_add_module(pynest2d bindings.cpp)
target_include_directories(pynest2d PRIVATE
    ${libnest2d_SOURCE_DIR}/include
    ${clipper_SOURCE_DIR}
)
target_link_libraries(pynest2d PRIVATE
    polyclipping
    nlopt
)
# libnest2d compile definitions
target_compile_definitions(pynest2d PRIVATE
    LIBNEST2D_GEOMETRIES_clipper
    LIBNEST2D_OPTIMIZER_nlopt
    LIBNEST2D_THREADING_std
)
# Boost headers
if(TARGET Boost::headers)
    target_link_libraries(pynest2d PRIVATE Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(pynest2d PRIVATE Boost::boost)
elseif(Boost_INCLUDE_DIRS)
    target_include_directories(pynest2d PRIVATE ${Boost_INCLUDE_DIRS})
endif()

install(TARGETS pynest2d DESTINATION .)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pynest2d.pyi DESTINATION .)
