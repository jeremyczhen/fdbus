cmake_minimum_required(VERSION 3.1.3)
project(fdbus-example C CXX)

option(fdbus_LOG_TO_STDOUT "Log to stdout" OFF)
option(fdbus_ENABLE_LOG "Enable log" ON)
option(fdbus_NORMAL_TEST "build test for normal lib test" ON)
option(fdbus_CLIB_TEST "build test for clib" ON)

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
    if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-std=c++11 -Wall ${CMAKE_CXX_FLAGS}")
    endif()
    #add_compile_options(-g -O0)
    add_definitions("-D__LINUX__")
endif()

if(DEFINED RULE_DIR)
    include(${RULE_DIR}/rule_base.cmake)
else()
    set(RULE_DIR ${CMAKE_SOURCE_DIR})
    include(rule_base.cmake)
endif()

get_filename_component(PACKAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR} PATH)
get_filename_component(PACKAGE_SOURCE_ROOT ${PACKAGE_SOURCE_ROOT} PATH)

set(IDL_GEN_ROOT ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
    ${PACKAGE_SOURCE_ROOT}/public
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

if (fdbus_NORMAL_TEST)
    include(example.cmake)
    include(idl-gen.cmake)
endif()

if (fdbus_CLIB_TEST)
    include(clib-test.cmake)
endif()

#set( CMAKE_VERBOSE_MAKEFILE on )

print_variable(fdbus_ENABLE_LOG)
print_variable(fdbus_LOG_TO_STDOUT)
print_variable(fdbus_ANDROID)
print_variable(fdbus_CLIB_TEST)
