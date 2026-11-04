cmake_minimum_required(VERSION 3.16)
project(little_arcade_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(../components/core core)
add_subdirectory(../components/board board)

enable_testing()
add_executable(core_contract_test core_contract_test.cpp)
target_link_libraries(core_contract_test PRIVATE little_arcade_core little_arcade_board_pins)
if(MSVC)
  target_compile_options(core_contract_test PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(core_contract_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME core_contract_test COMMAND core_contract_test)
