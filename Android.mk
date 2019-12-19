LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

idl_src := $(LOCAL_PATH)/idl
gen_base := idl-gen
idl_inc := $(LOCAL_PATH)/$(gen_base)
idl_dst := $(idl_inc)
idl_tool := LD_LIBRARY_PATH=$LD_LIBRARY_PATH:prebuilts/tools/linux-x86_64/protoc-3.0.0/lib prebuilts/tools/linux-x86_64/protoc-3.0.0/bin/protoc

gen_src_message_header := $(idl_dst)/common.base.MessageHeader.pb.cpp
gen_src_name_server := $(idl_dst)/common.base.NameServer.pb.cpp
gen_src_example := $(idl_dst)/common.base.Example.pb.cpp
gen_src_security := $(idl_dst)/common.base.Token.pb.cpp

gen_src_message_header_H := $(idl_dst)/common.base.MessageHeader.pb.h
gen_src_name_server_H := $(idl_dst)/common.base.NameServer.pb.h
gen_src_example_H := $(idl_dst)/common.base.Example.pb.h
gen_src_security_H := $(idl_dst)/common.base.Token.pb.h


LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG -DCONFIG_SOCKET_PEERCRED -DCONFIG_SOCKET_CONNECT_TIMEOUT=0

LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DCONFIG_DEBUG_LOG -DCONFIG_SOCKET_PEERCRED -DCONFIG_SOCKET_CONNECT_TIMEOUT=0

SRC_FILES := 
SRC_FILES += $(wildcard $(LOCAL_PATH)/fdbus/*.cpp)
#SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/linux/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/socket/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/platform/socket/*/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/security/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/idl-gen/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/utils/*.cpp)
SRC_FILES += $(wildcard $(LOCAL_PATH)/worker/*.cpp)

LOCAL_SRC_FILES := $(SRC_FILES:$(LOCAL_PATH)/%=%)

LOCAL_SRC_FILES += server/CBaseNameProxy.cpp \
				   server/CIntraNameProxy.cpp \
				   security/cJSON/cJSON.c


#ifdef CFG_PIPE_AS_EVENTFD
LOCAL_CPPFLAGS += -DCFG_PIPE_AS_EVENTFD
LOCAL_SRC_FILES += platform/CEventFd_pipe.cpp
#else
#LOCAL_SRC_FILES += platform/CEventFd_pipefd.cpp
#endif
LOCAL_MODULE := libcommon-base

#LOCAL_LDFLAGS := prebuilts/tools/linux-x86_64/protoc-3.11.2/lib/libprotobuf.a

LOCAL_SHARED_LIBRARIES := \
		   libprotobuf-cpp-full-rtti


LOCAL_EXPORT_C_INCLUDE_DIRS := \
		         $(LOCAL_PATH)/public \
				 $(idl_inc)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/public \
				   $(idl_inc) \
				   prebuilts/tools/linux-x86_64/protoc-3.0.0/include

$(gen_src_message_header) $(gen_src_message_header_H): $(idl_src)/common.base.MessageHeader.proto
	$(idl_tool) -I $(idl_src) --cpp_out=$(idl_dst) $<
	cp $(idl_dst)/common.base.MessageHeader.pb.cc $(idl_dst)/common.base.MessageHeader.pb.cpp
$(gen_src_name_server) $(gen_src_name_server_H): $(idl_src)/common.base.NameServer.proto
	$(idl_tool) -I $(idl_src) --cpp_out=$(idl_dst) $<
	cp $(idl_dst)/common.base.NameServer.pb.cc $(idl_dst)/common.base.NameServer.pb.cpp
$(gen_src_security) $(gen_src_security_H): $(idl_src)/common.base.Token.proto
	$(idl_tool) -I $(idl_src) --cpp_out=$(idl_dst) $<
	cp $(idl_dst)/common.base.Token.pb.cc $(idl_dst)/common.base.Token.pb.cpp
LOCAL_GENERATED_SOURCES += $(gen_src_message_header_H) $(gen_src_name_server_H) $(gen_src_security_H)

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= fdbus-jni
LOCAL_CPPFLAGS := -frtti -fexceptions -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCFG_JNI_ANDROID
LOCAL_CFLAGS := -Wno-unused-parameter -D__LINUX__ -DFDB_CFG_SOCKET_PATH=\"/data/local/tmp\" -DCONFIG_DEBUG_LOG -DCFG_JNI_ANDROID
LOCAL_SRC_FILES:= \
          jni/src/cpp/CJniClient.cpp \
          jni/src/cpp/CJniMessage.cpp \
          jni/src/cpp/CJniServer.cpp \
          jni/src/cpp/FdbusGlobal.cpp

LOCAL_SHARED_LIBRARIES := \
          libcommon-base \
          libandroid_runtime \
          liblog
          
LOCAL_C_INCLUDES += \
          frameworks/base/core/jni \
          frameworks/base/core/jni/include

include $(BUILD_SHARED_LIBRARY)
