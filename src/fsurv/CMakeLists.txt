# ----------------------------------------------------------------------------
# Copyright (C) 2014 Verizon.  All Rights Reserved.
# All Rights Reserved
#
#   Author: Reed P Morrison
#   Date:   02/07/2014
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
# ------------------------------------------------------------------------------

# Project setup
project(hurl)
cmake_minimum_required(VERSION 2.6.0)

# header files to install
LIST(APPEND INST_HDRS ${HDRS})

# include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
#${CMAKE_CURRENT_BINARY_DIR}
    ../../include
)

#add_definitions(-std=c++0x)

# Source files
SET(SRCS
  fsurv.cc
)

# Headers
SET(HDRS
)

# build and install hl
add_executable(fsurv_cmd ${HDRS} ${SRCS})
add_dependencies(fsurv_cmd hlxcore)
SET_TARGET_PROPERTIES(fsurv_cmd PROPERTIES OUTPUT_NAME "fsurv")
target_link_libraries(fsurv_cmd ${CMAKE_BINARY_DIR}/src/core/libhlxcore.a ${LIBRARIES})

install(TARGETS fsurv_cmd
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
        COMPONENT Executables
        )
