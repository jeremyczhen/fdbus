
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)

set(FDBUS_JAVA_SRC_ROOT ${PROJECT_ROOT}/jni/src/java)
set(FDBUS_JAVA_SRC_DIR ${FDBUS_JAVA_SRC_ROOT}/ipc/fdbus)

file(GLOB JNI_JAVA_SRC "${FDBUS_JAVA_SRC_DIR}/*.java")
set(CMAKE_JAVA_INCLUDE_PATH ${FDBUS_JAVA_SRC_DIR})

add_jar(fdbus-jni-jar ${JNI_JAVA_SRC}
        OUTPUT_DIR ${PROJECT_ROOT}/build/install/usr/share/fdbus
        OUTPUT_NAME fdbus-jni
        GENERATE_NATIVE_HEADERS fdbus-native DESTINATION ${PROJECT_ROOT}/build/idl-gen)

set(FDBUS_CPP_SRC_ROOT ${PROJECT_ROOT}/jni/src/cpp)
file(GLOB JNI_CPP_SRC "${FDBUS_CPP_SRC_ROOT}/*.cpp")
add_library(fdbus-jni SHARED ${JNI_CPP_SRC})
target_include_directories(fdbus-jni PUBLIC ${JNI_INCLUDE_DIRS})
target_link_libraries(fdbus-jni PRIVATE fdbus-native)
install(TARGETS fdbus-jni DESTINATION usr/lib)
