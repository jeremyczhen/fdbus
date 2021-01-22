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

// (c) Ivan Gagis, 2008
// e-mail: igagis@gmail.com

// Description:
//          cross platfrom C++ Sockets wrapper
//

#include <iostream>
#include "sckt.hpp"

#if !defined(CONFIG_SOCKET_CONNECT_TIMEOUT)
#define CONFIG_SOCKET_CONNECT_TIMEOUT 2000
#endif

#if !defined(CONFIG_SOCKET_LISTEN_BACKLOG)
#define CONFIG_SOCKET_LISTEN_BACKLOG 32
#endif

//system specific defines, typedefs and includes
#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#define snprintf _snprintf

#define M_INVALID_SOCKET INVALID_SOCKET
#define M_SOCKET_ERROR SOCKET_ERROR
#define M_EINTR WSAEINTR
#define M_FD_SETSIZE FD_SETSIZE
#define MSG_NOSIGNAL 0
#define MSG_DONTWAIT 0
typedef SOCKET T_Socket;
typedef int socklen_t;

#else //assume linux/unix
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stddef.h>
//#include <scm_cred.h>

#define M_INVALID_SOCKET (-1)
#define M_SOCKET_ERROR (-1)
#define M_EINTR EINTR
#define M_FD_SETSIZE FD_SETSIZE
typedef int T_Socket;
#define MAXINTERFACES 16

#endif

using namespace sckt;

Library* Library::instance = 0;

inline static T_Socket& CastToSocket(sckt::Socket::SystemIndependentSocketHandle& s){
    M_SCKT_STATIC_ASSERT( sizeof(s) >= sizeof(T_Socket) )
    return *reinterpret_cast<T_Socket*>(&s);
};

inline static const T_Socket& CastToSocket(const sckt::Socket::SystemIndependentSocketHandle& s){
    return CastToSocket(const_cast<sckt::Socket::SystemIndependentSocketHandle&>(s));
};

//static
void Library::InitSockets(){
#ifdef __WIN32__
    WORD versionWanted = MAKEWORD(2,2);
    WSADATA wsaData;
    if(WSAStartup(versionWanted, &wsaData) != 0 )
        throw sckt::Exc("sdlw::InitSockets(): Winsock 2.2 initialization failed");
#else //assume linux/unix
    // SIGPIPE is generated when a remote socket is closed
    void (*handler)(int);
    handler = signal(SIGPIPE, SIG_IGN);
    if(handler != SIG_DFL)
        signal(SIGPIPE, handler);
#endif
};

//static
void Library::DeinitSockets(){
#ifdef __WIN32__
    // Clean up windows networking
    if(WSACleanup() == M_SOCKET_ERROR)
        if(WSAGetLastError() == WSAEINPROGRESS){
            WSACancelBlockingCall();
            WSACleanup();
        }
#else //assume linux/unix
    // Restore the SIGPIPE handler
    void (*handler)(int);
    handler = signal(SIGPIPE, SIG_DFL);
    if(handler != SIG_IGN)
        signal(SIGPIPE, handler);
#endif
};

IPAddress Library::GetHostByName(const char *hostName, u16 port){
    if(!hostName)
        throw sckt::Exc("Sockets::GetHostByName(): pointer passed as argument is 0");
    
    IPAddress addr;
#if defined(CONFIG_HAS_GETHOSTBYNAME)
    addr.host = inet_addr(hostName);
    if(addr.host == INADDR_NONE){
        struct hostent *hp;
        hp = gethostbyname(hostName);
        if(hp)
            memcpy(&(addr.host), hp->h_addr, sizeof(addr.host)/* hp->h_length */);
        else
            throw sckt::Exc("Sockets::GetHostByName(): gethostbyname() failed");
    }
    addr.port = port;
#endif
    return addr;
};

sckt::Exc::Exc(const char* message){
    if(message==0)
        message = "unknown exception";
    
    int len = (int)strlen(message);
    this->msg = new char[len+1];
    memcpy(this->msg, message, len);
    this->msg[len] = 0;//null-terminate
};

sckt::Exc::~Exc(){
    delete[] this->msg;
};

Library::Library(){
    if(Library::instance != 0)
        throw sckt::Exc("Library::Library(): sckt::Library singletone object is already created");
    Library::InitSockets();
    this->instance = this;
};

