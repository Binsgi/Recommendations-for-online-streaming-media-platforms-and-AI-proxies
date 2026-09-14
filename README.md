# 🎵 Demo Player - 现代化 C++17 高性能音乐播放器 & AI Agent 推荐系统

基于 **Modern C++17**、**Epoll Reactor 事件驱动模型**、**MySQL 数据库连接池** 与 **Vue.js 前端** 开发的现代化全功能音乐播放与推荐系统，适配 **Ubuntu 22.04 amd64** 编译与部署环境。

---

## 🌟 核心特性与架构升级

### 1. Modern C++17 重构
- 采用现代 C++17 标准（RAII、智能指针、STL 并发容器、读写锁 `std::shared_mutex`、`std::optional`）。
- **Epoll Reactor 高并发服务端**：配合现代化线程池 `ThreadPool` 异步分发 HTTP 请求。
- **CMake 项目构建**：支持在 Linux (Ubuntu 22.04) 上一键检测依赖并自动化编译。

### 2. 数据库与连接池 (MySQL)
- 原架构 SQLite3 全面升级至 **MySQL 8.0/5.7**，使用 `libmysqlclient` 编写线程安全的 **数据库连接池 (Database Connection Pool)**。
- 完整持久化表结构（用户账号、加盐哈希密码、收藏夹、用户自建歌单、歌单曲目、轻量歌词持久化缓存、播放历史、用户画像偏好表）。

### 3. 歌曲缓存与存储深度优化 (杜绝磁盘暴涨)
- **拒绝整首 MP3 落地**：音频采用流媒体直链与按需代理，不在服务器永久落盘几万首数十 MB 的 MP3 文件。
- **LRU 内存 + MySQL 双重缓存**：仅对几 KB 大小的歌词和歌曲元数据进行持久化与内存热点 LRU 缓存（带容量上限，自动过期淘汰）。

### 4. 完备的用户认证与安全体系
- **验证码系统**：支持生成并验证 6 位验证码（内存带 TTL 有效期管理，防暴力破解与重放）。
- **无感登录维持**：登录后颁发 `Access Token` 与 `Refresh Token`，支持刷新登录 (`/api/auth/refresh-token`)。
- **注册与修改密码**：支持基于验证码的用户注册以及找回/修改密码。
- **用户信息**：支持获取当前用户 Profile 与个人中心。

### 5. 丰富音乐生态与不可播放歌曲智能过滤
- **智能搜索建议**：输入关键字实时返回下拉搜索联想词 (`/api/search/suggest`)。
- **歌单与分类**：支持获取歌单分类（Catlist）、热门精选歌单及歌单详情曲目。
- **用户自建歌单**：支持用户创建个性化歌单与歌曲添加。
- **剔除无效歌曲**：后端在搜索与歌单加载时**自动批量校验音频可播放状态，过滤掉直链为空或受限无版权的歌曲**，呈现给前端纯净可播放的歌单列表。

### 6. 🤖 AI Agent 个性化音乐助手
- 前端设立专门的 **AI 音乐智能体 (MelodyAI)** 交互面板。
- 支持根据用户输入的自然语言（听歌场景、心情、特定歌手）+ 结合用户历史收藏流派画像进行智能特征匹配。
- 智能体生成结构化推荐单曲，为每首歌曲输出**专属推荐理由**与**匹配度百分比**，支持一键试听、加入播放列表与收藏。

---

## 🛠️ 项目目录结构

```
demo-player/
├── CMakeLists.txt              # CMake 构建工程文件 (C++17)
├── .gitignore                  # Git 忽略配置
├── config.ini                  # 服务端配置文件 (端口/MySQL/API)
├── sql/
│   └── init.sql                # MySQL 数据库表结构初始化脚本
├── include/                    # C++ 头文件
│   ├── Config.hpp              # 全局配置读取单例
│   ├── Server.hpp              # epoll 网络服务器
│   ├── ThreadPool.hpp          # 现代 C++ 线程池
│   ├── Http.hpp                # HTTP 请求/响应与 MIME 解析
│   ├── Router.hpp              # RESTful API 路由分发器
│   ├── Database.hpp            # MySQL 连接池与数据层
│   ├── Cache.hpp               # 线程安全 LRU 缓存与验证码
│   ├── ApiProxy.hpp            # libcurl 代理与网易云 API 客户端
│   ├── AuthService.hpp         # 用户认证、Token、验证码与密码管理
│   ├── MusicService.hpp        # 歌单、分类、过滤无版权音乐、歌词缓存
│   ├── AgentService.hpp        # AI 智能音乐推荐引擎
│   └── json.hpp                # 现代 C++ JSON 解析工具
├── src/                        # C++ 源码实现
│   ├── main.cpp                # 程序入口
│   ├── Config.cpp
│   ├── Server.cpp
│   ├── Http.cpp
│   ├── Router.cpp
│   ├── Database.cpp
│   ├── Cache.cpp
│   ├── ApiProxy.cpp
│   ├── AuthService.cpp
│   ├── MusicService.cpp
│   └── AgentService.cpp
├── web/                        # 前端资源
│   ├── index.html              # 现代化全功能单页音乐播放器
│   ├── css/
│   │   ├── index.css           # 经典黑胶样式
│   │   └── modern.css          # 现代化玻璃拟态与 Agent 样式
│   ├── js/
│   │   ├── api.js              # Axios 统一 API 模块 (含 Token 自动刷新)
│   │   └── app.js              # Vue 应用核心逻辑
│   └── images/                 # 播放器图片资产
└── README.md                   # 部署与使用文档
```

