cmake_minimum_required(VERSION 3.1.3)
project(fdbus-example C CXX)

option(fdbus_LOG_TO_STDOUT "Log to stdout" OFF)
option(fdbus_ENABLE_LOG "Enable log" ON)

if (fdbus_LOG_TO_STDOUT)
    add_definitions("-DCONFIG_LOG_TO_STDOUT")
endif()
if (fdbus_ENABLE_LOG)
    add_definitions("-DCONFIG_DEBUG_LOG")
endif()

if (MSVC)
    add_definitions("-D__WIN32__")
elseif(fdbus_ANDROID)
    add_definitions("-D__LINUX__")
else()
    add_compile_options(-std=gnu++11 -Wall -g -O0)
    #add_compile_options(-g -O0)
    add_definitions("-D__LINUX__")
endif()

set(RULE_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(PROJECT_ROOT ${RULE_DIR} PATH)
get_filename_component(PROJECT_ROOT ${PROJECT_ROOT} PATH)
include(rule_base.cmake)

set(IDL_GEN_ROOT ${CMAKE_CURRENT_BINARY_DIR})

include(idl-gen.cmake)

include_directories(
    ${PROJECT_ROOT}/public
    ${IDL_GEN_ROOT}
    )

if (MSVC)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)

    link_libraries(libprotobufd ws2_32.lib)
    set(LIB_BUILD_TYPE "STATIC")
else()
    link_libraries(protobuf)
    set(LIB_BUILD_TYPE "SHARED")
endif()

link_libraries(common_base)
if (fdbus_LINK_SOCKET_LIB)
    link_libraries(socket)
endif()

include(example.cmake)

#set( CMAKE_VERBOSE_MAKEFILE on )

