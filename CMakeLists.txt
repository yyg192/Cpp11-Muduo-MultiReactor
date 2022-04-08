cmake_minimum_required(VERSION 2.5)
project(yyg_muduo)

#yyg_muduo最终编译成so 动态库，设置动态库的输出路径，放在根目录的ilb文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)  #别写成LIBRAY_OUTPUT_DIRECTORY了

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

include_directories(${PROJECT_SOURCE_DIR}/include)

aux_source_directory(./src SRC_LIST)

add_library(yyg_muduo SHARED ${SRC_LIST})