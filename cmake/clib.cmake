file(GLOB CLIB_SOURCES "${PROJECT_ROOT}/c/*.cpp")
add_library(fdbus-clib "SHARED" ${CLIB_SOURCES})

install(TARGETS fdbus-clib DESTINATION usr/lib)
