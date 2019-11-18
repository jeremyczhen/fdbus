Introduction
============

`FDBus Documentation <https://fdbus.readthedocs.io/en/latest/?badge=latest>`_

.. image:: https://readthedocs.org/projects/fdbus/badge/?version=latest

``FDBus`` is a middleware development framework targeting the following objectives:

- Inter-Process Communication (``IPC``) within single host and cross the network
- System abstraction (``Windows``, ``Linux``, ``QNX``)
- Components based on which middleware is built (job, worker, timer, watch...)

It is something like ``DBus`` or ``SOME/IP``, but with its own characteristic:

- **Distributed** : unlike ``DBus``, it has no central hub
- **High performance** : endpoints talk to each other directly
- **Addressing by name** : service is addressable through logic name
- **Address allocation** : service address is allocated dynamically
- **Networking** : communication inside host and cross hosts
- **IDL and code generation** : using protocol buffer
- **Language binding** : C++ Java
- **Total slution** : it is more than an ``IPC`` machanism. it is a middleware development framework

Its usage can be found in the following fields:

- Infotainment; instrument cluster, TBox and other ECU with posix-compatible OS running
- Inter VM communication between guest OSes in hypervisor
- SOHO Gateway
- Instrument for distributed industry control

Supported system
----------------

- ``Linux``
- ``Windows``
- ``QNX``

Dependence
----------
- cmake - 3.1.3 or above for non-jni build
- cmake - 3.11.1 or above for jni build
- protocol buffer
- compiler supporting C++11 (gcc 4.7+ for Linux; Visual Studio for Windows)

Download
--------
https://github.com/jeremyczhen/fdbus.git

Documentation & Blog
--------------------
https://blog.csdn.net/jeremy_cz/article/details/89060291

How to build
------------
For Ubuntu host version (running at host machine)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Dependence:

- cmake, gcc are installed

1. build protocol buffer

.. code-block:: bash

   1.1 cd ~/workspace
   1.2 git clone https://github.com/protocolbuffers/protobuf.git #get protobuf source code
   1.3 cd protobuf;git submodule update --init --recursive
   1.4 mkdir -p build/install;cd build #create directory for out-of-source build
   1.5 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 ../cmake
   1.6 make -j4 install #build and install to build/install directory

2. build fdbus

.. code-block:: bash

   2.1 cd ~/workspace
   2.2 git clone https://github.com/jeremyczhen/fdbus.git #get fdbus source code
   2.3 cd fdbus;mkdir -p build/install;cd build #create directory for out-of-source build
   2.4 cmake -DSYSTEM_ROOT=~/workspace/protobuf/build/install -DCMAKE_INSTALL_PREFIX=install ../cmake
   2.5 PATH=~/workspace/protobuf/build/install/bin:$PATH make #set PATH to the directory where protoc can be found

For cross compiling on Ubuntu (target version)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Dependence:

- cmake, gcc and toolchain are installed

1 build protocol buffer

.. code-block:: bash

   1.1 cd ~/workspace
   1.2 create toolchain.cmake #create toolchain.cmake and set g++ and gcc for target build in cmake/toolchain.cmake (see below)
   1.3 git clone https://github.com/protocolbuffers/protobuf.git protobuf-host #get protobuf source code for host build
   1.4 cd protobuf-host && git submodule update --init --recursive && cd ..
   1.5 cp protobuf-host protobuf-target -r #create a copy for cross compiling
   1.6 cd protobuf-host;mkdir -p build/install;cd build #create directory for out-of-source build
   1.7 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 ../cmake
   1.8 make -j4 install #build and install to build/install directory; now we have protoc running at host
   1.9 cd ../../protobuf-target;mkdir -p build/install;cd build #create directory for out-of-source build
   1.10 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 -DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake ../cmake
   1.11 PATH=~/workspace/protobuf-host/build/install/bin:$PATH make -j4 install #build and install to build/install directory

2. build fdbus

