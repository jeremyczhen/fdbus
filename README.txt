Fast Distributed Bus (FDBus)
============================

FDBus is a middleware development framework targeting the following objectives:
- Inter-Process Communication (IPC) within single host and cross the network
- System abstraction (Windows, Linux, QNX)
- Components based on which middleware is built (job, worker, timer, watch...)

It is something like DBus or SOME/IP, but with its own characteristic:
- Distributed: unlike DBus, it has no central hub
- High performance: endpoints talk to each other directly
- Addressing by name: service is addressable through logic name
- Address allocation: service address is allocated dynamically
- Networking: communication inside host and cross hosts
- IDL and code generation: using protocol buffer
- Total slution: it is more than an IPC machanism. it is a middleware development framework

Its usage can be found in the following fields:
- Infotainment; instrument cluster, TBox and other ECU with posix-compatible OS running
- Inter VM communication between guest OSes in hypervisor
- SOHO Gateway
- Instrument for distributed industry control

Supported system
----------------
- Linux
- Windows
- QNX

Dependence
----------
- cmake - 3.1.3 or above
- protocol buffer
- compiler supporting C++11

Download
--------
https://github.com/jeremyczhen/fdbus.git

How to build
------------
>>>> For Ubuntu host version (running at host machine)
Dependence:
- cmake, gcc are installed
1. build protocol buffer
1.1 cd ~/workspace
1.2 git clone https://github.com/protocolbuffers/protobuf.git #get protobuf source code
1.3 cd protobuf;mkdir -p build/install;cd build #create directory for out-of-source build
1.4 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 ../cmake
1.5 make -j4 install #build and install to build/install directory
2. build fdbus
2.1 cd ~/workspace
2.2 git clone https://github.com/jeremyczhen/fdbus.git #get fdbus source code
1.3 cd fdbus;mkdir -p build/install;cd build #create directory for out-of-source build
1.4 cmake -DSYSTEM_ROOT=~/workspace/protobuf/build/install -DCMAKE_INSTALL_PREFIX=install ../cmake
1.5 PATH=~/workspace/protobuf/build/install/bin:$PATH make #set PATH to the directory where protoc can be found

>>>> For cross compiling on Ubuntu (target version)
Dependence:
- cmake, gcc and toolchain are installed
1 build protocol buffer
1.1 cd ~/workspace
1.2 create toolchain.cmake #create toolchain.cmake and set g++ and gcc for target build in cmake/toolchain.cmake (see below)
1.3 git clone https://github.com/protocolbuffers/protobuf.git protobuf-host #get protobuf source code for host build
1.4 cp protobuf-host protobuf-target -r #create a copy for cross compiling
1.5 cd protobuf-host;mkdir -p build/install;cd build #create directory for out-of-source build
1.6 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 ../cmake
1.7 make -j4 install #build and install to build/install directory; now we have protoc running at host
1.8 cd ../../protobuf-target;mkdir -p build/install;cd build #create directory for out-of-source build
1.9 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 -DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake ../cmake
1.10 PATH=~/workspace/protobuf-host/build/install/bin:$PATH make -j4 install #build and install to build/install directory
2. build fdbus
2.1 cd ~/workspace
2.2 git clone https://github.com/jeremyczhen/fdbus.git
1.3 cd fdbus;mkdir -p build/install;cd build
1.4 cmake -DSYSTEM_ROOT=~/workspace/protobuf-target/build/install -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake ../cmake
1.5 PATH=~/workspace/protobuf-host/build/install/bin:$PATH make #set PATH to the directory where protoc can be found

>>>> For QNX
The same as cross compiling, but when building fdbus, should add the following option to cmake since QNX doesn't support peercred:
-Dfdbus_SOCKET_ENABLE_PEERCRED=OFF

>>>> For Windows version
Dependence:
- cmake, msvc are installed
1 build protocol buffer
1.1 cd c:\workspace
1.2 #suppose source code of protocol buffer is already downloaded and placed at c:\workspace\protobuf
1.3 cd protobuf;mkdir -p cbuild\install;cd cbuild #create directory for out-of-source build
1.4 cmake -DCMAKE_INSTALL_PREFIX=install ..\cmake
1.5 open protobuf.sln in c:\workspace\protobuf\cbuild and build project INSTALL
2. build fdbus
2.1 cd ~/workspace
2.2 #suppose source code of fdbus is already downloaded and placed at c:\workspace\fdbus
2.3 cd fdbus;mkdir -p build\install;cd build #create directory for out-of-source build
2.4 cmake -DSYSTEM_ROOT=c:\workspace\protobuf\build\install -DCMAKE_INSTALL_PREFIX=install ..\cmake
2.5 copy c:\workspace\protobuf\cbuild\install\bin\protoc.exe to the directory in PATH environment variable
2.6 open fdbus.sln in c:\workspace\fdbus\build and build project INSTALL

example of toolchain.cmake for cross-compiling
----------------------------------------------
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_CXX_COMPILER ~/project/android/workspace/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu-7.1.1/bin/aarch64-linux-gnu-g++)
SET(CMAKE_C_COMPILER ~/project/android/workspace/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu-7.1.1/bin/aarch64-linux-gnu-gcc)
