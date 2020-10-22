add_definitions("-DFDB_IDL_EXAMPLE_H=<idl-gen/common.base.Example.pb.h>")

add_executable(fdbobjtest
    ${PACKAGE_SOURCE_ROOT}/example/object/client_server_object.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbjobtest
    ${PACKAGE_SOURCE_ROOT}/example/job/job_test.cpp
)

add_executable(fdbclienttest
    ${PACKAGE_SOURCE_ROOT}/example/client-server/fdb_test_client.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbservertest
    ${PACKAGE_SOURCE_ROOT}/example/client-server/fdb_test_server.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbntfcentertest
    ${PACKAGE_SOURCE_ROOT}/example/notification-center/fdb_test_notification_center.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

add_executable(fdbappfwtest
    ${PACKAGE_SOURCE_ROOT}/example/app-framework/fdb_appfw_test.cpp
    ${IDL_GEN_ROOT}/idl-gen/common.base.Example.pb.cc
)

install(TARGETS fdbobjtest fdbjobtest fdbclienttest fdbservertest fdbntfcentertest fdbappfwtest RUNTIME DESTINATION usr/bin)