.. code-block:: bash

   2.1 cd ~/workspace
   2.2 git clone https://github.com/jeremyczhen/fdbus.git
   2.3 cd fdbus;mkdir -p build/install;cd build
   2.4 cmake -DSYSTEM_ROOT=~/workspace/protobuf-target/build/install -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake ../cmake
   2.5 PATH=~/workspace/protobuf-host/build/install/bin:$PATH make #set PATH to the directory where protoc can be found

For QNX
^^^^^^^

The same as cross compiling, but when building fdbus, should add the following option to cmake since QNX doesn't support peercred and eventfd:

.. code-block:: bash

   -Dfdbus_SOCKET_ENABLE_PEERCRED=OFF -Dfdbus_PIPE_AS_EVENTFD=true

For Android NDK
^^^^^^^^^^^^^^^
 Dependence:
 
 - cmake, gcc are installed, also need android NDK

1 build protocol buffer

.. code-block:: bash
    
    1.1 build host is the same as previously discussed
    1.2 cd ./protobuf-target;mkdir -p build/install;cd build #create directory for out-of-source build
    1.3 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 -DANDROID_LINKER_FLAGS="-landroid -llog" -Dprotobuf_BUILD_PROTOC_BINARIES=0 -Dprotobuf_BUILD_TESTS=0 -DCMAKE_TOOLCHAIN_FILE=~/android-ndk-r20/build/cmake/android.toolchain.cmake ../cmake
    1.4 PATH=~/workspace/protobuf-target/build/install/bin:$PATH make -j4 install #build and install to build/install directory

2 build fdbus

.. code-block:: bash
    
    2.1 cd ~/workspace
    2.2 git clone https://github.com/jeremyczhen/fdbus.git
    2.3 cd fdbus;mkdir -p build/install;cd build
    2.4 cmake -DSYSTEM_ROOT=~/workspace/protobuf-target/build/install -DCMAKE_INSTALL_PREFIX=install -Dfdbus_ANDROID=ON -DCMAKE_TOOLCHAIN_FILE=~/android-ndk-r20/build/cmake/android.toolchain.cmake ../cmake
    2.5 PATH=~/workspace/protobuf-host/build/install/bin:$PATH make #set PATH to the directory where protoc can be found

For Windows version
^^^^^^^^^^^^^^^^^^^

Dependence:

- cmake, msvc are installed

1 build protocol buffer

.. code-block:: bash

   1.1 cd c:\workspace
   1.2 #suppose source code of protocol buffer is already downloaded and placed at c:\workspace\protobuf
   1.3 cd protobuf;mkdir -p cbuild\install;cd cbuild #create directory for out-of-source build
   1.4 cmake -DCMAKE_INSTALL_PREFIX=install -Dprotobuf_WITH_ZLIB=OFF ..\cmake
   1.5 open protobuf.sln in c:\workspace\protobuf\cbuild and build project INSTALL

2. build fdbus

.. code-block:: bash

   2.1 cd ~/workspace
   2.2 #suppose source code of fdbus is already downloaded and placed at c:\workspace\fdbus
   2.3 cd fdbus;mkdir -p build\install;cd build #create directory for out-of-source build
   2.4 cmake -DSYSTEM_ROOT=c:\workspace\protobuf\build\install -DCMAKE_INSTALL_PREFIX=install ..\cmake
   2.5 copy c:\workspace\protobuf\cbuild\install\bin\protoc.exe to the directory in PATH environment variable
   2.6 open fdbus.sln in c:\workspace\fdbus\build and build project INSTALL

For cross compiling on Windows (target version)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
1. you should have cross-compiling toolchain installed (such as linaro ARM complier)
2. you should have 'make.exe' installed
3. run 'cmake' as before, adding "-DCMAKE_TOOLCHAIN_FILE=../../toolchain.cmake". Makefiles will be generated.
4. if you have visual studio installed, cmake will by default generate visual studio solution rather than makefiles. To avoid this, adding -G "Unix Makefiles" option, which forces cmake to generate makefile.

How to run
----------
For single host
^^^^^^^^^^^^^^^

.. code-block:: bash

   1. start name server:
      > name_server
   2. start clients/servers

For multi-host
^^^^^^^^^^^^^^