Library::~Library(){
    //this->instance should not be null here
    Library::DeinitSockets();
    this->instance = 0;
};

Socket::Socket() :
        isReady(false)
{
    CastToSocket(this->socket) = M_INVALID_SOCKET;
};

bool Socket::IsValid()const{
    return CastToSocket(this->socket) != M_INVALID_SOCKET;
};

Socket& Socket::operator=(const Socket& s){
    this->~Socket();
    CastToSocket(this->socket) = CastToSocket(s.socket);
    this->isReady = s.isReady;
    CastToSocket( const_cast<Socket&>(s).socket ) = M_INVALID_SOCKET;//same as std::auto_ptr
    return *this;
};

void Socket::setNonBlock(bool on)
{
    //Set the socket to non-blocking mode for accept()
#if defined(__BEOS__) && defined(SO_NONBLOCK)
    // On BeOS r5 there is O_NONBLOCK but it's for files only
    {
        long b = 1;
        setsockopt(CastToSocket(this->socket), SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
    }
#elif defined(O_NONBLOCK)
    {
        int flags = fcntl(CastToSocket(this->socket), F_GETFL, 0);
        if (on)
        {
            flags |= O_NONBLOCK;
        }
        else
        {
            flags &= ~O_NONBLOCK;
        }
        fcntl(CastToSocket(this->socket), F_SETFL, flags);
    }
#elif defined(__WIN32__)
    {
        u_long mode = on ? 1 : 0;
        ioctlsocket(CastToSocket(this->socket), FIONBIO, &mode);
    }
#elif defined(__OS2__)
    {
        int dontblock = 1;
        ioctl(CastToSocket(this->socket), FIONBIO, &dontblock);
    }
#else
#warning How do we set non-blocking mode on other operating systems?
#endif
}

int Socket::getNativeSocket()const{
    return (int)CastToSocket(this->socket);
}

IPAddress::IPAddress() :
        host(INADDR_ANY),
        port(0)
{};

//static
sckt::u32 IPAddress::ParseString(const char* ip){
    if(!ip || (ip[0] == '\0'))
        return 0;
    if ((ip[0] == '0') && (ip[1] == '\0'))
        return 0;
    
    //subfunctions
    struct sf{
        static void ThrowInvalidIP(){
            throw sckt::Exc("IPAddress::ParseString(): string is not a valid IP address");
        };
    };
    
    u32 h = 0;//parsed host
    const char *curp = ip;
    for(uint t=0; t<4; ++t){
        uint dights[3];
        uint numDgts;
        for(numDgts=0; numDgts<3; ++numDgts){
            if( *curp == '.' || *curp == 0 ){
                if(numDgts==0)
                    sf::ThrowInvalidIP();
                break;
            }else{
                if(*curp < '0' || *curp > '9')
                    sf::ThrowInvalidIP();
                dights[numDgts] = uint(*curp)-uint('0');
            }
            ++curp;
        }
        
        if(t<3 && *curp != '.')//unexpected delimiter or unexpected end of string
            sf::ThrowInvalidIP();
        else if(t==3 && *curp != 0)
            sf::ThrowInvalidIP();
        
        uint xxx=0;
        for(uint i=0; i<numDgts; ++i){
            uint ord = 1;
            for(uint j=1; j<numDgts-i; ++j)
               ord*=10;
            xxx+=dights[i]*ord;
        }
        if(xxx > 255)
            sf::ThrowInvalidIP();
        
        h |= (xxx<<(8*t));
        
        ++curp;
    }
    return h;
};

/* Open a TCP network server socket
   This creates a local server socket on the given port.
*/
void TCPServerSocket::Open(const IPAddress& ip, bool disableNaggle){
    if(this->IsValid())
        throw sckt::Exc("TCPServerSocket::Open(): socket already opened");
    
    this->disableNaggle = disableNaggle;
    
#ifndef __WIN32__
    if (ip.ipc_path.empty())
    {
#endif
        CastToSocket(this->socket) = ::socket(AF_INET, SOCK_STREAM, 0);
#ifndef __WIN32__
    }
    else
    {
        CastToSocket(this->socket) = ::socket(PF_LOCAL, SOCK_STREAM, 0);
    }
#endif
    if(CastToSocket(this->socket) == M_INVALID_SOCKET)
        throw sckt::Exc("TCPServerSocket::Open(): Couldn't create socket");
    
#ifndef __WIN32__
    if (ip.ipc_path.empty())
    {
#endif
        sockaddr_in sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_addr.s_addr = ip.host ? ip.host : INADDR_ANY;
        sockAddr.sin_port = htons(ip.port);

        // allow local address reuse
        {
            int yes = 1;
            setsockopt(CastToSocket(this->socket), SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));
        }

        // Bind the socket for listening
        if( bind(CastToSocket(this->socket), reinterpret_cast<sockaddr*>(&sockAddr), sizeof(sockAddr)) == M_SOCKET_ERROR ){
            this->Close();
            throw sckt::Exc("TCPServerSocket::Open(): Couldn't bind to local port");
        }
#ifndef __WIN32__
    }
    else
    {
        sockaddr_un sockAddr;
        if (ip.ipc_path.length() >= sizeof(sockAddr.sun_path))
        {
            this->Close();
            throw sckt::Exc("TCPServerSocket::Open(): path is too long");
        }
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sun_family = AF_UNIX;
        strcpy(sockAddr.sun_path, ip.ipc_path.c_str());
        socklen_t addr_len;
#ifdef FDB_CONFIG_UDS_ABSTRACT
        sockAddr.sun_path[0] = '\0';
        addr_len = ip.ipc_path.size() + offsetof(struct sockaddr_un, sun_path);
#else
        addr_len = sizeof(sockAddr);
#endif

        // Bind the socket for listening
        unlink(ip.ipc_path.c_str());
        if( bind(CastToSocket(this->socket), reinterpret_cast<sockaddr*>(&sockAddr), addr_len) == M_SOCKET_ERROR ){
            this->Close();
            throw sckt::Exc("TCPServerSocket::Open(): Couldn't bind to local address");
        }
        socket_type = SCKT_SOCKET_UNIX;
    }
#endif

    if( listen(CastToSocket(this->socket), CONFIG_SOCKET_LISTEN_BACKLOG) == M_SOCKET_ERROR ){
        this->Close();
        throw sckt::Exc("TCPServerSocket::Open(): Couldn't listen to local port");
    }

    if (socket_type != SCKT_SOCKET_UNIX)
    {
        struct sockaddr_in sock_addr;
        socklen_t len = sizeof(sock_addr);
        len = sizeof(sock_addr);
        if (getsockname(CastToSocket(socket), (struct sockaddr*)&sock_addr, &len) != -1)
        {
            self_ip = inet_ntoa(sock_addr.sin_addr);
            self_port = ntohs(sock_addr.sin_port);
        }
    }

    setNonBlock();

#if !defined(__WIN32__)
    {
        int flags = fcntl(CastToSocket(this->socket), F_GETFD);
        fcntl(CastToSocket(this->socket), F_SETFD, flags | FD_CLOEXEC);
    }
#endif
};

