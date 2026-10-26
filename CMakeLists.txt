cmake_minimum_required(VERSION 3.25)
project(mops_emulator LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  include(cmake/Tooling.cmake)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

option(MOPS_BUILD_TESTS "Build the mops tests" ${PROJECT_IS_TOP_LEVEL})
option(MOPS_WARNINGS_AS_ERRORS "Treat warnings as errors" ${PROJECT_IS_TOP_LEVEL})
option(MOPS_BUILD_DEMO "Build the mops demo" ${PROJECT_IS_TOP_LEVEL})

add_library(mops INTERFACE)
add_library(mops::mops ALIAS mops)

target_sources(mops INTERFACE
  FILE_SET HEADERS
  BASE_DIRS include
  FILES
    include/mops/mops.hpp
    include/mops/types.hpp
    include/mops/host_concepts.hpp
    include/mops/decode.hpp
    include/mops/detail/decode_cpy.hpp
    include/mops/detail/decode_set.hpp
    include/mops/detail/decode_fields.hpp
    include/mops/detail/constrained_unpredictable.hpp
    include/mops/execute.hpp
    include/mops/detail/execute_common.hpp
    include/mops/detail/execute_set.hpp
    include/mops/detail/syndrome.hpp
    include/mops/step.hpp
)
target_compile_features(mops INTERFACE cxx_std_20)

add_library(mops_warnings INTERFACE)
target_compile_options(mops_warnings INTERFACE -Wall -Wextra -Wpedantic)
if(MOPS_WARNINGS_AS_ERRORS)
  target_compile_options(mops_warnings INTERFACE -Werror)
endif()

if(MOPS_BUILD_DEMO)
  add_executable(demo examples/demo.cpp examples/movz.hpp)
  target_link_libraries(demo PRIVATE mops::mops mops_warnings)
endif()

if(MOPS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
