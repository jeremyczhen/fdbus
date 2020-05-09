function(link_directories_prepend)
    get_property(tmp_link_dir_var DIRECTORY PROPERTY LINK_DIRECTORIES)
    set(tmp_link_dir_var ${ARGV} ${tmp_link_dir_var})
    set_property(DIRECTORY PROPERTY LINK_DIRECTORIES ${tmp_link_dir_var})
endfunction()
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE on)

if (DEFINED REL_FLAGS)
    set(CMAKE_C_FLAGS_RELEASE ${REL_FLAGS})
    set(CMAKE_CXX_FLAGS_RELEASE ${REL_FLAGS})
endif()

if (DEFINED DBG_FLAGS)
    set(CMAKE_C_FLAGS_DEBUG ${DBG_FLAGS})
    set(CMAKE_CXX_FLAGS_DEBUG ${DBG_FLAGS})
endif()

if (DEFINED MACRO_DEF)
    foreach(mac ${MACRO_DEF})
        add_definitions(-D${mac})
    endforeach()
endif()

if (DEFINED SYSTEM_ROOT)
    foreach (root ${SYSTEM_ROOT})
        file(TO_CMAKE_PATH ${root} root)
        # where is the target environment 
        set(CMAKE_FIND_ROOT_PATH ${root})

        include_directories(${root}/usr/include ${root}/include)
        link_directories(${root}/usr/lib/. ${root}/lib/.)
    endforeach()
else()
    set(SYSTEM_ROOT /.)
endif()

if (DEFINED INC_PATH)
    foreach(dir ${INC_PATH})
        include_directories(${dir})
    endforeach()
endif()

if (DEFINED LIB_PATH)
    foreach(dir ${LIB_PATH})
        file(TO_CMAKE_PATH ${dir}/. dir)
        link_directories(${dir})
    endforeach()
endif()

file(TO_CMAKE_PATH ${RULE_DIR} RULE_DIR)
# uninstall target
configure_file(
    "${RULE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

function(print_variable)
    message(STATUS "${ARGV0}=${${ARGV0}}")
endfunction()

function(print_env)
    message(STATUS "${ARGV0}=$ENV{${ARGV0}}")
endfunction()

function(target_create_executable)
    set(dummy_file ${RULE_DIR}/dummy.c)
    add_executable(${ARGV0} ${dummy_file})
endfunction()

function(target_create_library)
    set(dummy_file ${RULE_DIR}/dummy.c)
    add_library(${ARGV0} ${ARGV1} ${dummy_file})
endfunction()

function(target_source_files)
    set(srcs ${ARGV})
    list(REMOVE_AT srcs 0) 
    target_sources(${ARGV0} PRIVATE ${srcs}) 
endfunction()

macro (generate_glob_list)
    foreach(prefix *.c *.cpp *.cxx *.cc ${CMAKE_C_SOURCE_FILE_EXTENSIONS} ${CMAKE_CXX_SOURCE_FILE_EXTENSIONS} ${CMAKE_ASM_SOURCE_FILE_EXTENSIONS})
        list(APPEND ${ARGV0} ${ARGV1}/*.${prefix})
    endforeach()
endmacro()

# Usage: add_executable_from_dirs(target GLOB|GLOB_RECURSE dir1 dir2 ...)
function(target_source_directories)
    set(dirs ${ARGV})
    list(REMOVE_AT dirs 0 1) 
    foreach(dir ${dirs})
        generate_glob_list(glob_list ${dir})
        file(${ARGV1} src_list LIST_DIRECTORIES false ${glob_list})
        target_source_files(${ARGV0} ${src_list})
    endforeach()
endfunction()

