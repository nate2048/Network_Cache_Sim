cmake_minimum_required(VERSION 3.21)
project(Network_Cache)

set(CMAKE_CXX_STANDARD 14)

add_executable(Network_Cache main.cpp file.cpp file.h event.cpp event.h ll_cache.cpp ll_cache.h)

find_package(GSL REQUIRED)    # See below (2)
target_link_libraries(Network_Cache GSL::gsl GSL::gslcblas)