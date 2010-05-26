#include <node_net2.h>
#include <v8.h>

#include <node.h>
#include <node_buffer.h>

#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> /* inet_pton */

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/ioctl.h>

#ifdef __linux__
# include <linux/sockios.h> /* For the SIOCINQ / FIONREAD ioctl */
#endif
/* Non-linux platforms like OS X define this ioctl elsewhere */
#ifndef FIONREAD
#include <sys/filio.h>
#endif

#include <errno.h>


namespace node {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;

static Persistent<String> fd_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;
static Persistent<String> type_symbol;
static Persistent<String> tcp_symbol;
static Persistent<String> unix_symbol;

static Persistent<FunctionTemplate> recv_msg_template;


#define FD_ARG(a)                                        \
  int fd;                                                \
  if (!(a)->IsInt32() || (fd = (a)->Int32Value()) < 0) { \
    return ThrowException(Exception::TypeError(          \
          String::New("Bad file descriptor argument"))); \
  }



static inline bool SetCloseOnExec(int fd) {
  return (fcntl(fd, F_SETFD, FD_CLOEXEC) != -1);
}


static inline bool SetNonBlock(int fd) {
  return (fcntl(fd, F_SETFL, O_NONBLOCK) != -1);
}


static inline bool SetSockFlags(int fd) {
  return SetNonBlock(fd) && SetCloseOnExec(fd);
}


// Creates nonblocking pipe
static Handle<Value> Pipe(const Arguments& args) {
  HandleScope scope;
  int fds[2];

  if (pipe(fds) < 0) return ThrowException(ErrnoException(errno, "pipe"));

  if (!SetSockFlags(fds[0]) || !SetSockFlags(fds[1])) {
    int fcntl_errno = errno;
    close(fds[0]);
    close(fds[1]);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Array> a = Array::New(2);
  a->Set(Integer::New(0), Integer::New(fds[0]));
  a->Set(Integer::New(1), Integer::New(fds[1]));
  return scope.Close(a);
}


// Creates nonblocking socket pair
static Handle<Value> SocketPair(const Arguments& args) {
  HandleScope scope;

  int fds[2];

  // XXX support SOCK_DGRAM?
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    return ThrowException(ErrnoException(errno, "socketpair"));
  }

  if (!SetSockFlags(fds[0]) || !SetSockFlags(fds[1])) {
    int fcntl_errno = errno;
    close(fds[0]);
    close(fds[1]);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Array> a = Array::New(2);
  a->Set(Integer::New(0), Integer::New(fds[0]));
  a->Set(Integer::New(1), Integer::New(fds[1]));
  return scope.Close(a);
}


// Creates a new non-blocking socket fd
// t.socket("TCP");
// t.socket("UNIX");
// t.socket("UDP");
static Handle<Value> Socket(const Arguments& args) {
  HandleScope scope;

  // default to TCP
  int domain = PF_INET;
  int type = SOCK_STREAM;

  if (args[0]->IsString()) {
    String::Utf8Value t(args[0]->ToString());
    // FIXME optimize this cascade.
    if (0 == strcasecmp(*t, "TCP")) {
      domain = PF_INET;
      type = SOCK_STREAM;
    } else if (0 == strcasecmp(*t, "TCP4")) {
      domain = PF_INET;
      type = SOCK_STREAM;
    } else if (0 == strcasecmp(*t, "TCP6")) {
      domain = PF_INET6;
      type = SOCK_STREAM;
    } else if (0 == strcasecmp(*t, "UNIX")) {
      domain = PF_UNIX;
      type = SOCK_STREAM;
    } else if (0 == strcasecmp(*t, "UDP")) {
      domain = PF_INET6;
      type = SOCK_DGRAM;
    } else {
      return ThrowException(Exception::Error(
            String::New("Unknown socket type.")));
    }
  }

  int fd = socket(domain, type, 0);

  if (fd < 0) return ThrowException(ErrnoException(errno, "socket"));

  if (!SetSockFlags(fd)) {
    int fcntl_errno = errno;
    close(fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  return scope.Close(Integer::New(fd));
}


// NOT AT ALL THREAD SAFE - but that's okay for node.js
// (yes this is all to avoid one small heap alloc)
static struct sockaddr *addr;
static socklen_t addrlen;
static inline Handle<Value> ParseAddressArgs(Handle<Value> first,
                                             Handle<Value> second,
                                             bool is_bind) {
  static struct sockaddr_un un;
  static struct sockaddr_in in;
  static struct sockaddr_in6 in6;

  if (first->IsString() && !second->IsString()) {
    // UNIX
    String::Utf8Value path(first->ToString());

    if (path.length() > sizeof un.sun_path) {
      return Exception::Error(String::New("Socket path too long"));
    }

    memset(&un, 0, sizeof un);
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, *path);

    addr = (struct sockaddr*)&un;
    addrlen = path.length() + sizeof(un.sun_family) + 1;

  } else {
    // TCP or UDP
    memset(&in, 0, sizeof in);
    memset(&in6, 0, sizeof in6);

    int port = first->Int32Value();
    in.sin_port = in6.sin6_port = htons(port);  
    in.sin_family = AF_INET;
    in6.sin6_family = AF_INET6;

    bool is_ipv4 = true;

    if (!second->IsString()) {
      in.sin_addr.s_addr = htonl(is_bind ? INADDR_ANY : INADDR_LOOPBACK);
      in6.sin6_addr = is_bind ? in6addr_any : in6addr_loopback;
    } else {
      String::Utf8Value ip(second->ToString());

      if (inet_pton(AF_INET, *ip, &(in.sin_addr)) <= 0) {
        is_ipv4 = false;
        if (inet_pton(AF_INET6, *ip, &(in6.sin6_addr)) <= 0) {
          return ErrnoException(errno, "inet_pton", "Invalid IP Address");
        }
      }
    }

    addr = is_ipv4 ? (struct sockaddr*)&in : (struct sockaddr*)&in6;
    addrlen = is_ipv4 ? sizeof in : sizeof in6;
  }
  return Handle<Value>();
}


// Bind with UNIX 
//   t.bind(fd, "/tmp/socket")
// Bind with TCP
//   t.bind(fd, 80, "192.168.11.2")
//   t.bind(fd, 80)
static Handle<Value> Bind(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Must have at least two args")));
  }

  FD_ARG(args[0])

  Handle<Value> error = ParseAddressArgs(args[1], args[2], true);
  if (!error.IsEmpty()) return ThrowException(error);

  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

  int r = bind(fd, addr, addrlen);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "bind"));
  }

  return Undefined();
}


