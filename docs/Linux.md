# 环境配置、编译与运行

## 基础构建工具
sudo apt update
sudo apt install -y build-essential cmake git pkg-config

## 中间件
sudo apt install -y mysql-server redis-server

## gRPC/Protobuf 编译工具链（需用源码编译或用 apt 版本）
sudo apt install -y protobuf-compiler-grpc libprotobuf-dev libgrpc++-dev

## 构建
cd Servers
mkdir -p build && cd build
cmake ..
make -j$(nproc)

## 安装VerifyServer依赖
cd VerifyServer && npm install

## 启动与配置数据库和缓存
sudo systemctl start mysql redis-server
mysql -u root -p < /home/admin/MyChatServer/Servers/docs/init.sql
mysql -u root -p < /home/admin/MyChatServer/Servers/docs/seed.sql

## 启动服务器
npm run serve

cd /home/admin/MyChatServer/Servers/build/StatusServer
./StatusServer

cd /home/admin/MyChatServer/Servers/build/ResourceServer
./ResourceServer

cd /home/admin/MyChatServer/Servers/build/ChatServer
./ChatServer -S chatserver1
./ChatServer -S chatserver2

cd /home/admin/MyChatServer/Servers/build/GateServer
./GateServer

# 遇到的问题

## OOM问题
在低内存服务器上编译时会崩溃，可以增加交换空间
sudo fallocate -l 4G /swapfile && sudo chmod 600 /swapfile
sudo mkswap /swapfile && sudo swapon /swapfile

永久生效：
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab

## 云服务器端口开放
GateServer 8080
ChatServer 50055/50058
ResourceServer 50053