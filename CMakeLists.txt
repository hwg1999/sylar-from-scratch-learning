cmake_minimum_required(VERSION 3.22)
project(sylar-from-scratch-learning)

include (cmake/utils.cmake)

# 指定编译选项
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -std=c++17 -O0 -ggdb -Wall")

# -rdynamic: 将所有符号都加入到符号表中，便于使用dlopen或者backtrace追踪到符号
# -fPIC: 生成位置无关的代码，便于动态链接
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -fPIC")

# -Wno-unused-function: 不要警告未使用函数
# -Wno-builtin-macro-redefined: 不要警告内置宏重定义，用于重定义内置的__FILE__宏
# -Wno-deprecated: 不要警告过时的特性
# -Wno-deprecated-declarations: 不要警告使用带deprecated属性的变量，类型，函数
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function -Wno-builtin-macro-redefined -Wno-deprecated -Wno-deprecated-declarations")

include_directories(.)

option(BUILD_TEST "ON for complile test" ON)

find_package(Boost REQUIRED) 
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

set(LIB_SRC
    sylar/log.cpp
    sylar/util.cpp
    sylar/mutex.cpp
    sylar/env.cpp
    sylar/config.cpp
    sylar/thread.cpp
    sylar/fiber.cpp
    sylar/scheduler.cpp
    sylar/iomanager.cpp
    sylar/timer.cpp
    sylar/fd_manager.cpp
    sylar/hook.cpp
    )

add_library(sylar SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(sylar)

set(LIBS
    sylar
    pthread
    dl
    yaml-cpp
)

if(BUILD_TEST)
sylar_add_executable(test_log "tests/test_log.cpp" sylar "${LIBS}")
sylar_add_executable(test_util "tests/test_util.cpp" sylar "${LIBS}")
sylar_add_executable(test_env "tests/test_env.cpp" sylar "${LIBS}")
sylar_add_executable(test_config "tests/test_config.cpp" sylar "${LIBS}")
sylar_add_executable(test_thread "tests/test_thread.cpp" sylar "${LIBS}")
sylar_add_executable(test_fiber "tests/test_fiber.cpp" sylar "${LIBS}")
sylar_add_executable(test_fiber2 "tests/test_fiber2.cpp" sylar "${LIBS}")
sylar_add_executable(test_scheduler "tests/test_scheduler.cpp" sylar "${LIBS}")
sylar_add_executable(test_iomanager "tests/test_iomanager.cpp" sylar "${LIBS}")
sylar_add_executable(test_timer "tests/test_timer.cpp" sylar "${LIBS}")
sylar_add_executable(test_hook "tests/test_hook.cpp" sylar "${LIBS}")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)