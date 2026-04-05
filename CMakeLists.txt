cmake_minimum_required(VERSION 3.16)

project(cache_aware_scheduling C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(CTest)

add_executable(cache_aware_scheduler
    src/cache.c
    src/scenario.c
    src/simulator.c
    src/main.c
)

target_include_directories(cache_aware_scheduler PRIVATE inc)

if(BUILD_TESTING)
    add_test(
        NAME simulator_self_test
        COMMAND cache_aware_scheduler --self-test
    )
endif()
