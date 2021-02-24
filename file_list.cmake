# Load strings submodule
execute_process(COMMAND git submodule update --init --rebase -- lib/strings)

include(lib/strings/file_list)
set(STRINGS_PUBLIC_HEADERS_DIR ${PUBLIC_HEADERS_DIR})
list(APPEND INCLUDE_DIRS ${STRINGS_PUBLIC_HEADERS_DIR})

# paths to various directories
get_filename_component(GENERATED_HEADERS_DIR ${CMAKE_BINARY_DIR}/generated-headers ABSOLUTE)
get_filename_component(PUBLIC_HEADERS_DIR    ${CMAKE_CURRENT_LIST_DIR}/include ABSOLUTE)
get_filename_component(PRIVATE_HEADERS_DIR   ${CMAKE_CURRENT_LIST_DIR}/private-headers ABSOLUTE)
get_filename_component(SRC_DIR               ${CMAKE_CURRENT_LIST_DIR}/src ABSOLUTE)
get_filename_component(TEST_DIR              ${CMAKE_CURRENT_LIST_DIR}/test ABSOLUTE)
get_filename_component(LIBRARY_DIR           ${CMAKE_CURRENT_LIST_DIR}/lib ABSOLUTE)
list(APPEND INCLUDE_DIRS ${PRIVATE_HEADERS_DIR} ${GENERATED_HEADERS_DIR})

unset(DEBUG_SCOPES CACHE)

# public headers
set(NODE_HEADERS
  ${PUBLIC_HEADERS_DIR}/node/base.hpp
  ${PUBLIC_HEADERS_DIR}/node/node.hpp
  ${PUBLIC_HEADERS_DIR}/node/wrapper.hpp
  ${PUBLIC_HEADERS_DIR}/node/error.hpp
  ${STRINGS_PUBLIC_HEADERS_DIR}/tstring.hpp
)
set(LINI_HEADERS
  ${PUBLIC_HEADERS_DIR}/languages.hpp
)

# source files
set(NODE_SOURCES
  ${SRC_DIR}/node/base.cpp
  ${SRC_DIR}/node/node.cpp
  ${SRC_DIR}/node/wrapper.cpp
  ${SRC_DIR}/node/string_interpolate.cpp
  ${TSTRING_SOURCES}
)
set(LINI_SOURCES
  ${SRC_DIR}/lang_ini.cpp
  ${SRC_DIR}/lang_yml.cpp
  ${SRC_DIR}/languages.cpp
)

set(INTERNAL_TESTS)
set(EXTERNAL_TESTS node languages)
set(COPIED_FILES
  key_file.txt
  assign_test.txt
  ini_test.txt
  ini_test_output.txt
  yml_test.txt
  yml_test_output.txt
  lemonbar_test.txt
  lemonbar_test_output.txt
)
