# shorturl_service

#功能
通过rpc接口实现原始长链转短链和获取长链，提供短链分享和短链访问服务


#编译方法

cd src

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Debug ..

make

#运行

sudo ./bin/shorturl_server ./shorturl_server.conf

#需要注意：

把配置文件shorturl_service.conf拷贝到build目录下