static Handle<Value> Close(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  if (0 > close(fd)) {
    return ThrowException(ErrnoException(errno, "close"));
  }

  return Undefined();
}


// t.shutdown(fd, "read");      -- SHUT_RD
// t.shutdown(fd, "write");     -- SHUT_WR
// t.shutdown(fd, "readwrite"); -- SHUT_RDWR
// second arg defaults to "write".
static Handle<Value> Shutdown(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int how = SHUT_WR;

  if (args[1]->IsString()) {
    String::Utf8Value t(args[1]->ToString());
    if (0 == strcasecmp(*t, "write")) {
      how = SHUT_WR;
    } else if (0 == strcasecmp(*t, "read")) {
      how = SHUT_RD;
    } else if (0 == strcasecmp(*t, "readwrite")) {
      how = SHUT_RDWR;
    } else {
      return ThrowException(Exception::Error(String::New(
            "Unknown shutdown method. (Use 'read', 'write', or 'readwrite'.)")));
    }
  }

  if (0 > shutdown(fd, how)) {
    return ThrowException(ErrnoException(errno, "shutdown"));
  }

  return Undefined();
}


// Connect with unix
//   t.connect(fd, "/tmp/socket")
//
// Connect with TCP or UDP
//   t.connect(fd, 80, "192.168.11.2")
//   t.connect(fd, 80, "::1")
//   t.connect(fd, 80)
//  the third argument defaults to "::1"
static Handle<Value> Connect(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Must have at least two args")));
  }

  FD_ARG(args[0])

  Handle<Value> error = ParseAddressArgs(args[1], args[2], false);
  if (!error.IsEmpty()) return ThrowException(error);

  int r = connect(fd, addr, addrlen);

  if (r < 0 && errno != EINPROGRESS) {
    return ThrowException(ErrnoException(errno, "connect"));
  }

  return Undefined();
}


