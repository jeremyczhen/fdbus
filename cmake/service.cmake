link_libraries(common_base)

add_executable(name_server
    ${PROJECT_ROOT}/server/main_ns.cpp
    ${PROJECT_ROOT}/server/CNameServer.cpp
    ${PROJECT_ROOT}/server/CInterNameProxy.cpp
    ${PROJECT_ROOT}/server/CHostProxy.cpp
    ${PROJECT_ROOT}/security/CServerSecurityConfig.cpp
)

add_executable(host_server
    ${PROJECT_ROOT}/server/main_hs.cpp
    ${PROJECT_ROOT}/server/CHostServer.cpp
    ${PROJECT_ROOT}/security/CHostSecurityConfig.cpp
)

add_executable(lssvc
    ${PROJECT_ROOT}/server/main_ls.cpp
)

add_executable(lshost
    ${PROJECT_ROOT}/server/main_lh.cpp
)

add_executable(logsvc
    ${PROJECT_ROOT}/server/main_log_server.cpp
    ${PROJECT_ROOT}/server/CLogPrinter.cpp
)

add_executable(logviewer
    ${PROJECT_ROOT}/server/main_log_client.cpp
    ${PROJECT_ROOT}/server/CLogPrinter.cpp
)

install(TARGETS name_server host_server lssvc lshost logsvc logviewer RUNTIME DESTINATION usr/bin)
