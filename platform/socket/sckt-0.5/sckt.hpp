/* The MIT License:

Copyright (c) 2008 Ivan Gagis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

// (c) Ivan Gagis 2008
// e-mail: igagis@gmail.com

// Description:
//          cross platfrom C++ Sockets wrapper
//

/**
@file sckt.hpp
@brief Main header file of the library.
This is the main header file of sckt. You need to include it to use sckt library.
*/

#ifndef M_SCKT_HPP
#define M_SCKT_HPP

#if defined(M_BUILD_DLL) & defined(__WIN32__)
#define M_DECLSPEC __declspec(dllexport)
#else
#define M_DECLSPEC
#endif

#ifdef __WIN32__
#pragma  warning( disable : 4290) 
#endif

#include <exception>
#include <new>
#include <string>
#include <map>


/**
@brief the main namespace of sckt library.
All the declarations of sckt library are made inside this namespace.
*/
namespace sckt{
enum SocketType
{
    SCKT_SOCKET_INET,
    SCKT_SOCKET_UNIX,
};

struct Options
{
    bool mNonBlock;
    int32_t mKAInterval;
    int32_t mKARetries;
    Options(bool non_block = true, int32_t ka_interval = 0, int32_t ka_retries = 0)
        : mNonBlock(non_block)
        , mKAInterval(ka_interval)
        , mKARetries(ka_retries)
    {}
};

//=================
//= Static Assert =
//=================
#ifndef DOC_DONT_EXTRACT //direction to doxygen not to extract this class
template <bool b> struct StaticAssert{
    virtual void STATIC_ASSERTION_FAILED()=0;
    virtual ~StaticAssert(){};
};
template <> struct StaticAssert<true>{};
#define M_SCKT_STATIC_ASSERT_II(x, l) struct StaticAssertInst_##l{ \
    sckt::StaticAssert<x> STATIC_ASSERTION_FAILED;};
#define M_SCKT_STATIC_ASSERT_I(x, l) M_SCKT_STATIC_ASSERT_II(x, l)
#define M_SCKT_STATIC_ASSERT(x)  M_SCKT_STATIC_ASSERT_I(x, __LINE__)
#endif //~DOC_DONT_EXTRACT
//= ~Static Assert=

//some typedefs, these are system specific, so maybe there will appear some #ifdef s in the future, for different systems.
typedef unsigned int uint;///< platform native unsigned integer
typedef unsigned long long int u64;///< 64 bit unsigned integer
typedef unsigned int u32;///< 32 bit unsigned integer
typedef unsigned short u16;///< 16 bit unsigned integer
typedef unsigned char byte;///< 8bit unsigned integer

M_SCKT_STATIC_ASSERT( sizeof(u64)==8 )
M_SCKT_STATIC_ASSERT( sizeof(u32)==4 )
M_SCKT_STATIC_ASSERT( sizeof(u16)==2 )
M_SCKT_STATIC_ASSERT( sizeof(byte)==1 )

//forward declarations
class SocketSet;
class IPAddress;

/**
@brief Basic exception class.
This is a basic exception class of the library. All other exception classes are derived from it.
*/
class M_DECLSPEC Exc : public std::exception{
    char *msg;
public:
    /**
    @brief Exception constructor.
    @param message Pointer to the exception message null-terminated string. Constructor will copy the string into objects internal memory buffer.
    */
    Exc(const char* message = 0);
    virtual ~Exc();
    
    /**
    @brief Returns a pointer to exception message.
    @return a pointer to objects internal memory buffer holding the exception message null-terminated string.
            Note, that after the exception object is destroyed the pointer returned by this method become invalid.
    */
    const char *What()const{
        return this->msg?this->msg:("sckt::Exc(): Unknown exception");
    };
    
private:  
    //override from std::exception
    const char *what() const noexcept {
        return this->What();
    };
};

void getIpAddress(std::map<std::string, std::string> &addr_tbl);

