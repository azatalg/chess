cmake_minimum_required(VERSION 3.16)
project(chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Compatibility for EABase" FORCE)
set(EASTL_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(EASTL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(EASTL
    URL https://github.com/electronicarts/EASTL/archive/refs/tags/3.21.23.tar.gz
    URL_HASH SHA256=2bcb48f88f7daf9f91c165aae751c10d11d6959b6e10f2dda8f1db893e684022
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(EASTL)

add_library(chess_engine STATIC
    game/board.cpp
    game/game.cpp
    game/magic_bitboards.cpp
    game/eastl_allocator.cpp)
target_include_directories(chess_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(chess_engine PUBLIC EASTL)

function(chess_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wno-c++20-compat>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endfunction()
chess_warnings(chess_engine)

add_library(chess_ai STATIC
    bot/bot/bot.cpp
    bot/flat_mc/flat_mc.cpp
    bot/search/search_bot.cpp
    bot/utils/utils.cpp)
target_link_libraries(chess_ai PUBLIC chess_engine)
target_include_directories(chess_ai PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
chess_warnings(chess_ai)

add_executable(chess main.cpp cli/terminal.cpp)
target_link_libraries(chess PRIVATE chess_engine chess_ai)
chess_warnings(chess)

if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(chess_server web/server.cpp)
    target_link_libraries(chess_server PRIVATE chess_engine chess_ai Threads::Threads)
    target_compile_definitions(chess_server PRIVATE
        CHESS_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web/static")
    chess_warnings(chess_server)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(chess_perft tests/perft.cpp)
    target_link_libraries(chess_perft PRIVATE chess_engine)
    chess_warnings(chess_perft)
    add_test(NAME chess.perft COMMAND chess_perft)
    add_executable(chess_search_test tests/search_test.cpp)
    target_link_libraries(chess_search_test PRIVATE chess_ai)
    chess_warnings(chess_search_test)
    add_test(NAME chess.search.mate_in_one COMMAND chess_search_test)
endif()

option(CHESS_BUILD_BENCHMARKS "Build chess engine benchmarks" ON)
if(CHESS_BUILD_BENCHMARKS)
    add_executable(chess_bench benchmarks/engine_bench.cpp)
    target_link_libraries(chess_bench PRIVATE chess_engine chess_ai)
    chess_warnings(chess_bench)

    add_custom_target(bench
        COMMAND chess_bench
        DEPENDS chess_bench
        USES_TERMINAL
        COMMENT "Running quick chess engine benchmarks")
endif()
