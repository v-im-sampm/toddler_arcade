cmake_minimum_required(VERSION 3.16)
project(little_arcade_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/W4 /WX /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()

add_library(little_arcade_test_support INTERFACE)
target_include_directories(little_arcade_test_support INTERFACE ${CMAKE_CURRENT_LIST_DIR}/support)

set(LA_COMPONENTS ${CMAKE_CURRENT_LIST_DIR}/../components)
add_subdirectory(${LA_COMPONENTS}/core core)
add_subdirectory(${LA_COMPONENTS}/board board)
add_subdirectory(${LA_COMPONENTS}/speech speech)
target_link_libraries(little_arcade_test_support INTERFACE little_arcade_core)

function(la_host_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE little_arcade_test_support little_arcade_core
                        little_arcade_board_pins little_arcade_speech)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

la_host_test(core_contract_test core_contract_test.cpp)
la_host_test(scene_manager_test scene_manager_test.cpp)
la_host_test(voice_policy_test voice_policy_test.cpp)

# Each game owns components/games/<name>/ and registers its own library + tests.
file(GLOB LA_GAME_DIRS LIST_DIRECTORIES true ${LA_COMPONENTS}/games/*)
foreach(dir ${LA_GAME_DIRS})
  if(EXISTS ${dir}/CMakeLists.txt)
    get_filename_component(game ${dir} NAME)
    add_subdirectory(${dir} games/${game})
  endif()
endforeach()