---

## 🚀 环境依赖与部署运行 (Ubuntu 22.04 amd64)

### 1. 安装编译工具与系统依赖
```bash
sudo apt update
sudo apt install -y build-essential cmake g++ \
    libmysqlclient-dev libcurl4-openssl-dev libssl-dev git mysql-server
```

### 2. 配置并初始化 MySQL 数据库
启动 MySQL 服务并导入 `sql/init.sql`：
```bash
# 启动 MySQL 服务
sudo systemctl start mysql

# 导入初始化脚本 (默认创建 demo_player 数据库及全部业务表)
mysql -u root -p < sql/init.sql
```

> **提示**：若 MySQL 的 root 密码与 `config.ini` 中的不一致，请修改 `config.ini` 中的 `[mysql]` 配置项。

### 3. 启动外部网易云 API 代理服务
本项目对接标准 NeteaseCloudMusicApi：
```bash
# 若尚未安装 Node.js API 代理
git clone https://github.com/Binaryify/NeteaseCloudMusicApi.git
cd NeteaseCloudMusicApi
npm install
npm start
# 默认运行在 http://localhost:3000
```

### 4. 使用 CMake 编译项目
在项目根目录下执行：
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```
编译成功后，可执行二进制文件将生成在 `bin/demo_player`。

### 5. 启动播放器后端服务
在项目根目录运行：
```bash
./bin/demo_player
```
控制台输出：
```text
========================================================
       🎵 Demo Player C++17 Server (Ubuntu 22.04)        
========================================================
[Config] Successfully loaded configuration from config.ini
[Database] Initializing MySQL connection pool (Size: 10)...
[Database] Connection pool initialized successfully. Active connections: 10
[Server] 🚀 DemoPlayer Server is running on port 8080
[Server] 🌐 Web Interface: http://localhost:8080
```

打开浏览器访问：**`http://localhost:8080`** 即可畅享完整音乐播放器与 AI 智能推荐！

---

## 📡 API 路由与接口说明

### 1. 用户认证与安全
- `POST /api/auth/send-code`：发送手机/邮箱验证码
- `POST /api/auth/verify-code`：校验验证码
- `POST /api/auth/register`：用户注册（加盐 SHA-256）
- `POST /api/auth/login`：用户登录（颁发 Access/Refresh Token）
- `POST /api/auth/refresh-token`：刷新登录 Token
- `POST /api/auth/reset-password`：验证码修改/重置密码
- `GET /api/user/profile`：获取当前登录用户信息

### 2. 音乐、搜索与歌单
- `GET /api/search/suggest?keywords=xxx`：搜索实时联想词
- `GET /api/song/search?keywords=xxx`：歌曲搜索（**自动过滤不可播放/URL为空歌曲**）
- `GET /api/song/url?id=xxx&level=standard`：获取音频可播放直链与音质
- `GET /api/song/lyric?id=xxx`：获取歌词（**LRU 内存 -> MySQL -> 外部 API 代理**）
- `GET /api/playlist/catlist`：获取歌单分类标签
- `GET /api/playlist/hot?cat=全部`：获取热门精选歌单
- `GET /api/playlist/detail?id=xxx`：获取歌单曲目（**自动过滤不可播放曲目**）
- `GET /api/user/playlists`：获取用户自建歌单列表

### 3. 收藏与历史
- `POST /api/favorite/toggle`：一键收藏 / 取消收藏
- `GET /api/favorite/list`：获取个人收藏歌单
- `POST /api/history/record`：记录播放历史
- `GET /api/history/list`：获取播放历史记录

### 4. 🤖 AI Agent 个性化推荐
- `POST /api/agent/recommend`：
  - 请求参数：`{ "prompt": "深夜写代码专注音乐", "mood": "专注", "genre": "轻音乐" }`
  - 返回数据：包含 AI 智能体问候语、情绪标签萃取、匹配歌曲列表（附带**匹配度**与**AI 推荐理由**）。

---

## 📦 Git 版本管理

初始化并提交：
```bash
git init
git add .
git commit -m "feat: complete C++17 modernization with MySQL, CMake, Auth, Playlists, Filter & AI Agent"
```