/* Open a TCP network socket.
   A TCP connection to the remote host and port is attempted.
*/
void TCPSocket::Open(const IPAddress& ip, Options *options, bool disableNaggle){
    if(this->IsValid())
        throw sckt::Exc("TCPSocket::Open(): socket already opened");
    
#ifndef __WIN32__
    if (ip.ipc_path.empty())
    {
#endif
        CastToSocket(this->socket) = ::socket(AF_INET, SOCK_STREAM, 0);
#ifndef __WIN32__
    }
    else
    {
        CastToSocket(this->socket) = ::socket(PF_LOCAL, SOCK_STREAM, 0);
    }
#endif
    if(CastToSocket(this->socket) == M_INVALID_SOCKET)
        throw sckt::Exc("TCPSocket::Open(): Couldn't create socket");

    //Connecting to remote host
#ifndef __WIN32__
    if (ip.ipc_path.empty())
    {
#endif
        sockaddr_in sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_addr.s_addr = ip.host;
        sockAddr.sin_port = htons(ip.port);

        // Connect to the remote host
        if (CONFIG_SOCKET_CONNECT_TIMEOUT)
        {
            struct timeval tv;
            
            tv.tv_sec = CONFIG_SOCKET_CONNECT_TIMEOUT / 1000;
            tv.tv_usec = (CONFIG_SOCKET_CONNECT_TIMEOUT % 1000) * 1000;
            if (setsockopt (CastToSocket(this->socket), SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,
                    sizeof(tv)) < 0)
            
            {
                throw sckt::Exc("TCPServerSocket::Open(): unable to set connect timeout");
            }
        }
        if( connect(CastToSocket(this->socket), reinterpret_cast<sockaddr *>(&sockAddr), sizeof(sockAddr)) == M_SOCKET_ERROR ){
            this->Close();
            throw sckt::Exc("TCPSocket::Open(): Couldn't connect to remote host");
        }
#ifndef __WIN32__
    }
    else
    {
        sockaddr_un sockAddr;
        if (ip.ipc_path.length() >= sizeof(sockAddr.sun_path))
        {
            this->Close();
            throw sckt::Exc("TCPServerSocket::Open(): path is too long");
        }
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sun_family = AF_UNIX;
        strcpy(sockAddr.sun_path, ip.ipc_path.c_str());
        socklen_t addr_len;
#ifdef FDB_CONFIG_UDS_ABSTRACT
        sockAddr.sun_path[0] = '\0';
        addr_len = ip.ipc_path.size() + offsetof(struct sockaddr_un, sun_path);
#else
        addr_len = sizeof(sockAddr);
#endif

        // Bind the socket for listening
        if( connect(CastToSocket(this->socket), reinterpret_cast<sockaddr*>(&sockAddr), addr_len) == M_SOCKET_ERROR ){
            this->Close();
            throw sckt::Exc("TCPServerSocket::Open(): Couldn't bind to local address");
        }
        socket_type = SCKT_SOCKET_UNIX;
    }
#endif
    this->setNonBlock(options ? options->mNonBlock : true);
    
    //Disable Naggle algorithm if required
    if(disableNaggle)
        this->DisableNaggle();
    
    this->isReady = false;

    if (socket_type != SCKT_SOCKET_UNIX)
    {
        struct sockaddr_in sock_addr;
        socklen_t len = sizeof(sock_addr);
        if (getpeername(CastToSocket(socket), (struct sockaddr*)&sock_addr, &len) != -1)
        {
            peer_ip = inet_ntoa(sock_addr.sin_addr);
            peer_port = ntohs(sock_addr.sin_port);
        }
        len = sizeof(sock_addr);
        if (getsockname(CastToSocket(socket), (struct sockaddr*)&sock_addr, &len) != -1)
        {
            self_ip = inet_ntoa(sock_addr.sin_addr);
            self_port = ntohs(sock_addr.sin_port);
        }
    }
    
#if !defined(__WIN32__)
    int flags = fcntl(CastToSocket(this->socket), F_GETFD);
    fcntl(CastToSocket(this->socket), F_SETFD, flags | FD_CLOEXEC);

#ifdef CONFIG_SOCKET_PEERCRED
    if (socket_type == SCKT_SOCKET_UNIX)
    {
        struct ucred ucred;
        socklen_t len = sizeof(struct ucred);
        if (getsockopt(CastToSocket(socket), SOL_SOCKET, SO_PEERCRED, &ucred, &len) != -1)
        {
            pid = ucred.pid;
            gid = ucred.gid;
            uid = ucred.uid;
        }
    }
#endif
#endif
};

