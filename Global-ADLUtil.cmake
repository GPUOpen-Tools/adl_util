## Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All rights reserved.
cmake_minimum_required(VERSION 3.10)

## Define ADL Util directory
set(ADL_UTIL_DIR ${COMMON_DIR}/Src/ADLUtil)

## Include directory
set(ADDITIONAL_INCLUDE_DIRECTORIES ${ADDITIONAL_INCLUDE_DIRECTORIES}
                                   ${ADL_UTIL_DIR})

## Declare ADL Util sources
set(ADL_UTIL_SRC
    ADLUtil.cpp)

set(ADL_UTIL_HEADERS
    ADLUtil.h)
