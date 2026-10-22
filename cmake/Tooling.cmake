file(GLOB_RECURSE MOPS_SOURCES CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/include/*.hpp
  ${PROJECT_SOURCE_DIR}/tests/*.cpp
  ${PROJECT_SOURCE_DIR}/tests/*.hpp
)

find_program(CLANG_FORMAT NAMES clang-format)
if(CLANG_FORMAT)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i ${MOPS_SOURCES}
    COMMENT "Formatting sources"
    VERBATIM
  )
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT} --dry-run --Werror ${MOPS_SOURCES}
    COMMENT "Checking formatting"
    VERBATIM
  )
endif()

find_program(RUN_CLANG_TIDY NAMES run-clang-tidy)
if(RUN_CLANG_TIDY)
  add_custom_target(tidy
    COMMAND ${RUN_CLANG_TIDY} -p ${CMAKE_BINARY_DIR} -quiet
            "-header-filter=^${PROJECT_SOURCE_DIR}/include/"
            "^${PROJECT_SOURCE_DIR}/tests/"
    COMMENT "Running clang-tidy"
    VERBATIM
  )
endif()
