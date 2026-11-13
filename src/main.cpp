#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <csignal>

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
    ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
    if (n <= 0)
      return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

std::string lower(std::string value) {
  for (char &c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

bool parseRequest(const std::string &data, std::string &method,
                  std::string &path, bool &keepAlive,
                  std::size_t &requestSize) {
  std::size_t end = data.find("\r\n\r\n");
  if (end == std::string::npos)
    return false;

  std::size_t lineEnd = data.find("\r\n");
  if (lineEnd == std::string::npos)
    return false;

  std::string line = data.substr(0, lineEnd);
  std::size_t first = line.find(' ');
  std::size_t second =
      line.find(' ', first == std::string::npos ? first : first + 1);
  if (first == std::string::npos || second == std::string::npos) {
    requestSize = 0;
    return true;
  }

  method = line.substr(0, first);
  path = line.substr(first + 1, second - first - 1);
  std::string version = line.substr(second + 1);
  keepAlive = (version == "HTTP/1.1");

  std::size_t bodySize = 0;
  std::size_t pos = lineEnd + 2;
  while (pos < end) {
    std::size_t next = data.find("\r\n", pos);
    if (next == std::string::npos || next > end)
      break;
    std::string header = lower(data.substr(pos, next - pos));
    if (header.rfind("content-length:", 0) == 0) {
      try {
        bodySize = static_cast<std::size_t>(std::stoul(header.substr(15)));
      } catch (...) {
        requestSize = 0;
        return true;
      }
    }
    if (header.rfind("connection:", 0) == 0) {
      if (header.find("close") != std::string::npos)
        keepAlive = false;
      if (header.find("keep-alive") != std::string::npos)
        keepAlive = true;
    }
    pos = next + 2;
  }

  requestSize = end + 4 + bodySize;
  return data.size() >= requestSize;
}

void handleClient(int client) {
  std::string buffer;
  char chunk[4096];

  while (!stopServer) {
    std::string method, path;
    bool keepAlive = true;
    std::size_t requestSize = 0;

    while (!parseRequest(buffer, method, path, keepAlive, requestSize)) {
      if (!waitForData(client))
        return;
      ssize_t n = recv(client, chunk, sizeof(chunk), 0);
      if (n <= 0)
        return;
      buffer.append(chunk, static_cast<std::size_t>(n));
    }

    if (requestSize == 0) {
      sendAll(client, "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                      "0\r\nConnection: close\r\n\r\n");
      return;
    }

    buffer.erase(0, requestSize);
    std::size_t query = path.find('?');
    if (query != std::string::npos)
      path.resize(query);

    std::string response;
    if (method == "GET" && path == "/health") {
      response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
      response += "Content-Length: 15\r\n";
      response += keepAlive ? "Connection: keep-alive\r\n\r\n"
                            : "Connection: close\r\n\r\n";
      response += "{\"status\":\"ok\"}";
    } else {
      response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n";
      response += keepAlive ? "Connection: keep-alive\r\n\r\n"
                            : "Connection: close\r\n\r\n";
    }

    if (!sendAll(client, response) || !keepAlive)
      return;
  }
}

int main(int argc, char *argv[]) {
  int port = 8080;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if ((arg == "--port" || arg == "--threads") && i + 1 < argc) {
      int value = std::stoi(argv[++i]);
      if (arg == "--port")
        port = value; // --threads пока не используется.
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

  while (!stopServer) {
    if (!waitForData(server))
      continue;

    int client = accept(server, nullptr, nullptr);
    if (client >= 0) {
      handleClient(client);
      close(client);
    }
  }

  close(server);
  std::cout << "Server stopped\n";
  return 0;
}
