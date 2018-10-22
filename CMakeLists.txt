cmake_minimum_required(VERSION 3.12)
project(DTest)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES DTest.c DTest.h)

#set(PYTHON_LIBRARY /home/daniel/anaconda3/lib/libpython3.so)
#set(PYTHON_INCLUDE_DIR /home/daniel/anaconda3/include/python3.6m)
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})
#include_directories(/home/daniel/anaconda3/include/python3.6m)

add_executable(DTest ${SOURCE_FILES})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker -export-dynamic")

target_link_libraries(DTest ${PYTHON_LIBRARIES})
target_link_libraries(DTest m)
target_link_libraries(DTest pthread)
target_link_libraries(DTest dl)
target_link_libraries(DTest util)
target_link_libraries(DTest xml2)