#define ADDRESS_TO_JS(info, address_storage) \
do { \
  char ip[INET6_ADDRSTRLEN]; \
  int port; \
  struct sockaddr_in *a4; \
  struct sockaddr_in6 *a6; \
  switch ((address_storage).ss_family) { \
    case AF_INET6: \
      a6 = (struct sockaddr_in6*)&(address_storage); \
      inet_ntop(AF_INET6, &(a6->sin6_addr), ip, INET6_ADDRSTRLEN); \
      port = ntohs(a6->sin6_port); \
      (info)->Set(address_symbol, String::New(ip)); \
      (info)->Set(port_symbol, Integer::New(port)); \
      break; \
    case AF_INET: \
      a4 = (struct sockaddr_in*)&(address_storage); \
      inet_ntop(AF_INET, &(a4->sin_addr), ip, INET6_ADDRSTRLEN); \
      port = ntohs(a4->sin_port); \
      (info)->Set(address_symbol, String::New(ip)); \
      (info)->Set(port_symbol, Integer::New(port)); \
      break; \
  } \
} while (0)


static Handle<Value> GetSockName(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  struct sockaddr_storage address_storage;
  socklen_t len = sizeof(struct sockaddr_storage);

  int r = getsockname(fd, (struct sockaddr *) &address_storage, &len);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "getsockname"));
  }

  Local<Object> info = Object::New();

  ADDRESS_TO_JS(info, address_storage);

  return scope.Close(info);
}


static Handle<Value> GetPeerName(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  struct sockaddr_storage address_storage;
  socklen_t len = sizeof(struct sockaddr_storage);

  int r = getpeername(fd, (struct sockaddr *) &address_storage, &len);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "getsockname"));
  }

  Local<Object> info = Object::New();

  ADDRESS_TO_JS(info, address_storage);

  return scope.Close(info);
}


static Handle<Value> Listen(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])
  int backlog = args[1]->IsInt32() ? args[1]->Int32Value() : 128;

  if (0 > listen(fd, backlog)) {
    return ThrowException(ErrnoException(errno, "listen"));
  }


  return Undefined();
}


// var peerInfo = t.accept(server_fd);
//
//   peerInfo.fd
//   peerInfo.address
//   peerInfo.port
//
// Returns a new nonblocking socket fd. If the listen queue is empty the
// function returns null (wait for server_fd to become readable and try
// again)
static Handle<Value> Accept(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  struct sockaddr_storage address_storage;
  socklen_t len = sizeof(struct sockaddr_storage);

  int peer_fd = accept(fd, (struct sockaddr*) &address_storage, &len);

  if (peer_fd < 0) {
    if (errno == EAGAIN) return scope.Close(Null());
    return ThrowException(ErrnoException(errno, "accept"));
  }

  if (!SetSockFlags(peer_fd)) {
    int fcntl_errno = errno;
    close(peer_fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Object> peer_info = Object::New();

  peer_info->Set(fd_symbol, Integer::New(peer_fd));

  ADDRESS_TO_JS(peer_info, address_storage);

  return scope.Close(peer_info);
}


static Handle<Value> SocketError(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int error;
  socklen_t len = sizeof(int);
  int r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "getsockopt"));
  }

  return scope.Close(Integer::New(error)); 
}


//  var bytesRead = t.read(fd, buffer, offset, length);
//  returns null on EAGAIN or EINTR, raises an exception on all other errors
//  returns 0 on EOF.
static Handle<Value> Read(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 4 parameters")));
  }

  FD_ARG(args[0])

  if (!Buffer::HasInstance(args[1])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[1]->ToObject());

  size_t off = args[2]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[3]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  ssize_t bytes_read = read(fd, (char*)buffer->data() + off, len);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "read"));
  }

  return scope.Close(Integer::New(bytes_read));
}