void TCPSocket::DisableNaggle(){
    if(!this->IsValid())
        throw sckt::Exc("TCPSocket::DisableNaggle(): socket is not opened");
    
    int yes = 1;
    setsockopt(CastToSocket(this->socket), IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
};

void Socket::Close(){
    if(this->IsValid()){
#ifdef __WIN32__
        //Closing socket in Win32.
        //refer to http://tangentsoft.net/wskfaq/newbie.html#howclose for details
        shutdown(CastToSocket(this->socket), SD_BOTH);
        closesocket(CastToSocket(this->socket));
#else //assume linux/unix
        close(CastToSocket(this->socket));
#endif
    }
    this->isReady = false;
    CastToSocket(this->socket) = M_INVALID_SOCKET;
};


void TCPServerSocket::Accept(TCPSocket &sock, Options *options){
    if(!this->IsValid())
        throw sckt::Exc("TCPServerSocket::Accept(): the socket is not opened");
    
    this->isReady = false;
    
    sockaddr_in sockAddr;
    
#ifdef __WIN32__
    int sock_alen = sizeof(sockAddr);
#else //linux/unix
    socklen_t sock_alen = sizeof(sockAddr);
#endif
    
    CastToSocket(sock.socket) = accept(CastToSocket(this->socket), reinterpret_cast<sockaddr*>(&sockAddr),
#ifdef USE_GUSI_SOCKETS
                (unsigned int *)&sock_alen);
#else
                &sock_alen);
#endif
    
    if(CastToSocket(sock.socket) == M_INVALID_SOCKET)
        return;//no connections to be accepted, return invalid socket
    
    sock.socket_type = socket_type;
    if (socket_type != SCKT_SOCKET_UNIX)
    {
        sock.setNonBlock(false);
        struct sockaddr_in sock_addr;
        socklen_t len = sizeof(sock_addr);
        if (getpeername(CastToSocket(sock.socket), (struct sockaddr*)&sock_addr, &len) != -1)
        {
            sock.peer_ip = inet_ntoa(sock_addr.sin_addr);
            sock.peer_port = ntohs(sock_addr.sin_port);
        }
        len = sizeof(sock_addr);
        if (getsockname(CastToSocket(sock.socket), (struct sockaddr*)&sock_addr, &len) != -1)
        {
            sock.self_ip = inet_ntoa(sock_addr.sin_addr);
            sock.self_port = ntohs(sock_addr.sin_port);
        }
    }

#if !defined(__WIN32__)
    {
        int flags = fcntl(CastToSocket(sock.socket), F_GETFD);
        fcntl(CastToSocket(sock.socket), F_SETFD, flags | FD_CLOEXEC);
    }

#ifdef CONFIG_SOCKET_PEERCRED
    if (socket_type == SCKT_SOCKET_UNIX)
    {
        int optval = 1;
        setsockopt(CastToSocket(sock.socket), SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval));

        struct ucred ucred;
        socklen_t len = sizeof(struct ucred);
        if (getsockopt(CastToSocket(sock.socket), SOL_SOCKET, SO_PEERCRED, &ucred, &len) != -1)
        {
            sock.pid = ucred.pid;
            sock.gid = ucred.gid;
            sock.uid = ucred.uid;
        }
    }
