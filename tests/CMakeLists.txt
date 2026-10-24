include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/063de7e9578f82b369302001269680b4b1553359.zip
  SYSTEM
)
FetchContent_MakeAvailable(googletest)

add_library(mops_support INTERFACE)
target_sources(mops_support INTERFACE
  FILE_SET HEADERS
  FILES
    support/printers.hpp
)

add_executable(test_mops
  decode/test_decode_golden.cpp
  decode/test_decode_negative.cpp
  decode/test_decode_cu.cpp
)
target_link_libraries(test_mops PRIVATE mops::mops mops_warnings mops_support GTest::gtest_main)
target_compile_definitions(test_mops PRIVATE
  MOPS_GOLDEN_FILE="${CMAKE_CURRENT_SOURCE_DIR}/data/decode_golden.txt"
)

include(GoogleTest)
gtest_discover_tests(test_mops)