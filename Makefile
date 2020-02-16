INSTALL_ROOT := ../../../qcom/qcom_qnx/apps/qnx_ap/install/aarch64le
PROTO_ROOT := ${INSTALL_ROOT}
PROTOC_ROOT := ../protobuf-3.0.0/protoc-tool/linux-x86_64

.PHONY: all
all:
	cwd=`pwd` && install_root=$$cwd/${INSTALL_ROOT} && \
	cmake -Bbuild \
            -Hcmake\
            -DCMAKE_INSTALL_PREFIX=$$install_root \
	    -DCMAKE_CXX_FLAGS='-Vgcc_ntoaarch64 -O2 -Wc,-Wall -g -Os -Wall -march=armv8-a -mcpu=cortex-a57 -mtune=cortex-a57 -fstack-protector-strong -EL -DGOOGLE_PROTOBUF_ARCH_64_BIT' \
	    -DCMAKE_C_FLAGS='-Vgcc_ntoaarch64 -O2 -Wc,-Wall -g -Os -Wall -march=armv8-a -mcpu=cortex-a57 -mtune=cortex-a57 -fstack-protector-strong -EL' \
	    -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
	    -Dfdbus_LOG_TO_STDOUT=ON \
	    -Dfdbus_SOCKET_ENABLE_PEERCRED=OFF \
	    -Dfdbus_PIPE_AS_EVENTFD=ON \
	    -Dfdbus_LINK_SOCKET_LIB=ON \
	    -Dfdbus_LINK_PTHREAD_LIB=OFF
	make -C build VERBOSE=1 -j16 install
	cwd=`pwd` && install_root=$$cwd/${INSTALL_ROOT} && proto_root=$$cwd/${PROTO_ROOT} && \
	cmake -Bbuild-example \
            -Hcmake/pb-example\
            -DCMAKE_INSTALL_PREFIX=$$install_root \
	    -DSYSTEM_ROOT=$$install_root \
	    -DCMAKE_CXX_FLAGS='-Vgcc_ntoaarch64 -O2 -Wc,-Wall -g -Os -Wall -march=armv8-a -mcpu=cortex-a57 -mtune=cortex-a57 -fstack-protector-strong -EL -DGOOGLE_PROTOBUF_ARCH_64_BIT' \
	    -DCMAKE_C_FLAGS='-Vgcc_ntoaarch64 -O2 -Wc,-Wall -g -Os -Wall -march=armv8-a -mcpu=cortex-a57 -mtune=cortex-a57 -fstack-protector-strong -EL' \
	    -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake \
	    -Dfdbus_LOG_TO_STDOUT=ON \
	    -Dfdbus_SOCKET_ENABLE_PEERCRED=OFF \
	    -Dfdbus_PIPE_AS_EVENTFD=true \
	    -Dfdbus_LINK_SOCKET_LIB=true
	cwd=`pwd` && protoc_root=$$cwd/${PROTOC_ROOT} && \
	     PATH=$$protoc_root/bin:$$PATH LD_LIBRARY_PATH=$$protoc_root/lib:$$LD_LIBRARY_PATH make -C build-example VERBOSE=1 -j16 install

.PHONY: install
install: all
	echo nop

.PHONY: clean
clean:
	echo nop
