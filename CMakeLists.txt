cmake_minimum_required(VERSION 3.20)

project(CityofGold VERSION ${SKBUILD_PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

# Handle multi-configuration generators
if (DEFINED CMAKE_CONFIGURATION_TYPES)
    if (DEFINED ENV{CIBUILDWHEEL})
        set(CMAKE_CONFIGURATION_TYPES "Release")
    else()
        set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Profile;Optimized")
    endif()
endif()

if (CMAKE_GENERATOR MATCHES "Ninja Multi-Config")
    set(CMAKE_DEFAULT_BUILD_TYPE "Release")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--param=destructive-interference-size=64)
    endif()
endif()

# compiler flags
if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /MP /GL /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /MP /GL /DNDEBUG")
    set(CMAKE_CXX_FLAGS_PROFILE "/O2 /MP /GL /Zi")
    set(CMAKE_CXX_FLAGS_OPTIMIZED "/O2 /MP /GL /DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -pg -g")
    set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG")
    set(CMAKE_CXX_FLAGS_PROFILE "-O2 -g -DNDEBUG")
    set(CMAKE_CXX_FLAGS_OPTIMIZED "-Ofast -march=native -mtune=native -DNDEBUG")
endif()

option(ENABLE_TESTS "Enable building and running tests" ON)
option(ENABLE_PYTHON_BINDINGS "Build Python bindings" ON)
option(ENABLE_RENDERING "Build environment rendering capability using SDL3" ON)

if (DEFINED ENV{CIBUILDWHEEL} OR DEFINED ENV{READTHEDOCS} OR DEFINED ENV{ASV})
    set(ENABLE_TESTS OFF)
endif()

if (DEFINED ENV{READTHEDOCS})
    message("DISABLING RENDERING!!!!!!!!!!!!!!!!!1")
    set(ENABLE_RENDERING OFF)
endif()

configure_file(include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)
include_directories(include)
include_directories(${CMAKE_BINARY_DIR}/include)

# The main library
add_library(city_of_gold_env
    src/environment.cpp
    src/map.cpp
    src/player.cpp
    src/cards.cpp
    $<$<BOOL:${ENABLE_RENDERING}>:src/render.cpp>
)


target_compile_definitions(city_of_gold_env PRIVATE COG_LIBRARY_EXPORTS)
target_include_directories(city_of_gold_env PRIVATE include)

# Tests
if(ENABLE_TESTS)
    find_package(doctest REQUIRED)

    add_executable(test_environment src/tests/test_environment.cpp)
    target_compile_definitions(test_environment PRIVATE COG_LIBRARY_EXPORTS)
    target_link_libraries(test_environment PRIVATE city_of_gold_env doctest::doctest)

    include(CTest)
    enable_testing()
    add_test(NAME EnvironmentUnitTests COMMAND test_environment)
endif()

if(MSVC)
    target_compile_options(city_of_gold_env PRIVATE /W4)
    target_compile_options(city_of_gold_env PRIVATE
        "$<$<CONFIG:Debug>:
        /fsanitize=address
        >"
    )
    target_link_libraries(city_of_gold_env PRIVATE
        "$<$<CONFIG:Debug>:
        /fsanitize=address
        >"
    )
else()
    target_compile_options(city_of_gold_env PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wconversion
        -pedantic
    )
    target_compile_options(city_of_gold_env PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined;-fdiagnostics-color=always>"
    )
    target_link_libraries(city_of_gold_env PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined>"
    )
    target_link_options(city_of_gold_env PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address;-fsanitize=undefined>"
    )
endif()

if(ENABLE_RENDERING)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable SDL shared lib" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Enable SDL static build" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "Disable SDL shared lib" FORCE)
    add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

    target_link_libraries(city_of_gold_env PRIVATE SDL3-static)
    target_compile_definitions(city_of_gold_env PUBLIC COG_BUILD_WITH_RENDERING)

endif()

add_executable(profile_environment src/tests/profile.cpp)
target_link_libraries(profile_environment PRIVATE city_of_gold_env)
target_compile_definitions(profile_environment PRIVATE COG_LIBRARY_EXPORTS)


if(ENABLE_PYTHON_BINDINGS)

    set(fallback_name "city_of_gold")
    if(DEFINED SKBUILD_PROJECT_NAME)
        set(proj_name "${SKBUILD_PROJECT_NAME}")
    else()
        set(proj_name "${fallback_name}")
    endif()


    project(
      ${SKBUILD_PROJECT_NAME}
      VERSION ${SKBUILD_PROJECT_VERSION}
      LANGUAGES CXX
    )

    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 REQUIRED)

    pybind11_add_module(bindings_
        src/pybind/common.cpp
        src/pybind/single_env.cpp
        src/pybind/vectorized.cpp
    )
    target_compile_definitions(bindings_ PRIVATE
        COG_LIBRARY_EXPORTS
        BINDINGS_LIBRARY_EXPORTS
    )

    # gperftools deps only for Profile configuration
    target_link_libraries(bindings_ PRIVATE
        "$<$<CONFIG:Profile>:profiler;tcmalloc>"
        city_of_gold_env
        pybind11::module
    )
    set_target_properties(bindings_ PROPERTIES
        OUTPUT_NAME "_city_of_gold"
    )

    target_include_directories(bindings_ PRIVATE external)

    install(DIRECTORY module_tree/ DESTINATION ./${proj_name})
    install(DIRECTORY assets DESTINATION ./${proj_name}/)
    install(TARGETS bindings_ LIBRARY DESTINATION ./${proj_name}/)

endif()