/**
@brief a structure which holds IP address
*/
class M_DECLSPEC IPAddress{
public:
    u32 host;///< IP address
    u16 port;///< IP port number
#ifndef __WIN32__
    std::string ipc_path;
#endif
    
    inline IPAddress();
    
    /**
    @brief Create IP address specifying exact ip address and port number.
    @param h - IP address. For example, 0x100007f represents "127.0.0.1" IP address value.
    @param p - IP port number.
    */
    inline IPAddress(u32 h, u16 p) :
            host(h),
            port(p)
    {};
    
    /**
    @brief Create IP address specifying exact ip address as 4 bytes and port number.
    The ip adress can be specified as 4 separate byte values, for example:
    @code
    sckt::IPAddress ip(127, 0, 0, 1, 80); //"127.0.0.1" port 80
    @endcode
    @param h1 - 1st triplet of IP address.
    @param h2 - 2nd triplet of IP address.
    @param h3 - 3rd triplet of IP address.
    @param h4 - 4th triplet of IP address.
    @param p - IP port number.
    */
    inline IPAddress(byte h1, byte h2, byte h3, byte h4, u16 p) :
            host(u32(h1) + (u32(h2)<<8) + (u32(h3)<<16) + (u32(h4)<<24)),
            port(p)
    {};
    
    /**
    @brief Create IP address specifying ip address as string and port number.
    @param ip - IP address null-terminated string. Example: "127.0.0.1".
    @param p - IP port number.
    */
    inline IPAddress(const char* ip, u16 p) :
            host(IPAddress::ParseString(ip)),
            port(p)
    {};

#ifndef __WIN32__
    inline IPAddress(const char *path)
        : host(0)
        , port(0)
        , ipc_path(path)
    {
    }
#endif
    
    /**
    @brief compares two IP addresses for equality.
    @param ip - IP address to compare with.
    @return true if hosts and ports of the two IP addresses are equal accordingly.
    @return false otherwise.
    */
    inline bool operator==(const IPAddress& ip){
        return (this->host == ip.host) && (this->port == ip.port)
#ifndef __WIN32__
                && !ipc_path.compare(ip.ipc_path)
#endif
            ;
    }
private:
    //parse IP address from string
    static u32 ParseString(const char* ip);
};

/**
@brief Library singletone class.
This is a library singletone class. Creating an object of this class initializes the library
while destroying this object deinitializes it. So, the convenient way of initializing the library
is to create an object of this class on the stack. Thus, when the object goes out of scope its
destructor will be called and the library will be deinitialized automatically.
This is what C++ RAII is all about ;-).
*/
//singletone
class M_DECLSPEC Library{
    static Library *instance;
public:
    Library();
    ~Library();
    
    /**
    @brief Get reference to singletone object.
    This static method returns a reference to Sockets singletone object.
    If the object was not created before then this function will throw sckt::Exc.
    @return reference to Sockets singletone object.
    */
    static Library& Inst(){
        if(!Library::instance)
            throw sckt::Exc("Sockets::Inst(): singletone sckt::Library object is not created");
        return *Library::instance;
    };
    
    /**
    @brief Resolve host IP by its name.
    This function resolves host IP address by its name. If it fails resolving the IP address it will throw sckt::Exc.
    @param hostName - null-terminated string representing host name. Example: "www.somedomain.com".
    @param port - IP port number which will be placed in the resulting IPAddress structure.
    @return filled IPAddress structure.
    */
    IPAddress GetHostByName(const char *hostName, u16 port);
private:
    static void InitSockets();
    static void DeinitSockets();
};

/**
@brief Basic socket class.
This is a base class for all socket types such as TCP sockets or UDP sockets.
*/
class M_DECLSPEC Socket{
    friend class SocketSet;
    
public:
    //this type will hold system specific socket handle.
    //this buffer should be large enough to hold socket handle in different systems.
    //sizeof(u64) looks enough so far (need to work on 64-bit platforms too).
#ifndef DOC_DONT_EXTRACT //direction to doxygen not to extract this class
    struct SystemIndependentSocketHandle{
        sckt::byte buffer[sizeof(u64)];
    };
#endif//~DOC_DONT_EXTRACT

