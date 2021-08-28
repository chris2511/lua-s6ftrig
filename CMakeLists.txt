
cmake_minimum_required(VERSION 3.13.0)

project(s6ftrig)

include(FindLua)
# S6 init system
find_library(S6_libs libs6.a)
find_library(S6_skalibs skalibs/libskarnet.a)
find_path(S6_header s6/s6-supervise.h)

add_compile_options(-Wall -Wextra -pedantic -Werror -Waggregate-return -Wshadow -Wstrict-prototypes)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(${CMAKE_PROJECT_NAME} SHARED s6ftrig.c)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${S6_libs} ${S6_skalibs})
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${S6_header} ${LUA_INCLUDE_DIR})

if (NOT DEFINED LUA_LIBDIR)
  set(LUA_LIBDIR "/usr/local/lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
endif()

install(TARGETS ${CMAKE_PROJECT} DESTINATION ${LUA_LIBDIR})
