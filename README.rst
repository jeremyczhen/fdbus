Introduction
============

`FDBus Documentation <https://fdbus.readthedocs.io/en/latest/?badge=latest>`_

.. image:: https://readthedocs.org/projects/fdbus/badge/?version=latest

``FDBus`` is an easy-to-use, light weight and high performance IPC framework. It is something like ``DBus`` or ``SOME/IP``, but with its own characteristics:

- **Distributed** : unlike ``DBus``, it has no central hub; client and serives are connected directly
- **High performance** : endpoints talk to each other directly
- **Addressing by name** : service is addressable through logic name
- **Address allocation** : service address is allocated dynamically
- **Networking** : communication inside host and across network
- **IDL and code generation** : google protocol buffer is recommended
- **Language binding** : C++ C Java Python
- **Total slution** : it is more than an ``IPC`` machanism. it is more like a middleware development framework
- **Notification Center** : In addition to distributed service, it also support centralized notification center like MQTT
- **Logging and Debugging** : All FDBus messages can be filtered and logged; services can be monitored; connected clients can be listed...

Its usage can be found in the following fields:

- Infotainment; instrument cluster, TBox and other ECU with posix-compatible OS running
- Inter VM communication between guest OSes in hypervisor
- SOHO Gateway
- Instrument for distributed industry control
- Backbone to support SOA (Service-Oriented Architecture)

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
================
Build with buildCentral
-------------------------
It is recommended to build with buildCentral.

Prepare buildCentral
^^^^^^^^^^^^^^^^^^^^^^
1. Install python from `Python <https://www.python.org/downloads/>`_
2. Install the following modules:

 - sudo pip install python-magic
 - sudo pip install networkx
 - sudo pip install simplejson

3. Download buildCentral:

 - git clone https://github.com/jeremyczhen/buildCentral.git

Build with buildCentral
^^^^^^^^^^^^^^^^^^^^^^^^

1. Enter buildCentral/workspace to download FDBus and protobuf:

 - git clone https://github.com/jeremyczhen/fdbus.git
 - git clone https://github.com/jeremyczhen/protobuf.git

2. Supposing we are at root of buildCentral (buildCentral/), build host (Windows, Ubuntu, ...) version of FDBus with the following command. FDBus can be found at: ``output/stage/host``.

 - tools/buildCentral/install/mk -thost

3. Cross build for other architectures and FDBus can be found at: ``output/stage/$variant/$target_arch/``:

 - tools/buildCentral/install/mk -tarm-qnx          - build for ARM QNX
 - tools/buildCentral/install/mk -twin-lin          - build for Windows Linario

4. You can configure toolchain at: ``project/build/buildcentralrc``

For more details please find help for `buildCentral <https://github.com/jeremyczhen/buildCentral>`_

Build with commands
---------------------
For Ubuntu host version (running at host machine)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. [1]

Dependence:

- cmake, gcc are installed

.. code-block:: bash

 cd ~/workspace
 git clone https://github.com/jeremyczhen/fdbus.git #get fdbus source code
 cd fdbus;mkdir -p build/install;cd build #create directory for out-of-source build
 cmake -DCMAKE_INSTALL_PREFIX=install ../cmake
 make install

For cross compiling on Ubuntu (target version)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Dependence:

- cmake and cross-compiling toolchain are installed

.. code-block:: bash

 cd ~/workspace
 git clone https://github.com/jeremyczhen/fdbus.git
 cd fdbus;mkdir -p build/install;cd build
 #update ../cmake/toolchain.cmake (see below example)
 cmake -DCMAKE_INSTALL_PREFIX=install -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain.cmake ../cmake
 make install

For QNX
^^^^^^^

The same as cross compiling, but the following option should be added to cmake due to minor difference in QNX SDP:

::

 -Dfdbus_SOCKET_ENABLE_PEERCRED=OFF -Dfdbus_PIPE_AS_EVENTFD=true -Dfdbus_LINK_SOCKET_LIB=true

For Windows version
^^^^^^^^^^^^^^^^^^^
.. [2]

Dependence:

- cmake, msvc are installed

1. cd c:\\workspace
2. suppose source code of fdbus is already downloaded and placed at c:\\workspace\\fdbus
3. create directory c:\\workspace\\fdbus\\build\\install and enter c:\\workspace\\fdbus\\build
4. cmake -DCMAKE_INSTALL_PREFIX=install ..\\cmake
5. open fdbus.sln in c:\\workspace\\fdbus\\build and build project INSTALL

For cross compiling on Windows (target version)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
1. you should have cross-compiling toolchain installed (such as linaro ARM complier)
2. you should have 'make.exe' installed
3. run 'cmake' as [2]_, adding "-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain.cmake -G "Unix Makefiles"". Makefiles will be generated.