.. code-block:: bash

   1. start name server at host1:
      host1> name_server
   2. start host server at host1:
   3. start name server at host2:
      host2> name_server -u tcp://ip_of_host1:60000
   4. start clients/servers at host1 and host2

example of toolchain.cmake for cross-compiling
----------------------------------------------

.. code-block:: bash

   >>>> cat toolchain.cmake
   SET(CMAKE_SYSTEM_NAME Linux)
   SET(CMAKE_CXX_COMPILER ~/project/android/workspace/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu-7.1.1/bin/aarch64-linux-gnu-g++)
   SET(CMAKE_C_COMPILER ~/project/android/workspace/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu-7.1.1/bin/aarch64-linux-gnu-gcc)

cmake options
-------------

.. note::

   The following options can be specified with ``-Dfdbus_XXX=ON/OFF`` when running ``cmake``.
   The status with ``*`` is set as default.

``fdbus_BUILD_TESTS``
 | *``ON`` : build examples
 | ``OFF``: don't build examples
``fdbus_ENABLE_LOG``
 | *``ON`` : enable log output of fdbus lib
 | ``OFF``: disable log output of fdbus lib
``fdbus_LOG_TO_STDOUT``
 | ``ON`` : send fdbus log to stdout (terminal)
 | *``OFF``: fdbus log is sent to log server
``fdbus_ENABLE_MESSAGE_METADATA``
 | *``ON`` : time stamp is included in fdbus message to track delay of message during request-reply interaction
 | ``OFF``: time stamp is disabled
``fdbus_SOCKET_BLOCKING_CONNECT``
 | ``ON`` : socket method connect() will be blocked forever if server is not ready to accept
 | *``OFF``: connect() will be blocked with timer to avoid permanent blocking
``fdbus_SOCKET_ENABLE_PEERCRED``
 | *``ON`` : peercred of UDS (Unix Domain Socket) is enabled
 | ``OFF``: peercred of UDS is disabled
``fdbus_ALLOC_PORT_BY_SYSTEM``
 | ``ON`` : socket number of servers are allocated by the system
 | *``OFF``: socket number of servers are allocated by name server
``fdbus_SECURITY``
 | ``ON`` : enable security
 | *``OFF``: disable security
``fdbus_BUILD_JNI``
 | ``ON`` : build JNI shared library and jar package
 | *``OFF``: don't build JNI artifacts

.. note::

   The following options can be specified with 
   ``-DMACRO_DEF='VARIABLE=value;VARIABLE=value'``

``FDB_CFG_SOCKET_PATH``
 | specify directory of UDS file
 | default: /tmp

``CONFIG_SOCKET_CONNECT_TIMEOUT``
 | specify timeout of connect() when connect to socket server in ms. 
   "``0``" means block forever.
 | default: 2000

Security concept
----------------
Authentication of client:
^^^^^^^^^^^^^^^^^^^^^^^^^

 | 1. server registers its name to ``name server``;
 | 2. ``name server`` reply with URL and token;
 | 3. server binds to the URL and holds the token;
 | 4. client requests name resolution from ``name server``;
 | 5. ``name server`` authenticate client by checking peercred
   (``SO_PEERCRED`` option of socket), including ``UID``, ``GID`` of the client
 | 6. if success, ``name server`` gives URL and token of requested server to
   the client
 | 7. client connects to the server with URL followed by sending the token 
   to the server
 | 8. server verify the token and grant the connection if pass; 
   for unauthorized client, since it does not have a valid token, server will 
   drop the connection 
 | 9. ``name server`` can assign multiple tokens to server but only send one 
   of them to the client according to security level of the client

Authenication of host
^^^^^^^^^^^^^^^^^^^^^

TBD

Known issues
^^^^^^^^^^^^^^^^^^^^^

 | 1. Issue: sem_timedwait() is used as notifier and blocker of event loop, leading to timer failure when TOD is changed since sem_wait() take CLOCK_REALTIME clock for timeout control.
 |    Solution: When creating worker thread, pass FDB_WORKER_ENABLE_FD_LOOP as parameter, forcing poll() instead of sem_timedwait() as loop notifier and blocker
