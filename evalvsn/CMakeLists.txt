cmake_minimum_required (VERSION 3.5)
project (evalvsn)

# Set default build type to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
endif()

find_package (OpenCV REQUIRED)
add_executable (evalvsn ./src/evalvsn.cc ./src/video.cc ./src/frame.cc ./src/dct.cc ./src/quantification.cc ./src/zigzag.cc ./src/priority.cc ./src/rle.cc ./src/static.cc ./src/golomb.cc ./src/bin.cc ./src/hex.cc ./src/trace.cc ./src/packet.cc ./src/evaluate.cc ./src/pham.cc)
set_target_properties(evalvsn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
target_link_libraries (evalvsn ${OpenCV_LIBS})