#endif
#endif
    sock.setNonBlock(options ? options->mNonBlock : true);
    
    if(this->disableNaggle)
        sock.DisableNaggle();
};


sckt::uint TCPSocket::Send(const sckt::byte* data, uint size){
    if(!this->IsValid())
        throw sckt::Exc("TCPSocket::Send(): socket is not opened");
    
    int sent = 0,
        left = int(size);
    
    //Keep sending data until it's sent or an error occurs
    int errorCode = 0;

    int res;
    do{
        res = (int)send(CastToSocket(this->socket), reinterpret_cast<const char*>(data), left, MSG_NOSIGNAL/* | MSG_DONTWAIT*/);
        if(res == M_SOCKET_ERROR){
#ifdef __WIN32__
            errorCode = WSAGetLastError();
            if (errorCode == WSAETIMEDOUT)
            {
                res = 0;
                sent = 0;
                break;
            }
#else //linux/unix
            errorCode = errno;
            if ((errorCode == EAGAIN) || (errorCode == EWOULDBLOCK)) {
                res = 0;
                break;
            }
#endif
        }else{
            errorCode = 0;
            sent += res;
            left -= res;
            data += res;
        }
    }while( (left > 0) && ((res != M_SOCKET_ERROR) || (errorCode == M_EINTR)) );
    
    if(res == M_SOCKET_ERROR)
        throw sckt::Exc("TCPSocket::Send(): send() failed");
    
    return uint(sent);
};


sckt::uint TCPSocket::Recv(sckt::byte* buf, uint maxSize){
    //this flag shall be cleared even if this function fails to avoid subsequent
    //calls to Recv() because it indicates that there's activity.
    //So, do it at the beginning of the function.
    this->isReady = false;
    
    if(!this->IsValid())
        throw sckt::Exc("TCPSocket::Send(): socket is not opened");
    
    int len;
    int errorCode = 0;
    
    do{
        len = (int)recv(CastToSocket(this->socket), reinterpret_cast<char *>(buf), maxSize, 0/*MSG_DONTWAIT*/);
        if (!len)
        {
            len = M_SOCKET_ERROR;
            break;
        }
        if(len == M_SOCKET_ERROR){
#ifdef __WIN32__
            errorCode = WSAGetLastError();
#else //linux/unix
            errorCode = errno;
            if ((errorCode == EAGAIN) || (errorCode == EWOULDBLOCK)) {
                len = 0;
                break;
            }
#endif
        }
        else{
            errorCode = 0;
        }
    }while(errorCode == M_EINTR);
    
    if(len == M_SOCKET_ERROR)
        throw sckt::Exc("TCPSocket::Recv(): recv() failed");
    
    return uint(len);
};

