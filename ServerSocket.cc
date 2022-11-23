/*
 * Copyright ©2022 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2022 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  struct addrinfo hints;

 memset(&hints, 0, sizeof(struct addrinfo));

  // check if ai_family is valid
  if (ai_family != AF_INET && ai_family != AF_INET6 && ai_family != AF_UNSPEC)
    return false;

  // fill in the fields of "hints" for getaddrinfo()
  hints.ai_family = ai_family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // convert port_ to C-string
  struct addrinfo *result;
  std::string port = std::to_string(port_);

  // get a list of address structures via "result"
  int res = getaddrinfo(NULL, port.c_str(), &hints, &result);
  if (res != 0) {
    return false;
  }

  int ret_fd = -1;
  for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
    ret_fd = socket(rp->ai_family,
                    rp->ai_socktype,
                    rp->ai_protocol);

    if (ret_fd == -1) {
      continue;
    }

    // Configure the socket
    int optval = 1;
    Verify333(setsockopt(ret_fd, SOL_SOCKET, SO_REUSEADDR,
                         &optval, sizeof(optval)) == 0);

    // Binding the socket to the address and port number returned by getaddrinfo()
    if (bind(ret_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      sock_family_ = rp->ai_family;
      break;
    }

    // The bind failed, try the next address/port returned by getaddrinfo()
    close(ret_fd);
    ret_fd = -1;
  }

  freeaddrinfo(result);

  // If we failed to bind, return false
  if (ret_fd == -1)
    return false;

  // Success. Tell the OS that we want this to be a listening socket
  if (listen(ret_fd, SOMAXCONN) != 0) {
    close(ret_fd);
    return false;
  }

  listen_sock_fd_ = ret_fd;
  *listen_fd = ret_fd;

  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          std::string* const client_addr,
                          uint16_t* const client_port,
                          std::string* const client_dns_name,
                          std::string* const server_addr,
                          std::string* const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  struct sockaddr *addr = reinterpret_cast<struct sockaddr *>(&caddr);
  int client_fd = -1;
  while (1) {
    client_fd = accept(listen_sock_fd_,
                       addr,
                       &caddr_len);

    if (client_fd < 0) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;

      return false;
    }
    break;
  }

  if (client_fd < 0)
    return false;

  // store client file descriptor in output parameter
  *accepted_fd = client_fd;

  // get client IP address and port and store them
  if (addr->sa_family == AF_INET) {  // client uses IPv4 address
    char str[INET_ADDRSTRLEN];
    struct sockaddr_in *in4 = reinterpret_cast<struct sockaddr_in *>(addr);
    inet_ntop(AF_INET, &(in4->sin_addr), str, INET_ADDRSTRLEN);

    *client_addr = std::string(str);
    *client_port = htons(in4->sin_port);
  } else {  // client uses IPv6 address
    char str[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *in6 = reinterpret_cast<struct sockaddr_in6 *>(addr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), str, INET6_ADDRSTRLEN);
    *client_addr = std::string(str);
    *client_port = htons(in6->sin6_port);
  }

  // get client DNS name and store it
  char hname[1024];
  Verify333(getnameinfo(addr, caddr_len, hname, 1024, NULL, 0, 0) == 0);
  *client_dns_name = std::string(hname);

  // get the client IP address/DNS name and store them
  char host_name[1024];
  host_name[0] = '\0';
  if (sock_family_ == AF_INET) {  // server use IPv4 address
    struct sockaddr_in server;
    socklen_t server_len = sizeof(server);
    char addr_buf[INET_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &server, &server_len);
    inet_ntop(AF_INET, &server.sin_addr, addr_buf, INET_ADDRSTRLEN);
    getnameinfo((const struct sockaddr *) &server,
                server_len, host_name, 1024, NULL, 0, 0);

    *server_addr = std::string(addr_buf);
    *server_dns_name = std::string(host_name);
  } else {  // server uses IPv6 address
    struct sockaddr_in6 server;
    socklen_t server_len = sizeof(server);
    char addrbuf[INET6_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &server, &server_len);
    inet_ntop(AF_INET6, &server.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
    getnameinfo((const struct sockaddr *) &server,
                server_len, host_name, 1024, NULL, 0, 0);

    *server_addr = std::string(addrbuf);
    *server_dns_name = std::string(host_name);
  }

  return true;
}

}  // namespace hw4
