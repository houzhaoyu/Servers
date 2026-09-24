# 分布式 C++ 即时通讯系统

这是一款基于 C++17 开发的高性能、可扩展的分布式即时通讯后端系统。项目采用微服务架构思想，将网关、逻辑、状态及资源处理解耦，支持集群化部署与分布式链路追踪。



配套的客户端代码：

https://github.com/houzhaoyu/ChatClient.git



## 1. 核心架构

系统由以下五个主要部分组成：

- **VerifyServer(验证服务器)**：基于 Node.js 开发。负责验证码的生成、Redis 存储（过期机制）及邮件发送（SMTP）。它通过 gRPC 为 GateServer 提供验证服务

- **GateServer (网关服务器)**：系统的唯一入口。处理 HTTP 请求，负责用户注册、登录校验、验证码分发（对接 VerifyServer），并根据负载为客户端分配 ChatServer。
- **ChatServer (聊天服务器)**：核心业务节点。通过 TCP 长连接维持用户在线状态，处理聊天消息，支持跨服务器的消息转发（通过 gRPC 路由）。
- **StatusServer (状态服务器)**：全局管理用户在线状态、登录位置（Session 映射）及各聊天服务器的负载情况。
- **ResourceServer (资源服务器)**：处理文件数据。负责图片和文件的分片上传、断点续传（基于 MD5 校验）及下载。
- **Common Library (公共库)**：封装了数据库连接池（MySQL/Redis）、网络底层（Asio）、分布式锁、协议定义（Protobuf）及统一日志系统。

## 2. 技术栈

- **语言标准**：C++17, Node.js

- **网络框架**：Boost.Asio, Boost.Beast (处理异步 IO、TCP 及 HTTP)

- **远程过程调用 (RPC)**：gRPC + Protobuf (服务间通信及序列化)

- **数据库**：

  - **MySQL**：用户信息、好友关系、聊天记录持久化存储。
  - **Redis**：缓存验证码、Session 状态、在线人数及实现分布式锁。

- **日志系统**：spdlog (异步高性能日志)

- **序列化**：Jsoncpp, Protobuf

- **构建工具**：CMake (Version 3.16+)

  

## 3. 环境要求

### 操作系统

- **Windows** (推荐使用 Visual Studio 2022)
- **Linux** (需安装对应版本的 GCC/Clang)

### 必备中间件

1. **MySQL 8.0+**：需创建名为 ChatApp 的数据库。
2. **Redis**：用于存储实时状态及分布式锁。
3. **Node.js 16+**：用于运行 VerifyServer。

### 第三方库依赖

依赖通过 **vcpkg** 管理（Boost、gRPC/Protobuf、hiredis），MySQL Connector/C++ 使用官方二进制（9.7）：

- Boost
- gRPC & Protobuf
- MySQL Connector/C++ (9.7)
- hiredis (C 接口)

## 4. 构建与编译

项目采用 **Monorepo** 结构，通过根目录的一键构建所有服务。第三方依赖通过 **vcpkg** 管理，构建参数通过 `CMakePresets.json` 配置。

### 步骤 1：Windows 环境配置与编译准备

**1.1 安装 vcpkg 与项目依赖**

项目根目录已提供 `vcpkg.json` 清单，声明了 `boost`、`hiredis`、`grpc`（含 `codegen` 特性）。

- 安装 vcpkg（本例为 `D:\tool\vcpkg`）；如走代理，先在 PowerShell 中设置：
  ```powershell
  $env:HTTP_PROXY = "http://127.0.0.1:7890"
  $env:HTTPS_PROXY = "http://127.0.0.1:7890"
  ```
- 在项目根目录执行（manifest 模式，产物安装到 `vcpkg_installed/`）：
  ```
  D:\tool\vcpkg\vcpkg.exe install --triplet x64-windows
  ```

**1.2 安装 MySQL Connector/C++（官方二进制 9.x）**

- 从 https://dev.mysql.com/downloads/connector/cpp/ 下载 **9.x 系列** Windows x64 MSI 安装器（注意：不要选默认列出的 26.x，那是 X DevAPI 线，不含 JDBC API）。
- 安装时选择 **Complete（完整）**，或 **Custom（自定义）** 并勾选 **「JDBC API」的 DLL 组件 + Developer 组件**（否则缺少头文件与 `.lib` 导入库，无法编译链接）。
- 默认安装到 `C:\Program Files\MySQL\MySQL Connector C++ 9.7`。

**1.3 核对 CMakePresets.json**

确认 `cacheVariables` 与本机安装路径一致：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "vcpkg-debug",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "D:/tool/vcpkg/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "VCPKG_INSTALLED_DIR": "${sourceDir}/vcpkg_installed",
        "MYSQL_ROOT": "C:/Program Files/MySQL/MySQL Connector C++ 9.7"
      }
    }
  ]
}
```



### 步骤 2：编译

**使用 Visual Studio 2022**:

1. “打开文件夹”选择 `Servers/` 目录。
2. 在 CMake 预设中选择 `vcpkg-debug`。
3. 等待配置完成（自动生成 gRPC 的 `.pb.h/.cc` 文件）。
4. 点击 **生成 -> 全部生成**。

**使用命令行（Windows，走 vcpkg preset）**:

```
cmake --preset vcpkg-debug
cmake --build build --config Debug
```

**使用命令行（Linux，手动指定依赖路径）**:

```
mkdir build && cd build
cmake -DBOOST_ROOT=... -DGRPC_ROOT=... -DMYSQL_ROOT=... ..
cmake --build . --config Debug
```

**安装 VerifyServer 依赖**:

```
cd VerifyServer
npm install
```



## 5. 启动指南

必须按顺序启动服务，以满足 gRPC 的依赖关系：

1. **基础设施**：启动 MySQL 和 Redis。
2. **VerifyServer**：启动验证码服务。
3. **StatusServer**：启动状态服务器（默认端口 50052）。
4. **ResourceServer**：启动资源服务器（默认端口 50053）。
5. **ChatServer**：可以启动多个实例。
   - 实例1：ChatServer.exe -S chatserver1
   - 实例2：ChatServer.exe -S chatserver2
6. **GateServer**：启动网关服务器（默认端口 8080）。

## 6. 项目特性

- **多语言混合架构**：利用 Node.js 快速处理 IO 密集型的邮件发送任务，利用 C++ 处理高性能长连接业务。
- **单点登录 (SSO)**：基于 Redis 分布式锁和 Session 校验，实现多端登录互踢逻辑。
- **异步日志**：集成 spdlog 异步模式，支持 TraceId 链路追踪，方便在分布式环境下定位问题。
- **高并发 IO**：底层采用 Asio 线程池模型，业务逻辑通过 LogicSystem 队列实现 IO 与逻辑分离。
- **断点续传**：资源服务器根据文件 MD5 识别重复块，支持大文件的可靠传输。









|              |                                                  |
| ------------ | ------------------------------------------------ |
| **feat**     | 新功能 (feature)                                 |
| **fix**      | 修补 bug                                         |
| **docs**     | 文档修改 (documentation)                         |
| **style**    | 格式变动 (不影响代码运行的变动，如空格、分号等)  |
| **refactor** | 重构 (既不是新增功能，也不是修改 bug 的代码变动) |
| **perf**     | 性能优化 (performance)                           |