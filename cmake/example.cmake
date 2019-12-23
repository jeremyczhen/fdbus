add_definitions("-DFDB_IDL_EXAMPLE_H=<idl-gen/common.base.Example.pb.h>")

add_executable(fdbobjtest
    ${PROJECT_ROOT}/example/client_server_object.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbjobtest
    ${PROJECT_ROOT}/example/job_test.cpp
)

add_executable(fdbclienttest
    ${PROJECT_ROOT}/example/fdb_test_client.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbservertest
    ${PROJECT_ROOT}/example/fdb_test_server.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

install(TARGETS fdbobjtest fdbjobtest fdbclienttest fdbservertest RUNTIME DESTINATION usr/bin)
