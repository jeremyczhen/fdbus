cmake_minimum_required(VERSION 3.1.3)
project(fdbus C CXX)

option(fdbus_BUILD_TESTS "Build tests" ON)
option(fdbus_ENABLE_LOG "Enable log" ON)
option(fdbus_LOG_TO_STDOUT "Log to stdout" OFF)
option(fdbus_ENABLE_MESSAGE_METADATA "Enable metadata of message" ON)
option(fdbus_SOCKET_BLOCKING_CONNECT "Blocking until socket is connected" OFF)
option(fdbus_SOCKET_ENABLE_PEERCRED "Enable peercred of socket" ON)
option(fdbus_ALLOC_PORT_BY_SYSTEM "Allocate port number by system rather than by name server" OFF)

if (MSVC)
    add_definitions("-D__WIN32__")
else()
    add_compile_options(-std=gnu++11 -Wall)
    add_definitions("-D__LINUX__")
endif()

if (fdbus_ENABLE_LOG)
    add_definitions("-DCONFIG_DEBUG_LOG")
endif()

if (fdbus_LOG_TO_STDOUT)
    add_definitions("-DCONFIG_LOG_TO_STDOUT")
endif()

if (fdbus_ENABLE_MESSAGE_METADATA)
    add_definitions("-DCONFIG_FDB_MESSAGE_METADATA")
endif()

if (fdbus_SOCKET_BLOCKING_CONNECT)
    add_definitions("-DCONFIG_SOCKET_BLOCKING_CONNECT")
endif()

if (fdbus_SOCKET_ENABLE_PEERCRED)
    add_definitions("-DCONFIG_SOCKET_PEERCRED")
endif()

if (fdbus_ALLOC_PORT_BY_SYSTEM)
    add_definitions("-DCFG_ALLOC_PORT_BY_SYSTEM")
endif()

set(RULE_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(PROJECT_ROOT ${RULE_DIR} PATH)
include(rule_base.cmake)

set(IDL_GEN_ROOT ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB FDBUS_SOURCES "${PROJECT_ROOT}/fdbus/*.cpp")
if (MSVC)
    file(GLOB PLATFORM_SOURCES "${PROJECT_ROOT}/platform/win/*.cpp")
else()
    file(GLOB PLATFORM_SOURCES "${PROJECT_ROOT}/platform/linux/*.cpp")
endif()
file(GLOB_RECURSE SOCKET_SOURCES "${PROJECT_ROOT}/platform/socket/*.cpp")
file(GLOB UTILS_SOURCES "${PROJECT_ROOT}/utils/*.cpp")
file(GLOB WORKER_SOURCES "${PROJECT_ROOT}/worker/*.cpp")
file(GLOB PUBLIC_HEADERS "${PROJECT_ROOT}/common_base/*.h")

set(OTHER_SOURCES
    ${PROJECT_ROOT}/server/CBaseNameProxy.cpp
    ${PROJECT_ROOT}/server/CIntraNameProxy.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.NameServer.pb.cc
    ${IDL_GEN_ROOT}/idl-gen/common.base.MessageHeader.pb.cc
)

include(idl-gen.cmake)

include_directories(
    ${PROJECT_ROOT}
    ${PROJECT_ROOT}/public
    "${IDL_GEN_ROOT}"
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

add_library(common_base ${LIB_BUILD_TYPE}
                          ${FDBUS_SOURCES}
                          ${PLATFORM_SOURCES}
                          ${SOCKET_SOURCES}
                          ${UTILS_SOURCES}
                          ${WORKER_SOURCES}
                          ${OTHER_SOURCES}
)
install(TARGETS common_base DESTINATION usr/lib)
install(DIRECTORY ${PROJECT_ROOT}/public/common_base/ DESTINATION usr/include/common_base)

link_libraries(common_base)

include(service.cmake)
if (fdbus_BUILD_TESTS)
    include(example.cmake)
endif()
