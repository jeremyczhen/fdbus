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
=================
- ``Linux``
- ``Windows``
- ``QNX``

Dependence
===========
- cmake - 3.1.3 or above for non-jni build
- cmake - 3.11.1 or above for jni build
- protocol buffer
- compiler supporting C++11 (gcc 4.7+ for Linux; Visual Studio for Windows)

Download
===========
https://github.com/jeremyczhen/fdbus.git

Documentation & Blog
=====================
https://blog.csdn.net/jeremy_cz/article/details/89060291

How to build
================
It is recommended to build with buildCentral.
For more details please find help for `FDK <https://github.com/jeremyczhen/manifest>`_

cmake options
=============
The following options can be specified with ``-Dfdbus_XXX=ON/OFF`` when running ``cmake``.
   The status with ``*`` is set as default.

``fdbus_BUILD_TESTS``
 | \* ``ON`` : build examples
 | ``OFF``: don't build examples
``fdbus_ENABLE_LOG``
 | \*``ON`` : enable log output of fdbus lib
 | ``OFF``: disable log output of fdbus lib
``fdbus_LOG_TO_STDOUT``
 | ``ON`` : send fdbus log to stdout (terminal)
 | \* ``OFF``: fdbus log is sent to log server
``fdbus_ENABLE_MESSAGE_METADATA``
 | \* ``ON`` : time stamp is included in fdbus message to track delay of message during request-reply interaction
 | ``OFF``: time stamp is disabled
``fdbus_SOCKET_BLOCKING_CONNECT``
 | ``ON`` : socket method connect() will be blocked forever if server is not ready to accept
 | \* ``OFF``: connect() will be blocked with timer to avoid permanent blocking
``fdbus_SOCKET_ENABLE_PEERCRED``
 | \* ``ON`` : peercred of UDS (Unix Domain Socket) is enabled
 | ``OFF``: peercred of UDS is disabled
``fdbus_ALLOC_PORT_BY_SYSTEM``
 | ``ON`` : socket number of servers are allocated by the system
 | \* ``OFF``: socket number of servers are allocated by name server
``fdbus_SECURITY``
 | ``ON`` : enable security
 | \* ``OFF``: disable security
``fdbus_BUILD_JNI``
 | ``ON`` : build JNI shared library and jar package
 | \* ``OFF``: don't build JNI artifacts


The following options can be specified with 
   ``-DMACRO_DEF='VARIABLE=value;VARIABLE=value'``

``FDB_CFG_SOCKET_PATH``
 | specify directory of UDS file
 | default: /tmp

``CONFIG_SOCKET_CONNECT_TIMEOUT``
 | specify timeout of connect() when connect to socket server in ms. 
 | "``0``" means block forever.
 | default: 2000

