link_libraries(fdbus-clib)

add_executable(fdbtestclibserver
    ${PACKAGE_SOURCE_ROOT}/c/test/fdbus_test_server.c
)

add_executable(fdbtestclibclient
    ${PACKAGE_SOURCE_ROOT}/c/test/fdbus_test_client.c
)

install(TARGETS fdbtestclibserver fdbtestclibclient RUNTIME DESTINATION usr/bin)