// bytesRead = t.recvMsg(fd, buffer, offset, length)
// if (recvMsg.fd) {
//   receivedFd = recvMsg.fd;
// }
static Handle<Value> RecvMsg(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 4 parameters")));
  }

  FD_ARG(args[0])

  if (!Buffer::HasInstance(args[1])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[1]->ToObject());

  size_t off = args[2]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[3]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int received_fd;

  struct iovec iov[1];
  iov[0].iov_base = (char*)buffer->data() + off;
  iov[0].iov_len = len;

  struct msghdr msg;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  /* Set up to receive a descriptor even if one isn't in the message */
  char cmsg_space[64]; // should be big enough
  msg.msg_controllen = 64;
  msg.msg_control = (void *) cmsg_space;

  ssize_t bytes_read = recvmsg(fd, &msg, 0);

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "recvMsg"));
  }

  // Why not return a two element array here [bytesRead, fd]? Because
  // creating an object for each recvmsg() action is heavy. Instead we just
  // assign the recved fd to a globalally accessable variable (recvMsg.fd)
  // that the wrapper can pick up. Since we're single threaded, this is not
  // a problem - just make sure to copy out that variable before the next
  // call to recvmsg().

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg && cmsg->cmsg_type == SCM_RIGHTS) {
    received_fd = *(int *) CMSG_DATA(cmsg);
    recv_msg_template->GetFunction()->Set(fd_symbol, Integer::New(received_fd));
  } else {
    recv_msg_template->GetFunction()->Set(fd_symbol, Null());
  }

  return scope.Close(Integer::New(bytes_read));
}


//  var bytesWritten = t.write(fd, buffer, offset, length);
//  returns null on EAGAIN or EINTR, raises an exception on all other errors
static Handle<Value> Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 4 parameters")));
  }

  FD_ARG(args[0])

  if (!Buffer::HasInstance(args[1])) { 
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[1]->ToObject());

  size_t off = args[2]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[3]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  ssize_t written = write(fd, (char*)buffer->data() + off, len);

  if (written < 0) {
    if (errno == EAGAIN || errno == EINTR) {
      return scope.Close(Integer::New(0));
    }
    return ThrowException(ErrnoException(errno, "write"));
  }

  return scope.Close(Integer::New(written));
}


// var bytesWritten = t.sendFD(self.fd)
//  returns null on EAGAIN or EINTR, raises an exception on all other errors
static Handle<Value> SendFD(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 2 parameters")));
  }

  FD_ARG(args[0])

  // TODO: make sure fd is a unix domain socket?

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::TypeError(
          String::New("FD to send is not an integer")));
  }

  int fd_to_send = args[1]->Int32Value();

  struct msghdr msg;
  struct iovec iov[1];
  char control_msg[CMSG_SPACE(sizeof(fd_to_send))];
  struct cmsghdr *cmsg;
  static char dummy = 'd'; // Need to send at least a byte of data in the message

  iov[0].iov_base = &dummy;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_flags = 0;
  msg.msg_control = (void *) control_msg;
  msg.msg_controllen = CMSG_LEN(sizeof(fd_to_send));
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = msg.msg_controllen;
  *(int*) CMSG_DATA(cmsg) = fd_to_send;

  ssize_t written = sendmsg(fd, &msg, 0);

  if (written < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "sendmsg"));
  }

  /* Note that the FD isn't explicitly closed here, this
   * happens in the JS */

  return scope.Close(Integer::New(written));
}


// Probably only works for Linux TCP sockets?
// Returns the amount of data on the read queue.
static Handle<Value> ToRead(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int value;
  int r = ioctl(fd, FIONREAD, &value);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "ioctl"));
  }

  return scope.Close(Integer::New(value));
}


static Handle<Value> SetNoDelay(const Arguments& args) {
  int flags, r;
  HandleScope scope;

  FD_ARG(args[0])

  flags = args[1]->IsFalse() ? 0 : 1;
  r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&flags, sizeof(flags));

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "setsockopt"));
  }
  return Undefined();
}


static Handle<Value> SetKeepAlive(const Arguments& args) {
  int r;
  HandleScope scope;

  bool enable = false;
  int time = 0;

  FD_ARG(args[0])

  if (args.Length() > 0) enable = args[1]->IsTrue();
  if (enable == true) {
    time = args[2]->Int32Value();
  }

  int flags = enable ? 1 : 0;
  r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flags, sizeof(flags));
  if ((time > 0)&&(r >= 0)) {
#if defined(__APPLE__)
    r = setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, (void *)&time, sizeof(time));
