#ifndef SHORT_RPC_H
#define SHORT_RPC_H

#include <string>
#include <uuid/uuid.h>
#include <memory>

void startRpcServer();

std::string convertToShortUrl(const std::string& fullUrl);
std::string resolveShortUrl(const std::string& shortUrl);
std::string handleRequest(const std::string& requestStr);


template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) +
                1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}
#endif // SHORT_RPC_H