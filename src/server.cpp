#include "axe/server.hpp"
#include "./log.hpp"
#include "axe/listen.hpp"
#include "axe/request.hpp"
#include "axe/socket_address.hpp"
#include "unistd.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <signal.h>
#include <string>

namespace axe {

#define BUFFERLEN 300

bool Server::isExitHandlerSet = false;

std::vector<int> Server::fds = {};

void Server::exitHandler(int) {
  std::cout << "\n";
  LOG("Caught exit signal. Disconnecting sockets for all servers...")
  for (auto f : Server::fds) {
    shutdown(f, SHUT_RDWR);
  }
  exit(0);
}

Server::Server() : fd(socket(AF_INET, SOCK_STREAM, 0)) {
  Server::fds.push_back(fd);
  if (!isExitHandlerSet) {
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = exitHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    isExitHandlerSet = true;
  }
}

void Server::listen(const int port, const char *addr, bool isIPv6) {
  if (fd != 0) {
    if (isIPv6) {
      DLOG("Is IPv6")
      in6_addr inAddr;
      inet_pton(AF_INET6, addr, &inAddr);
      sockaddr_in6 v6;
      v6.sin6_addr = inAddr;
      v6.sin6_port = htons(port);
      v6.sin6_family = AF_INET6;
      sockaddress = new SocketAddress(v6);
    } else {
      DLOG("Is IPv4")
      in_addr inAddr;
      inet_pton(AF_INET, addr, &inAddr);
      sockaddr_in v4;
      v4.sin_addr = inAddr;
      v4.sin_port = htons(port);
      v4.sin_family = AF_INET;
      sockaddress = new SocketAddress(v4);
    }
    DLOG("Got socket address")
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) == 0) {
      DLOG("Socket option set successfully")
      sockaddr *addrPtr = nullptr;
      socklen_t addrLen = 0;
      if (sockaddress->isV4()) {
        addrPtr = (sockaddr *)&sockaddress->getIPv4();
        addrLen = sizeof(sockaddr_in);
      } else {
        addrPtr = (sockaddr *)&sockaddress->getIPv6();
        addrLen = sizeof(sockaddr_in6);
      }
      if (bind(fd, addrPtr, addrLen) >= 0) {
        DLOG("Binding successful")
        int newFD = 0;
        sockaddr *acceptAddr = nullptr;
        socklen_t *acceptAddrLen = nullptr;
        if (socketListen(fd, 3) >= 0) {
          DLOG("Listening successful")
          while (true) {
            if ((newFD = accept(fd, acceptAddr, acceptAddrLen)) >= 0) {
              std::string message;
              char buffer[BUFFERLEN];
              DLOG("Buffer allocated")
              auto readRes = 1;
              while ((readRes != 0) && (readRes != -1)) {
                DLOG("Reading...")
                readRes = read(newFD, buffer, BUFFERLEN);
                DLOG("Read result is " << readRes)
                if (readRes != -1) {
                  message += buffer;
                  std::fill(std::begin(buffer), std::end(buffer), '\0');
                  if (readRes == 0) {
                    break;
                  } else if (readRes == BUFFERLEN) {
                    continue;
                  } else {
                    break;
                  }
                } else {
                  break;
                }
              }
              auto req = Request(fd, message);
              close(newFD);
              DLOG("Communication file descriptor closed succesfully")
            } else {
              perror("Accepting connection failed");
              exit(EXIT_FAILURE);
            }
          }
          shutdown(fd, SHUT_RDWR);
        } else {
          perror("Binding socket failed");
          exit(EXIT_FAILURE);
        }
      } else {
        perror("Listening on port failed");
        exit(EXIT_FAILURE);
      }
    } else {
      perror("Setting socket option failed");
      exit(EXIT_FAILURE);
    }
  } else {
    perror("Getting file descriptor for socket failed");
    exit(EXIT_FAILURE);
  }
}

} // namespace axe