#elif defined(__linux__)
    r = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&time, sizeof(time));
#else
    // Solaris nor FreeBSD support TCP_KEEPIDLE, so do nothing here.
#endif
  }
  if (r < 0) {
    return ThrowException(ErrnoException(errno, "setsockopt"));
  }
  return Undefined();
}


static Handle<Value> IsIP(const Arguments& args) {
  HandleScope scope;
  
  if (!args[0]->IsString()) {
    return scope.Close(Integer::New(4));
  }

  String::Utf8Value s(args[0]->ToString());

  // avoiding buffer overflows in the following strcat
  // 2001:0db8:85a3:08d3:1319:8a2e:0370:7334
  // 39 = max ipv6 address.
  if (s.length() > INET6_ADDRSTRLEN) {
    return scope.Close(Integer::New(0));
  }

  struct sockaddr_in6 a;

  if (inet_pton(AF_INET, *s, &(a.sin6_addr)) > 0) return scope.Close(Integer::New(4));
  if (inet_pton(AF_INET6, *s, &(a.sin6_addr)) > 0) return scope.Close(Integer::New(6));

  return scope.Close(Integer::New(0));
}


static Handle<Value> CreateErrnoException(const Arguments& args) {
  HandleScope scope;

  int errorno = args[0]->Int32Value();
  String::Utf8Value syscall(args[1]->ToString());

  Local<Value> exception = ErrnoException(errorno, *syscall); 

  return scope.Close(exception);
}


void InitNet2(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "write", Write);
  NODE_SET_METHOD(target, "read", Read);

  NODE_SET_METHOD(target, "sendFD", SendFD);

  recv_msg_template =
      Persistent<FunctionTemplate>::New(FunctionTemplate::New(RecvMsg));
  target->Set(String::NewSymbol("recvMsg"), recv_msg_template->GetFunction());

  NODE_SET_METHOD(target, "socket", Socket);
  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "shutdown", Shutdown);
  NODE_SET_METHOD(target, "pipe", Pipe);
  NODE_SET_METHOD(target, "socketpair", SocketPair);

  NODE_SET_METHOD(target, "connect", Connect);
  NODE_SET_METHOD(target, "bind", Bind);
  NODE_SET_METHOD(target, "listen", Listen);
  NODE_SET_METHOD(target, "accept", Accept);
  NODE_SET_METHOD(target, "socketError", SocketError);
  NODE_SET_METHOD(target, "toRead", ToRead);
  NODE_SET_METHOD(target, "setNoDelay", SetNoDelay);
  NODE_SET_METHOD(target, "setKeepAlive", SetKeepAlive);
  NODE_SET_METHOD(target, "getsockname", GetSockName);
  NODE_SET_METHOD(target, "getpeername", GetPeerName);
  NODE_SET_METHOD(target, "isIP", IsIP);
  NODE_SET_METHOD(target, "errnoException", CreateErrnoException);

  target->Set(String::NewSymbol("ENOENT"), Integer::New(ENOENT));
  target->Set(String::NewSymbol("EMFILE"), Integer::New(EMFILE));
  target->Set(String::NewSymbol("EINPROGRESS"), Integer::New(EINPROGRESS));
  target->Set(String::NewSymbol("EINTR"), Integer::New(EINTR));
  target->Set(String::NewSymbol("EACCES"), Integer::New(EACCES));
  target->Set(String::NewSymbol("EPERM"), Integer::New(EPERM));
  target->Set(String::NewSymbol("EADDRINUSE"), Integer::New(EADDRINUSE));
  target->Set(String::NewSymbol("ECONNREFUSED"), Integer::New(ECONNREFUSED));

  errno_symbol          = NODE_PSYMBOL("errno");
  syscall_symbol        = NODE_PSYMBOL("syscall");
  fd_symbol             = NODE_PSYMBOL("fd");
  address_symbol        = NODE_PSYMBOL("address");
  port_symbol           = NODE_PSYMBOL("port");
}

}  // namespace node