void UDPSocket::Open(u16 port){
    if(this->IsValid())
        throw sckt::Exc("UDPSocket::Open(): the socket is already opened");
    
    CastToSocket(this->socket) = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(CastToSocket(this->socket) == M_INVALID_SOCKET)
	throw sckt::Exc("UDPSocket::Open(): ::socket() failed");
    
    /* Bind locally, if appropriate */
    if(port != 0){
        struct sockaddr_in sockAddr;
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = AF_INET;
        sockAddr.sin_addr.s_addr = INADDR_ANY;
        sockAddr.sin_port = htons(port);
        
        // Bind the socket for listening
        if(bind(CastToSocket(this->socket), reinterpret_cast<struct sockaddr*>(&sockAddr), sizeof(sockAddr)) == M_SOCKET_ERROR){
            this->Close();
            throw sckt::Exc("UDPSocket::Open(): could not bind to local port");
        }
    }
#ifdef SO_BROADCAST
    //Allow LAN broadcasts with the socket
    {
        int yes = 1;
        setsockopt(CastToSocket(this->socket), SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
    }
#endif
    
    this->isReady = false;
};

sckt::uint UDPSocket::Send(const sckt::byte* buf, u16 size, IPAddress destinationIP){
    sockaddr_in sockAddr;
    int sockLen = sizeof(sockAddr);
    
    sockAddr.sin_addr.s_addr = destinationIP.host;
    sockAddr.sin_port = htons(destinationIP.port);
    sockAddr.sin_family = AF_INET;
    int res = (int)sendto(CastToSocket(this->socket), reinterpret_cast<const char*>(buf), size, 0, reinterpret_cast<struct sockaddr*>(&sockAddr), sockLen);
    
    if(res == M_SOCKET_ERROR)
        throw sckt::Exc("UDPSocket::Send(): sendto() failed");
    
    return res;
};

sckt::uint UDPSocket::Recv(sckt::byte* buf, u16 maxSize, IPAddress &out_SenderIP){
    sockaddr_in sockAddr;
    
#ifdef __WIN32__
    int sockLen = sizeof(sockAddr);
#else //linux/unix
    socklen_t sockLen = sizeof(sockAddr);
#endif
    
    int res = (int)recvfrom(CastToSocket(this->socket), reinterpret_cast<char*>(buf), maxSize, 0, reinterpret_cast<sockaddr*>(&sockAddr), &sockLen);
    
    if(res == M_SOCKET_ERROR)
        throw sckt::Exc("UDPSocket::Recv(): recvfrom() failed");
    
    out_SenderIP.host = ntohl(sockAddr.sin_addr.s_addr);
    out_SenderIP.port = ntohs(sockAddr.sin_port);
    return res;
};

SocketSet::SocketSet(uint maxNumSocks):
        maxSockets(maxNumSocks),
        numSockets(0)
{
    if(this->maxSockets > M_FD_SETSIZE)
        throw sckt::Exc("SocketSet::SocketSet(): socket size reuqested is too large");
    this->set = new Socket*[this->maxSockets];
};

void SocketSet::AddSocket(Socket *sock){
    if(!sock)
        throw sckt::Exc("SocketSet::AddSocket(): null socket pointer passed as argument");
    
    if(this->numSockets == this->maxSockets)
        throw sckt::Exc("SocketSet::AddSocket(): socket set is full");
    
    for(uint i=0; i<this->numSockets; ++i){
        if(this->set[i] == sock)
            return;
    }
    
    this->set[this->numSockets] = sock;
    ++this->numSockets;
};

void SocketSet::RemoveSocket(Socket *sock){
    if(!sock)
        throw sckt::Exc("SocketSet::RemoveSocket(): null socket pointer passed as argument");
    
    uint i;
    for(i=0; i<this->numSockets; ++i)
        if(this->set[i]==sock)
            break;
    
    if(i==this->numSockets)
        return;//socket sock not found in the set

    --this->numSockets;//decrease numsockets before shifting the sockets
    //shift sockets
    for(;i<this->numSockets; ++i){
        this->set[i] = this->set[i+1];
    }
};

bool SocketSet::CheckSockets(uint timeoutMillis){
    if(this->numSockets == 0)
        return false;
    
    T_Socket maxfd = 0;
    
    //Find the largest file descriptor
    for(uint i = 0; i<this->numSockets; ++i){
        if(CastToSocket(this->set[i]->socket) > maxfd)
            maxfd = CastToSocket(this->set[i]->socket);
    }
    
    int retval;
    fd_set readMask;
    
    //Check the file descriptors for available data
    int errorCode = 0;
    do{
        //Set up the mask of file descriptors
        FD_ZERO(&readMask);
        for(uint i=0; i<this->numSockets; ++i){
            T_Socket socketHnd = CastToSocket(this->set[i]->socket);
            FD_SET(socketHnd, &readMask);
        }
        
        // Set up the timeout
        //TODO: consider moving this out of do{}while() loop
        timeval tv;
        tv.tv_sec = timeoutMillis/1000;
        tv.tv_usec = (timeoutMillis%1000)*1000;
        
        retval = select(maxfd+1, &readMask, NULL, NULL, &tv);
        if(retval == M_SOCKET_ERROR){
#ifdef __WIN32__
            errorCode = WSAGetLastError();
#else
            errorCode = errno;
#endif
        }
        else {
            errorCode = 0;
        }
    }while(errorCode == M_EINTR);
    
    // Mark all file descriptors ready that have data available
    if(retval != 0 && retval != M_SOCKET_ERROR){
        int numSocketsReady = 0;
        for(uint i=0; i<this->numSockets; ++i){
            T_Socket socketHnd = CastToSocket(this->set[i]->socket);
            if( (FD_ISSET(socketHnd, &readMask)) ){
                this->set[i]->isReady = true;
                ++numSocketsReady;
            }
        }
        
        //on Win32 when compiling with mingw there are some strange things,
        //sometimes retval is not zero but there is no any sockets marked as ready in readMask.
        //I do not know why this happens on win32 and mingw. The workaround is to calculate number
        //of active sockets mnually, ignoring the retval value.
        if(numSocketsReady > 0)
            return true;
    }
    return false;
};

void sckt::getIpAddress(std::map<std::string, std::string> &addr_tbl)
{
#ifdef __WIN32__
    char ac[80];
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
        throw sckt::Exc("Error when getting local host name.");
    }

    struct hostent *phe = gethostbyname(ac);
    if (phe == 0) {
        throw sckt::Exc("Yow! Bad host lookup.");
    }

    for (int i = 0; phe->h_addr_list[i] != 0; ++i) {
        struct in_addr addr;
        char if_name[16];
        snprintf(if_name, sizeof(if_name), "%d", i);
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        addr_tbl[if_name] = inet_ntoa(addr);
    }
#else
    int sock_fd;
    struct ifreq buf[MAXINTERFACES];
    struct ifconf ifc;
    int interface_num;
    char *addr;//[ADDR_LEN];

    if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        throw sckt::Exc("Create socket failed");
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_req = buf;
    if(ioctl(sock_fd, SIOCGIFCONF, (char *)&ifc) < 0)
    {
        close(sock_fd);
        throw sckt::Exc("Get a list of interface addresses failed");
    }
    
    interface_num = (int)(ifc.ifc_len / sizeof(struct ifreq));

    while(interface_num--)
    {
        if(ioctl(sock_fd, SIOCGIFFLAGS, (char *)&buf[interface_num]) < 0)
        {
            printf("Get the active flag word of the device");
            continue;
        }
        if(buf[interface_num].ifr_flags & IFF_PROMISC)
            printf("Interface is in promiscuous mode\n");

        if(!(buf[interface_num].ifr_flags & IFF_UP))
        {
            // interface is not up.
            continue;
        }

        if(ioctl(sock_fd, SIOCGIFADDR, (char *)&buf[interface_num]) < 0)
        {
            // fail to get address
            continue;
        }
        addr = inet_ntoa(((struct sockaddr_in*)(&buf[interface_num].ifr_addr))->sin_addr);
        addr_tbl[buf[interface_num].ifr_name] = addr;
    }

    close(sock_fd);
#endif
}
