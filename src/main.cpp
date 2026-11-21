#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>

#include "http/HttpParser.hpp"
#include "storage/Storage.hpp"
#include "server/Router.hpp"


volatile sig_atomic_t stopServer = 0;

void stopHandler(int) { stopServer = 1; }

bool waitForData(int fd) {
  pollfd pfd{fd, POLLIN, 0};
  while (!stopServer) {
    int result = poll(&pfd, 1, 500);
    if (result > 0)
      return (pfd.revents & POLLIN) != 0;
    if (result < 0 && errno != EINTR)
      return false;
  }
  return false;
}

bool sendAll(int fd, const std::string &data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0)
      return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}
void handleClient(int client, Router& router) {
    std::string buffer;
    char chunk[4096];

    while (!stopServer) {
        auto parsed = HttpParser::parse(buffer);

        if (parsed.status == ParseStatus::incomplete) {
            if (!waitForData(client)) {
                return;
            }

            ssize_t n = recv(client, chunk, sizeof(chunk), 0);

            if (n <= 0) {
                return;
            }

            buffer.append(chunk, static_cast<std::size_t>(n));
            continue;
        }

        if (parsed.status == ParseStatus::bad_request) {
            sendAll(
                client,
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n"
            );
            return;
        }

        HttpRequest request = parsed.request;
        buffer.erase(0, parsed.consumed);

        std::string response = router.handle(request);

        if (!sendAll(client, response) || !request.keepAlive) {
            return;
        }
    }
}


int main(int argc, char *argv[]) {
  int port = 8080;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--port" || arg == "--threads") && i + 1 < argc) {
      int value = std::stoi(argv[++i]);
      if (arg == "--port")
        port = value; 
    } else {
      std::cerr << "Usage: server [--port PORT] [--threads N]\n";
      return 1;
    }
  }

  std::signal(SIGINT, stopHandler);
  std::signal(SIGTERM, stopHandler);

  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    std::cerr << "socket error\n";
    return 1;
  }

  int reuse = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(static_cast<unsigned short>(port));

  if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) <
          0 ||
      listen(server, 10) < 0) {
    std::cerr << "bind/listen error\n";
    close(server);
    return 1;
  }

  std::cout << "Server listening on port " << port << '\n';

  Storage storage;
  Router router(storage);

  while (!stopServer) {
    if (!waitForData(server))
      continue;

    int client = accept(server, nullptr, nullptr);
    if (client >= 0) {
      handleClient(client, router);
      close(client);
    }
  }

  close(server);
  std::cout << "Server stopped\n";
  return 0;
}
