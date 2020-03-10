#=====================================================================================
#                   makefile to build fdbus in aosp source tree                      |
#=====================================================================================

#=====================================================================================
#                           build libcommon-base.so                                  |
#=====================================================================================
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libcommon-base

LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCONFIG_SOCKET_PEERCRED -DCONFIG_SOCKET_CONNECT_TIMEOUT=0 -DCONFIG_LOG_TO_STDOUT
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCONFIG_SOCKET_PEERCRED -DCONFIG_SOCKET_CONNECT_TIMEOUT=0 -DCONFIG_LOG_TO_STDOUT

SRC_FILES := 
SRC_FILES += $(wildcard $(LOCAL_PATH)/fdbus/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/linux/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/socket/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/socket/*/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/security/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/utils/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/worker/*.cpp)

LOCAL_SRC_FILES := $(SRC_FILES:$(LOCAL_PATH)/%=%)

LOCAL_SRC_FILES += server/CBaseNameProxy.cpp \
                   server/CIntraNameProxy.cpp \
                   security/cJSON/cJSON.c

FDB_IDL_DIR := $(LOCAL_PATH)/idl

LOCAL_SRC_FILES += platform/CEventFd_eventfd.cpp

LOCAL_EXPORT_C_INCLUDE_DIRS := \
                   $(LOCAL_PATH)/public

LOCAL_C_INCLUDES += $(LOCAL_PATH)/public

include $(BUILD_SHARED_LIBRARY)

#=====================================================================================
#                             build libfdbus-jni.so                                  |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= libfdbus-jni
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCFG_JNI_ANDROID -DFDB_CFG_KEEP_ENV_TYPE
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCFG_JNI_ANDROID
LOCAL_SRC_FILES:= \
              jni/src/cpp/CJniClient.cpp \
              jni/src/cpp/CJniMessage.cpp \
              jni/src/cpp/CJniServer.cpp \
              jni/src/cpp/FdbusGlobal.cpp

LOCAL_SHARED_LIBRARIES := \
              libcommon-base
          
LOCAL_C_INCLUDES += \
              frameworks/base/core/jni \
              frameworks/base/core/jni/include

include $(BUILD_SHARED_LIBRARY)

#=====================================================================================
#                                build fdbus-jni.jar                                 |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_SDK_VERSION := current
LOCAL_MODULE := fdbus-jni
LOCAL_SRC_FILES := $(call all-java-files-under, jni/src/java/ipc/fdbus)
include $(BUILD_JAVA_LIBRARY)

#=====================================================================================
#               build fdbus-java-client.jar (java test client)                       |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_SDK_VERSION := current
LOCAL_MODULE := fdbus-java-client
LOCAL_PROTOC_OPTIMIZE_TYPE := lite 
LOCAL_PROTOC_FLAGS := -Iidl
LOCAL_SOURCE_FILES_ALL_GENERATED := true
LOCAL_SRC_FILES := jni/test/MediaClient.java \
            jni/test/MyFdbusMessageEncoder.java \
            jni/test/CPerson.java \
            idl/common.base.Example.proto
LOCAL_JAVA_LIBRARIES := fdbus-jni \
            libprotobuf-java-lite
include $(BUILD_JAVA_LIBRARY)

#=====================================================================================
#               build fdbus-java-server.jar (java test server)                       |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_SDK_VERSION := current
LOCAL_MODULE := fdbus-java-server
LOCAL_PROTOC_OPTIMIZE_TYPE := lite 
LOCAL_PROTOC_FLAGS := -Iidl
LOCAL_SOURCE_FILES_ALL_GENERATED := true
LOCAL_SRC_FILES := jni/test/MediaServer.java \
            jni/test/MyFdbusMessageEncoder.java \
            jni/test/CPerson.java \
            idl/common.base.Example.proto
LOCAL_JAVA_LIBRARIES := fdbus-jni \
            libprotobuf-java-lite
include $(BUILD_JAVA_LIBRARY)

#=====================================================================================
#                                  build name-server                                 |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= name-server
#LOCAL_INIT_RC := fdbus-name-server.rc
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                server/main_ns.cpp \
                server/CNameServer.cpp \
                server/CInterNameProxy.cpp \
                server/CHostProxy.cpp \
                security/CServerSecurityConfig.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base

include $(BUILD_EXECUTABLE)

#=====================================================================================
#                                  build host-server                                 |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= host-server
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\"
LOCAL_SRC_FILES:= \
                server/main_hs.cpp \
                server/CHostServer.cpp \
                security/CHostSecurityConfig.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base

include $(BUILD_EXECUTABLE)

#=====================================================================================
#                             build lssvc (list service)                             |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= lssvc
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                server/main_ls.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base
include $(BUILD_EXECUTABLE)

#=====================================================================================
#                              build lshost (list host)                              |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= lshost
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                server/main_lh.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base
include $(BUILD_EXECUTABLE)

#=====================================================================================
#                             build logsvc (log service)                             |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= logsvc 
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                server/main_log_server.cpp \
                server/CLogPrinter.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base

include $(BUILD_EXECUTABLE)

#=====================================================================================
#                           build logviewer (log viewer)                             |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= logviewer
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                server/main_log_client.cpp \
                server/CLogPrinter.cpp

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base
include $(BUILD_EXECUTABLE)

#=====================================================================================
#                            build fdbtest (native test)                             |
#=====================================================================================
include $(CLEAR_VARS)
LOCAL_MODULE:= fdbtest 
FDB_IDL_EXAMPLE_H = \<$(FDB_IDL_DIR)/common.base.Example.pb.h\>
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG -DFDB_IDL_EXAMPLE_H=$(FDB_IDL_EXAMPLE_H)
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG
LOCAL_SRC_FILES:= \
                example/client_server_object.cpp \
                example/CFdbProtoMsgBuilder.cpp \
                idl/common.base.Example.proto

LOCAL_SHARED_LIBRARIES := \
                libprotobuf-cpp-full-rtti \
                libcommon-base

LOCAL_PROTOC_OPTIMIZE_TYPE := full 
LOCAL_PROTOC_FLAGS := -I.

include $(BUILD_EXECUTABLE)
