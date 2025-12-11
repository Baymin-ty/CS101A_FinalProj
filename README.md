# Tank Maze 🎮

一个基于 SFML 3.0 的坦克迷宫游戏，支持单人模式和双人联机对战。

## 游戏简介

在随机生成的迷宫中驾驶坦克，击败敌人或与好友对战！游戏提供两种模式：

- **单人模式**：在迷宫中击败所有 NPC 敌人并到达终点
- **多人模式**：两名玩家实时联机对战，先到达终点或击败对方者获胜

## 游戏特性

- 🎲 **随机迷宫生成**：每局游戏都有独特的迷宫布局
- 🎯 **可自定义设置**：地图尺寸、敌人数量均可调节
- 💥 **可破坏墙壁**：部分墙壁可被子弹摧毁，开辟新路径
- 🌐 **实时联机对战**：基于 TCP 的双人联机系统
- 🎨 **精美坦克贴图**：多种颜色的坦克外观

## 操作说明

| 按键 | 功能 |
|------|------|
| W/A/S/D | 移动坦克 |
| 鼠标移动 | 控制炮塔方向 |
| 鼠标左键 | 射击 |
| ESC | 暂停/返回菜单 |
| R | 重新开始 |

## 游戏模式

### 单人模式

1. 在主菜单选择 **Start Game**
2. 可调节以下设置：
   - **Random Map**: 开启/关闭随机地图
   - **Map Width**: 地图宽度 (21-71)
   - **Map Height**: 地图高度 (15-51)
   - **Enemies**: 敌人数量 (3-30)
3. 击败所有敌人并到达绿色终点即可获胜

### 多人模式

1. 启动服务器：`node server/server.js`
2. 在主菜单选择 **Multiplayer**
3. 输入服务器 IP 地址（本地测试使用 127.0.0.1）
4. **创建房间**：按 `C` 键创建房间，获得房间码
5. **加入房间**：输入房间码加入已创建的房间
6. 两名玩家都加入后游戏自动开始

**胜利条件**：
- 先到达绿色终点
- 或击败对方玩家

**重新开始**：
- 房主按 R：重新生成迷宫，等待对方玩家
- 非房主按 R：自动重新加入房间

## 编译与运行

### 依赖

- CMake 3.16+
- C++17 编译器
- SFML 3.0（自动通过 FetchContent 下载）
- Node.js（仅多人模式服务器需要）

### 编译

```bash
mkdir Build && cd Build
cmake ..
make
```

### 运行

```bash
# 单人模式
./CS101AFinalProj

# 多人模式（先启动服务器）
node ../server/server.js &
./CS101AFinalProj
```

## 项目结构

```
CS101A_FinalProj/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 项目说明文档
│
├── src/                    # 源代码目录
│   ├── main.cpp            # 程序入口
│   ├── Game.cpp            # 游戏主逻辑（状态管理、渲染、事件处理）
│   ├── Tank.cpp            # 坦克类（玩家控制、移动、射击）
│   ├── Enemy.cpp           # 敌人 AI（巡逻、追踪、攻击）
│   ├── Bullet.cpp          # 子弹类（移动、碰撞）
│   ├── Maze.cpp            # 迷宫类（地图数据、渲染、碰撞检测）
│   ├── MazeGenerator.cpp   # 迷宫生成器（随机迷宫算法）
│   ├── HealthBar.cpp       # 血条 UI 组件
│   ├── NetworkManager.cpp  # 网络管理（TCP 连接、消息收发）
│   │
│   └── include/            # 头文件目录
│       ├── Game.hpp
│       ├── Tank.hpp
│       ├── Enemy.hpp
│       ├── Bullet.hpp
│       ├── Maze.hpp
│       ├── MazeGenerator.hpp
│       ├── HealthBar.hpp
│       ├── NetworkManager.hpp
│       └── Utils.hpp       # 工具函数（角度计算、常量定义）
│
├── server/                 # 多人游戏服务器
│   └── server.js           # Node.js TCP 服务器
│
├── tank_assets/            # 游戏资源
│   ├── PNG/                # 坦克贴图
│   │   ├── Hulls_Color_A/  # 玩家坦克车身
│   │   ├── Hulls_Color_B/  # 敌人/对手坦克车身
│   │   ├── Weapon_Color_A/ # 玩家炮塔
│   │   └── Weapon_Color_B/ # 敌人/对手炮塔
│   └── ...
│
├── Build/                  # 编译输出目录
└── .vscode/                # VS Code 配置
```

## 核心模块说明

| 模块 | 功能 |
|------|------|
| `Game` | 游戏主循环、状态机、场景渲染、事件分发 |
| `Tank` | 坦克实体，处理输入、移动、旋转、射击 |
| `Enemy` | 敌人 AI，包含巡逻、追踪、攻击行为 |
| `Bullet` | 子弹物理、碰撞检测、伤害计算 |
| `Maze` | 迷宫数据结构、瓦片渲染、墙壁碰撞 |
| `MazeGenerator` | 使用 DFS 算法生成随机迷宫 |
| `NetworkManager` | 单例网络管理器，处理 TCP 通信 |
| `HealthBar` | 血条 UI 绘制 |

## 网络协议

多人模式使用自定义二进制协议，消息格式：

```
[2 bytes: length][1 byte: type][payload...]
```

主要消息类型：
- `CreateRoom` / `JoinRoom`：房间操作
- `MazeData`：迷宫数据同步
- `PlayerUpdate`：玩家状态同步（位置、角度、血量）
- `PlayerShoot`：射击事件
- `GameResult`：游戏结果

## License

MIT License