    int getNativeSocket() const;

protected:
    bool isReady;
    
    SystemIndependentSocketHandle socket;
    
    Socket();
    
    Socket& operator=(const Socket& s);
    void setNonBlock(bool on = true);
    
public:
    virtual ~Socket(){
        this->Close();
    };
    
    /**
    @brief Tells whether the socket is opened or not.
    TODO: write some detailed description.
    @return Returns true if the socket is opened or false otherwise.
    */
    bool IsValid()const;
    
    /**
    @brief Tells whether there is some activity on the socket or not.
    See sckt::SocketSet class description for more info on how to use this method properly.
    @return
        - true if there is some data to read on the socket or if the remote socket has disconnected.
            The latter case can be detected by checking if subsequent Recv() method returns 0.
        - false if there is no any activity on the socket.
    */
    inline bool IsReady()const{
        return this->isReady;
    };
    
    /**
    @brief Closes the socket disconnecting it if necessary.
    */
    void Close();
};

/**
@brief a class which represents a TCP socket.
*/
class M_DECLSPEC TCPSocket : public Socket{
    friend class TCPServerSocket;
public:
    /**
    @brief Constructs an invalid TCP socket object.
    */
    TCPSocket()
        : pid(0)
        , gid(0)
        , uid(0)
        , peer_port(0)
        , self_port(0)
        , socket_type(SCKT_SOCKET_INET)
    {
    };
    
    /**
    @brief A copy constructor.
    Copy constructor creates a new socket object which refers to the same socket as s.
    After constructor completes the s becomes invalid.
    In other words, the behavior of copy constructor is similar to one of std::auto_ptr class from standard C++ library.
    @param s - other TCP socket to make a copy from.
    */
    //copy constructor
    TCPSocket(const TCPSocket& s)
    {
        //NOTE: that operator= calls destructor, so this->socket should be invalid, base class constructor takes care about it.
        this->operator=(s);//same as auto_ptr
    };
    
    /**
    @brief A constructor which automatically calls sckt::TCPSocket::Open() method.
    This constructor creates a socket and calls its sckt::TCPSocket::Open() method.
    So, it creates an already opened socket.
    @param ip - IP address to 'connect to/listen on'.
    @param disableNaggle - enable/disable Naggle algorithm.
    */
    TCPSocket(const IPAddress& ip, Options *options = 0, bool disableNaggle = false)
        : pid(0)
        , gid(0)
        , uid(0)
        , peer_port(0)
        , self_port(0)
        , socket_type(SCKT_SOCKET_INET)
    {
        this->Open(ip, options, disableNaggle);
    };
    
    /**
    @brief Assignment operator, works similar to std::auto_ptr::operator=().
    After this assignment operator completes this socket object refers to the socket the s objejct referred, s become invalid.
    It works similar to std::auto_ptr::operator=() from standard C++ library.
    @param s - socket to assign from.
    */
    TCPSocket& operator=(const TCPSocket& s){
        this->Socket::operator=(s);
        return *this;
    };
    
    /**
    @brief Connects the socket.
    This method connects the socket to remote TCP server socket.
    @param ip - IP address.
    @param disableNaggle - enable/disable Naggle algorithm.
    */
    void Open(const IPAddress& ip, Options *options = 0, bool disableNaggle = false);
    
    /**
    @brief Send data to connected socket.
    Sends data on connected socket. This method blocks until all data is completely sent.
    @param data - pointer to the buffer with data to send.
    @param size - number of bytes to send.
    @return the number of bytes sent. Note that this value should normally be equal to the size argument value.
    */
    uint Send(const byte* data, uint size);
    
