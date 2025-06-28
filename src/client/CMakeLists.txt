
# 源文件列表：test.cc + 短链客户端实现文件
set(CLIENT_SRCS
    shorturl_rpc_client.cc                  # 客户端主逻辑（sendRequest、convertToShortUrl 等）
    shorturl_rpc.pb.cc                       # Protobuf 生成的序列化/反序列化代码
)

# 头文件路径（根据实际项目结构调整）
include_directories(
    /usr/include/jsoncpp                    # JSON 头文件（Ubuntu 默认路径）
    /usr/include/uuid                       # UUID 头文件（generateUUID 依赖）
)
# 生成可执行文件并链接依赖库
add_executable(shorturl_client ${CLIENT_SRCS})

TARGET_LINK_LIBRARIES(shorturl_client jsoncpp protobuf uuid)