Build FDBus example (depends on protobuf) for Ubuntu
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
1. Build protocol buffer

.. code-block:: bash

 cd ~/workspace
 git clone https://github.com/protocolbuffers/protobuf.git #get protobuf source code
 cd protobuf;git submodule update --init --recursive
 mkdir -p build/install;cd build #create directory for out-of-source build
 cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_SHARED_LIBS=1 ../cmake
 make -j4 install #build and install to build/install directory

2. Build fdbus [1]_
3. Supposing it is available at ~/workspace/fdbus, build fdbus example

.. code-block:: bash

 cd ~/workspace/fdbus;mkdir -p build-example/install;cd build-example #create directory for out-of-source build
 cmake -DSYSTEM_ROOT=~/workspace/protobuf/build/install;~/workspace/fdbus/build/install -DCMAKE_INSTALL_PREFIX=install ../cmake
 PATH=~/workspace/protobuf/build/install/bin:$PATH make install #set PATH to the directory where protoc can be found

Build FDBus example (depends on protobuf) for Windows
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Dependence:

- cmake, msvc are installed

1 build protocol buffer

 a) cd c:\\workspace
 b) suppose source code of protocol buffer is already downloaded and placed at c:\\workspace\\protobuf
 c) create directory c:\\workspace\\protobuf\\cbuild\\install and enter c:\\protobuf\\fdbus\\cbuild
 d) cmake -DCMAKE_INSTALL_PREFIX=install -Dprotobuf_WITH_ZLIB=OFF ..\\cmake
 e) open protobuf.sln in c:\workspace\protobuf\cbuild and build project INSTALL

2. Build fdbus [2]_
3. Supposing it is available at c:\\workspace\\fdbus, build example for fdbus

 a) create directory c:\\workspace\\fdbus\\build-example\\install and enter c:\\workspace\\fdbus\\build-example
 b) cmake -DSYSTEM_ROOT=c:\\workspace\\protobuf\\build\\install;c:\\workspace\\fdbus\\build\\install -DCMAKE_INSTALL_PREFIX=install ..\\cmake\\pb-example
 c) copy c:\\workspace\\protobuf\\cbuild\\install\\bin\\protoc.exe to the directory in PATH environment variable
 d) open fdbus.sln in c:\\workspace\\fdbus\\build-example and build project INSTALL

How to run
----------
For single host
^^^^^^^^^^^^^^^
1. start name server:

 > name_server

2. start clients/servers
3. using lssvc, logsvc, logviewer to look into details

For multi-host
^^^^^^^^^^^^^^

1. start name server at host1:

  host1> name_server

2. start host server at host1:

  host1> host_server

3. start name server at host2:

 host2> name_server -u tcp://ip_of_host1:60000

4. start clients/servers at host1 or host2
5. using lssvc, logsvc, logviewer to look into details

example of toolchain.cmake for cross-compiling
----------------------------------------------

::

   > cat toolchain.cmake
   SET(CMAKE_SYSTEM_NAME Linux)
   SET(CMAKE_CXX_COMPILER $ENV{QNX_HOST}/usr/bin/q++)
   SET(CMAKE_C_COMPILER $ENV{QNX_HOST}/usr/bin/qcc)


cmake options
-------------

The following options can be specified with ``-Dfdbus_XXX=ON/OFF`` when running ``cmake``.
   The status with ``*`` is set as default.

``fdbus_BUILD_TESTS``
 | \*``ON`` : build examples
 | ``OFF``: don't build examples
``fdbus_ENABLE_LOG``
 | \*``ON`` : enable log output of fdbus lib
 | ``OFF``: disable log output of fdbus lib
``fdbus_LOG_TO_STDOUT``
 | ``ON`` : send fdbus log to stdout (terminal)
 | \*``OFF``: fdbus log is sent to log server
``fdbus_ENABLE_MESSAGE_METADATA``
 | \*``ON`` : time stamp is included in fdbus message to track delay of message during request-reply interaction
 | ``OFF``: time stamp is disabled
``fdbus_SOCKET_BLOCKING_CONNECT``
 | ``ON`` : socket method connect() will be blocked forever if server is not ready to accept
 | \*``OFF``: connect() will be blocked with timer to avoid permanent blocking
``fdbus_SOCKET_ENABLE_PEERCRED``
 | \*``ON`` : peercred of UDS (Unix Domain Socket) is enabled
 | ``OFF``: peercred of UDS is disabled
``fdbus_ALLOC_PORT_BY_SYSTEM``
 | ``ON`` : socket number of servers are allocated by the system
 | \*``OFF``: socket number of servers are allocated by name server
``fdbus_SECURITY``
 | ``ON`` : enable security
 | \*``OFF``: disable security
``fdbus_BUILD_JNI``
 | ``ON`` : build JNI shared library and jar package
 | \*``OFF``: don't build JNI artifacts


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