    /**
    @brief Receive data from connected socket.
    Receives data available on the socket.
    If there is no data available this function blocks until some data arrives.
    @param buf - pointer to the buffer where to put received data.
    @param maxSize - maximal number of bytes which can be put to the buffer.
    @return if returned value is not 0 then it represents the number of bytes written to the buffer.
    @return 0 returned value indicates disconnection of remote socket.
    */
    //returns 0 if connection was closed by peer
    uint Recv(byte* buf, uint maxSize);

    /* credentials */
    u32 pid;
    u32 gid;
    u32 uid;
    std::string peer_ip;
    u16 peer_port;
    std::string self_ip;
    u16 self_port;
    SocketType socket_type;
    
private:
    void DisableNaggle();
};

/**
@brief a class which represents a TCP server socket.
TCP server socket is the socket which can listen for new connections
and accept them creating an ordinary TCP socket for it.
*/
class M_DECLSPEC TCPServerSocket : public Socket{
    bool disableNaggle;//this flag indicates if accepted sockets should be created with disabled Naggle
    SocketType socket_type;
public:
    /**
    @brief Creates an invalid (unopened) TCP server socket.
    */
    TCPServerSocket() :
            disableNaggle(false)
          , socket_type(SCKT_SOCKET_INET)
          , self_port(0)
    {};
    
    /**
    @brief A copy constructor.
    Copy constructor creates a new socket object which refers to the same socket as s.
    After constructor completes the s becomes invalid.
    In other words, the behavior of copy constructor is similar to one of std::auto_ptr class from standard C++ library.
    @param s - other TCP socket to make a copy from.
    */
    //copy constructor
    TCPServerSocket(const TCPServerSocket& s) :
            disableNaggle(s.disableNaggle)
          , socket_type(s.socket_type)
          , self_port(0)
    {
        //NOTE: that operator= calls destructor, so this->socket should be invalid, base class constructor takes care about it.
        this->operator=(s);//same as auto_ptr
    };
    
    /**
    @brief Assignment operator, works similar to std::auto_ptr::operator=().
    After this assignment operator completes this socket object refers to the socket the s objejct referred, s become invalid.
    It works similar to std::auto_ptr::operator=() from standard C++ library.
    @param s - socket to assign from.
    */
    TCPServerSocket& operator=(const TCPServerSocket& s){
        this->Socket::operator=(s);
        return *this;
    };
    
    /**
    @brief A constructor which automatically calls sckt::TCPServerSocket::Open() method.
    This constructor creates a socket and calls its sckt::TCPServerSocket::Open() method.
    So, it creates an already opened socket listening on the specified port.
    @param port - IP port number to listen on.
    @param disableNaggle - enable/disable Naggle algorithm for all accepted connections.
    */
    TCPServerSocket(const IPAddress& ip, bool disableNaggle = false) :
          socket_type(SCKT_SOCKET_INET)
        , self_port(0)
    {
        this->Open(ip, disableNaggle);
    };
    
    /**
    @brief Connects the socket or starts listening on it.
    This method starts listening on the socket for incoming connections.
    @param port - IP port number to listen on.
    @param disableNaggle - enable/disable Naggle algorithm for all accepted connections.
    */
    void Open(const IPAddress& ip, bool disableNaggle = false);
    
    /**
    @brief Accepts one of the pending connections, non-blocking.
    Accepts one of the pending connections and returns a TCP socket object which represents
    either a valid connected socket or an invalid socket object.
    This function does not block if there is no any pending connections, it just returns invalid
    socket object in this case. One can periodically check for incoming connections by calling this method.
    @return sckt::TCPSocket object. One can later check if the returned socket object
        is valid or not by calling sckt::Socket::IsValid() method on that object.
        - if the socket is valid then it is a newly connected socket, further it can be used to send or receive data.
        - if the socket is invalid then there was no any connections pending, so no connection was accepted.
    */
    void Accept(TCPSocket &sock, Options *options);
    std::string self_ip;
    u16 self_port;
};

