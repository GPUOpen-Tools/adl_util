## Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.25)

project(ADL_UTIL LANGUAGES CXX)

add_library(adl_util STATIC)
add_library(AMD::adl_util ALIAS adl_util)
target_sources(adl_util
    PRIVATE
        ADLUtil.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE "HEADERS"
        BASE_DIRS .
        FILES
            "ADLUtil.h"
)

target_compile_features(adl_util PRIVATE cxx_std_17)

# adl_util depends on adl and tsingleton as public dependencies exposed through ADLUtil.h
target_link_libraries(adl_util
    PUBLIC
        AMD::adl
        AMD::tsingleton
)
