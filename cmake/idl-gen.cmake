
FILE(GLOB_RECURSE PROTO_SOURCES "${PACKAGE_SOURCE_ROOT}/idl/*.proto")

set(gen_tool "protoc")
set(GEN_DIR ${IDL_GEN_ROOT}/idl-gen)
file(MAKE_DIRECTORY ${GEN_DIR})

set(PROTO_TARGET "build_proto_source")
add_custom_target(${PROTO_TARGET})
foreach(idl ${PROTO_SOURCES})
    get_filename_component(base_name ${idl} NAME)
    string(FIND ${base_name} "." SHORTEST_EXT_POS REVERSE)
    string(SUBSTRING ${base_name} 0 ${SHORTEST_EXT_POS} base_name_we)
    #message(FATAL_ERROR "${base_name}, ${base_name_we}")

    set(gen_header ${GEN_DIR}/${base_name_we}.pb.h)
    set(gen_source ${GEN_DIR}/${base_name_we}.pb.cc)
    add_custom_command(OUTPUT ${gen_source} ${gen_header}
        COMMAND ${gen_tool} -I${PACKAGE_SOURCE_ROOT}/idl --cpp_out=${GEN_DIR} ${idl}
        DEPENDS ${idl}
        WORKING_DIRECTORY ${PACKAGE_SOURCE_ROOT}/idl
    )
    add_custom_target(${base_name_we} ALL DEPENDS ${gen_source} ${gen_header})
    add_dependencies(${PROTO_TARGET} ${base_name_we})
    set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${gen_header};${gen_source}")
endforeach()

