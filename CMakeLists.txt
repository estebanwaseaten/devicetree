cmake_minimum_required(VERSION 3.0.0)

project(devicetree)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)



set(HEADER_LIST
    "include/devicetree.h"
    "src/uint_large.h"
)
set(SOURCE_LIST
	"src/devicetree.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_LIST} )

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
    PRIVATE src
)

target_link_libraries(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
