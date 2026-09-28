# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)



# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("tictactoe C")

add_executable(tictactoe WIN32 "src/main.c")

target_include_directories(tictactoe PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(tictactoe "${CMAKE_SOURCE_DIR}/lib/x64/SDL2main.lib")
target_link_libraries(tictactoe "${CMAKE_SOURCE_DIR}/lib/x64/SDL2.lib")


