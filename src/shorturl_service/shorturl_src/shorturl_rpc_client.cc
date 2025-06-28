#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <json/json.h>
#include "shorturl_rpc.h"
#include "shorturl_rpc.pb.h"

#define JSON_ENABLE 1
#define PROTOBUF_ENABLE 0

// 发送请求并接收响应
std::string sendRequest(const std::string& requestStr) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[4096] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        return "";
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(50051);//端口号

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return "";
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return "";
    }

    send(sock, requestStr.c_str(), requestStr.length(), 0);

    memset(buffer, 0, sizeof(buffer));
    ssize_t bytesRead = read(sock, buffer, sizeof(buffer));
    std::string responseStr;
    if (bytesRead > 0) {
        responseStr = std::string(buffer, bytesRead);
    }

    close(sock);
    return responseStr;
}

// 转换长链为短链
std::string convertToShortUrl(const std::string& fullUrl) {
#if JSON_ENABLE
    Json::Value request;
    request["method"] = "ConvertToShortUrl";
    request["params"]["full_url"] = fullUrl;

    Json::FastWriter writer;
    std::string requestStr = writer.write(request);
#elif PROTOBUF_ENABLE
    shorturl_rpc::ConvertToShortUrlRequest request;
    request.set_full_url(fullUrl);
    std::string requestStr;
    request.SerializeToString(&requestStr);
#endif

    std::string responseStr = sendRequest(requestStr);

#if JSON_ENABLE
    Json::Value response;
    Json::Reader reader;
    if (reader.parse(responseStr, response)) {
        if (response["code"].asInt() == 0) {
            return response["urlmd5"].asString();
        }
    }
    return "";
#elif PROTOBUF_ENABLE
    shorturl_rpc::ConvertToShortUrlResponse response;
    if (response.ParseFromString(responseStr)) {
        if (response.code() == 0) {
            return response.urlmd5();
        }
    }
    return "";
#endif
}

// 解析短链为长链
std::string resolveShortUrl(const std::string& shortUrl) {
#if JSON_ENABLE
    Json::Value request;
    request["method"] = "ResolveShortUrl";
    request["params"]["short_url"] = shortUrl;
    
    Json::FastWriter writer;
    std::string requestStr = writer.write(request);
#elif PROTOBUF_ENABLE
    shorturl_rpc::ResolveShortUrlRequest request;
    request.set_short_url(shortUrl);
    std::string requestStr;
    request.SerializeToString(&requestStr);
#endif

    std::string responseStr = sendRequest(requestStr);

#if JSON_ENABLE
    Json::Value response;
    Json::Reader reader;
    if (reader.parse(responseStr, response)) {
        if (response["code"].asInt() == 0) {
            return response["full_url"].asString();
        }
    }
    return "";
#elif PROTOBUF_ENABLE
    shorturl_rpc::ResolveShortUrlResponse response;
    if (response.ParseFromString(responseStr)) {
        if (response.code() == 0) {
            return response.full_url();
        }
    }
    return "";
#endif
}