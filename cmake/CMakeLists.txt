cmake_minimum_required(VERSION 3.1.3)
project(fdbus C CXX)

option(fdbus_ENABLE_LOG "Enable log" ON)
option(fdbus_LOG_TO_STDOUT "Log to stdout" OFF)
option(fdbus_ENABLE_MESSAGE_METADATA "Enable metadata of message" ON)
option(fdbus_SOCKET_ENABLE_PEERCRED "Enable peercred of socket" ON)
option(fdbus_ALLOC_PORT_BY_SYSTEM "Allocate port number by system rather than by name server" OFF)
option(fdbus_SECURITY "Enable security of FDBus" OFF)
option(fdbus_ANDROID "build Android version" OFF)
option(fdbus_PIPE_AS_EVENTFD "Do not use event FD" OFF)
option(fdbus_BUILD_JNI "Build jni" OFF)
option(fdbus_LINK_SOCKET_LIB "specify -lsocket to link" OFF)
option(fdbus_LINK_PTHREAD_LIB "specify -lsocket to link" ON)
option(fdbus_BUILD_CLIB "build library for C" ON)
option(fdbus_FORCE_NO_RTTI "forced to build without rtti" ON)
option(fdbus_UDS_ABSTRACT "using abstract address for UDS" OFF)

if (MSVC)
    add_definitions("-D__WIN32__")
elseif(fdbus_ANDROID)
    add_definitions("-D__LINUX__")
else()
    if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "-std=gnu++11 -Wall ${CMAKE_CXX_FLAGS}")
    endif()
    #add_compile_options(-g -O0)
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

if (fdbus_SOCKET_ENABLE_PEERCRED)
    add_definitions("-DCONFIG_SOCKET_PEERCRED")
endif()

if (fdbus_ALLOC_PORT_BY_SYSTEM)
    add_definitions("-DCFG_ALLOC_PORT_BY_SYSTEM")
endif()

if (fdbus_SECURITY)
    add_definitions("-DCFG_FDBUS_SECURITY")
endif()
if (fdbus_PIPE_AS_EVENTFD)
    add_definitions("-DCFG_PIPE_AS_EVENTFD")
endif()
if (fdbus_FORCE_NO_RTTI)
    add_definitions("-DCONFIG_FDB_NO_RTTI")
endif()
if (fdbus_UDS_ABSTRACT)
    add_definitions("-DFDB_CONFIG_UDS_ABSTRACT")
endif()

if(DEFINED RULE_DIR)
    include(${RULE_DIR}/rule_base.cmake)
else()
    set(RULE_DIR ${CMAKE_SOURCE_DIR})
    include(rule_base.cmake)
endif()

get_filename_component(PACKAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR} PATH)

print_variable(PACKAGE_SOURCE_ROOT)

set(IDL_GEN_ROOT ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB FDBUS_SOURCES "${PACKAGE_SOURCE_ROOT}/fdbus/*.cpp")
if (MSVC)
    file(GLOB PLATFORM_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/win/*.cpp")
else()
    file(GLOB PLATFORM_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/linux/*.cpp")
endif()
if (MSVC)
    set(EVENTFD_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/CEventFd_pipe.cpp")
else()
    if (fdbus_PIPE_AS_EVENTFD)
        set(EVENTFD_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/CEventFd_pipe.cpp")
    else()
        set(EVENTFD_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/CEventFd_eventfd.cpp")
    endif()
endif()
file(GLOB_RECURSE SOCKET_SOURCES "${PACKAGE_SOURCE_ROOT}/platform/socket/*.cpp")
file(GLOB UTILS_SOURCES "${PACKAGE_SOURCE_ROOT}/utils/*.cpp")
file(GLOB WORKER_SOURCES "${PACKAGE_SOURCE_ROOT}/worker/*.cpp")
file(GLOB PUBLIC_HEADERS "${PACKAGE_SOURCE_ROOT}/common_base/*.h")

set(SECURITY_SOURCES
	${PACKAGE_SOURCE_ROOT}/security/cJSON/cJSON.c
	${PACKAGE_SOURCE_ROOT}/security/CApiSecurityConfig.cpp
	${PACKAGE_SOURCE_ROOT}/security/CFdbToken.cpp
	${PACKAGE_SOURCE_ROOT}/security/CFdbusSecurityConfig.cpp
)

set(OTHER_SOURCES
    ${PACKAGE_SOURCE_ROOT}/server/CBaseNameProxy.cpp
    ${PACKAGE_SOURCE_ROOT}/server/CIntraNameProxy.cpp
)

include_directories(
    ${PACKAGE_SOURCE_ROOT}
    ${PACKAGE_SOURCE_ROOT}/public
    )

if (MSVC)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)

    link_libraries(ws2_32.lib)
    set(LIB_BUILD_TYPE "STATIC")
else()
    if (fdbus_LINK_PTHREAD_LIB)
        link_libraries(pthread)
    endif()
    if (fdbus_LINK_SOCKET_LIB)
        link_libraries(socket)
    endif()

    set(LIB_BUILD_TYPE "SHARED")
endif()

add_library(common_base ${LIB_BUILD_TYPE}
                          ${FDBUS_SOURCES}
                          ${PLATFORM_SOURCES}
                          ${EVENTFD_SOURCES}
                          ${SOCKET_SOURCES}
                          ${UTILS_SOURCES}
                          ${WORKER_SOURCES}
                          ${OTHER_SOURCES}
                          ${SECURITY_SOURCES}
)
install(TARGETS common_base DESTINATION usr/lib)
install(DIRECTORY ${PACKAGE_SOURCE_ROOT}/public/common_base/ DESTINATION usr/include/common_base)

include(service.cmake)

if (fdbus_BUILD_JNI)
    include(jni.cmake)
endif()

if (fdbus_BUILD_CLIB)
    include(clib.cmake)
endif()

#set( CMAKE_VERBOSE_MAKEFILE on )

print_variable(fdbus_ENABLE_LOG)
print_variable(fdbus_LOG_TO_STDOUT)
print_variable(fdbus_ENABLE_MESSAGE_METADATA)
print_variable(fdbus_SOCKET_ENABLE_PEERCRED)
print_variable(fdbus_ALLOC_PORT_BY_SYSTEM)
print_variable(fdbus_SECURITY)
print_variable(fdbus_ANDROID)
print_variable(fdbus_PIPE_AS_EVENTFD)
print_variable(fdbus_BUILD_JNI)
print_variable(fdbus_LINK_SOCKET_LIB)
print_variable(fdbus_LINK_PTHREAD_LIB)
print_variable(fdbus_BUILD_CLIB)
