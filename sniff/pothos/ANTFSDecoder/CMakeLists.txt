########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(ANTFS_Blocks CXX)

find_package(Pothos CONFIG REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++11 -Wno-c++11-extensions")
set(CMAKE_LD_FLAGS "${CMAKE_LD_FLAGS} -L/usr/local/lib")

########################################################################
## BTLE blocks
########################################################################
POTHOS_MODULE_UTIL(
	TARGET ANTFS_Blocks
    SOURCES
		ANTFSDecoder.cpp
    DESTINATION antfs
    ENABLE_DOCS
)