class M_DECLSPEC UDPSocket : public Socket{
public:
    UDPSocket(){};
    
    ~UDPSocket(){
        this->Close();
    };
    
    /**
    @brief Open the socket.
    This mthod opens the socket, this socket can further be used to send or receive data.
    After the socket is opened it becomes a valid socket and Socket::IsValid() will return true for such socket.
    After the socket is closed it becomes invalid.
    In other words, a valid socket is an opened socket.
    In case of errors this method throws sckt::Exc.
    @param port - IP port number on which the socket will listen for incoming datagrams.
        This is useful for server-side sockets, for client-side sockets use UDPSocket::Open().
    */
    void Open(u16 port);
    
    
    inline void Open(){
        this->Open(0);
    };
    
    //returns number of bytes sent, should be less or equal to size.
    uint Send(const byte* buf, u16 size, IPAddress destinationIP);
    
    //returns number of bytes received, 0 if connection was gracefully closed (???).
    uint Recv(byte* buf, u16 maxSize, IPAddress &out_SenderIP);
};


/**
@brief Socket set class for checking multiple sockets for activity.
This class represents a set of sockets which can be checked for any activity
such as incoming data received or remote socket has disconnected.
Note, that the socket set holds only references to socket objects, so it is up to you
to make sure that all the socket objects you add to a particular socket set will not be
destroyed without prior removing them from socket set.
*/
class M_DECLSPEC SocketSet{
    Socket** set;
    uint maxSockets;
    uint numSockets;
  public:
    
    /**
    @brief Creates a socket set of the specified size.
    Creates a socket set which can hold the specified number of sockets at maximum.
    @param maxNumSocks - maximum number of sockets this socket set can hold.
    */
    SocketSet(uint maxNumSocks);
    
    /**
    @brief Destroys the socket set.
    Note, that it does not destroy the sockets this set holds references to.
    */
    ~SocketSet(){
        delete[] set;
    };
    
    /**
    @brief Returns number of sockets the socket set currently holds.
    @return number of sockets the socket set currently holds.
    */
    inline uint NumSockets()const{return this->numSockets;};
    
    /**
    @brief Returns maximal number of sockets this set can hold.
    @return maximal number of sockets this set can hold.
    */
    inline uint MaxSockets()const{return this->maxSockets;};
    
    /**
    @brief Add a socket to socket set.
    @param sock - pointer to the socket object to add.
    */
    void AddSocket(Socket *sock);
    
    /**
    @brief Remove socket from socket set.
    @param sock - pointer to socket object which we want to remove from the set.
    */
    void RemoveSocket(Socket *sock);
    
    /**
    @brief Check sokets from socket set for activity.
    This method checks sockets for activities of incoming data ready or remote socket has disconnected.
    This method sets ready flag for all sockets with activity which can later be checked by
    sckt::Socket::IsReady() method. The ready flag will be cleared by subsequent sckt::TCPSocket::Recv() function call.
    @param timeoutMillis - maximum number of milliseconds to wait for socket activity to appear.
        if 0 is specified the function will not wait and will return immediately.
    @return true if there is at least one socket with activity.
    @return false if there are no any sockets with activities.
    */
    //This function checks to see if data is available for reading on the
    //given set of sockets.  If 'timeout' is 0, it performs a quick poll,
    //otherwise the function returns when either data is available for
    //reading, or the timeout in milliseconds has elapsed, which ever occurs
    //first.  This function returns true if there are any sockets ready for reading,
    //or false if there was an error with the select() system call.
    bool CheckSockets(uint timeoutMillis);
};

};//~namespace sckt
#endif//~once


/**

@mainpage sckt library

@section sec_about About
<b>sckt</b> is a simple cross platfrom C++ wrapper above sockets networking API designed for games.

@section sec_getting_started Getting started
@ref page_usage_tutorial "library usage tutorial" - quickstart tutorial
*/

/**
@page page_usage_tutorial sckt usage tutorial

TODO: write usage tutorial

*/
