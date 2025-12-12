#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>

// 网络消息类型
enum class NetMessageType : uint8_t
{
  // 连接相关
  Connect = 1,
  ConnectAck,
  Disconnect,

  // 游戏房间
  CreateRoom,
  JoinRoom,
  RoomCreated,
  RoomJoined,
  RoomError,
  GameStart,

  // 游戏状态同步
  PlayerUpdate,  // 玩家位置、角度等
  PlayerShoot,   // 玩家射击
  MazeData,      // 迷宫数据
  RequestMaze,   // 请求迷宫数据
  ReachExit,     // 到达终点
  GameWin,       // 游戏胜利（先到终点）
  GameResult,    // 游戏结果（胜/负）
  RestartRequest, // 重新开始请求
  
  // NPC同步
  NpcActivate,   // NPC激活
  NpcUpdate,     // NPC状态更新（位置、血量等）
  NpcShoot,      // NPC射击
  NpcDamage,     // NPC受伤
  
  // 玩家离开
  PlayerLeft,    // 对方玩家离开房间
};

// 玩家状态数据
struct PlayerState
{
  float x = 0, y = 0;
  float rotation = 0;
  float turretAngle = 0;
  float health = 100;
  bool reachedExit = false;
};

// NPC状态数据
struct NpcState
{
  int id = 0;
  float x = 0, y = 0;
  float rotation = 0;
  float turretAngle = 0;
  float health = 100;
  int team = 0;
  bool activated = false;
};

// 回调类型
using OnConnectedCallback = std::function<void()>;
using OnDisconnectedCallback = std::function<void()>;
using OnRoomCreatedCallback = std::function<void(const std::string& roomCode)>;
using OnRoomJoinedCallback = std::function<void(const std::string& roomCode)>;
using OnGameStartCallback = std::function<void()>;
using OnMazeDataCallback = std::function<void(const std::vector<std::string>& mazeData)>;
using OnRequestMazeCallback = std::function<void()>;
using OnPlayerUpdateCallback = std::function<void(const PlayerState& state)>;
using OnPlayerShootCallback = std::function<void(float x, float y, float angle)>;
using OnGameResultCallback = std::function<void(bool isWinner)>;
using OnRestartRequestCallback = std::function<void()>;
using OnErrorCallback = std::function<void(const std::string& error)>;
using OnNpcActivateCallback = std::function<void(int npcId, int team)>;
using OnNpcUpdateCallback = std::function<void(const NpcState& state)>;
using OnNpcShootCallback = std::function<void(int npcId, float x, float y, float angle)>;
using OnNpcDamageCallback = std::function<void(int npcId, float damage)>;
using OnPlayerLeftCallback = std::function<void()>;

class NetworkManager
{
public:
  static NetworkManager& getInstance();

  // 禁止拷贝
  NetworkManager(const NetworkManager&) = delete;
  NetworkManager& operator=(const NetworkManager&) = delete;

  // 连接到服务器
  bool connect(const std::string& host, unsigned short port);
  void disconnect();
  bool isConnected() const { return m_connected; }

  // 房间操作
  void createRoom(int mazeWidth, int mazeHeight);
  void joinRoom(const std::string& roomCode);

  // 发送迷宫数据（房主调用）
  void sendMazeData(const std::vector<std::string>& mazeData);

  // 发送游戏数据
  void sendPosition(const PlayerState& state);
  void sendShoot(float x, float y, float angle);
  void sendReachExit();
  void sendGameResult(bool localWin);  // 发送游戏结果
  void sendRestartRequest();           // 发送重新开始请求
  
  // NPC同步
  void sendNpcActivate(int npcId, int team);  // 发送NPC激活
  void sendNpcUpdate(const NpcState& state);  // 发送NPC状态更新
  void sendNpcShoot(int npcId, float x, float y, float angle);  // 发送NPC射击
  void sendNpcDamage(int npcId, float damage);  // 发送NPC受伤

  // 处理网络消息（在主线程调用）
  void update();

  // 设置回调
  void setOnConnected(OnConnectedCallback cb) { m_onConnected = cb; }
  void setOnDisconnected(OnDisconnectedCallback cb) { m_onDisconnected = cb; }
  void setOnRoomCreated(OnRoomCreatedCallback cb) { m_onRoomCreated = cb; }
  void setOnRoomJoined(OnRoomJoinedCallback cb) { m_onRoomJoined = cb; }
  void setOnGameStart(OnGameStartCallback cb) { m_onGameStart = cb; }
  void setOnMazeData(OnMazeDataCallback cb) { m_onMazeData = cb; }
  void setOnRequestMaze(OnRequestMazeCallback cb) { m_onRequestMaze = cb; }
  void setOnPlayerUpdate(OnPlayerUpdateCallback cb) { m_onPlayerUpdate = cb; }
  void setOnPlayerShoot(OnPlayerShootCallback cb) { m_onPlayerShoot = cb; }
  void setOnGameResult(OnGameResultCallback cb) { m_onGameResult = cb; }
  void setOnRestartRequest(OnRestartRequestCallback cb) { m_onRestartRequest = cb; }
  void setOnError(OnErrorCallback cb) { m_onError = cb; }
  void setOnNpcActivate(OnNpcActivateCallback cb) { m_onNpcActivate = cb; }
  void setOnNpcUpdate(OnNpcUpdateCallback cb) { m_onNpcUpdate = cb; }
  void setOnNpcShoot(OnNpcShootCallback cb) { m_onNpcShoot = cb; }
  void setOnNpcDamage(OnNpcDamageCallback cb) { m_onNpcDamage = cb; }
  void setOnPlayerLeft(OnPlayerLeftCallback cb) { m_onPlayerLeft = cb; }

  std::string getRoomCode() const { return m_roomCode; }

private:
  NetworkManager() = default;
  ~NetworkManager() { disconnect(); }

  void sendPacket(const std::vector<uint8_t>& data);
  void receiveData();
  void processMessage(const std::vector<uint8_t>& data);

  sf::TcpSocket m_socket;
  bool m_connected = false;
  std::string m_roomCode;

  // 接收缓冲区
  std::vector<uint8_t> m_receiveBuffer;

  // 回调
  OnConnectedCallback m_onConnected;
  OnDisconnectedCallback m_onDisconnected;
  OnRoomCreatedCallback m_onRoomCreated;
  OnRoomJoinedCallback m_onRoomJoined;
  OnGameStartCallback m_onGameStart;
  OnMazeDataCallback m_onMazeData;
  OnRequestMazeCallback m_onRequestMaze;
  OnPlayerUpdateCallback m_onPlayerUpdate;
  OnPlayerShootCallback m_onPlayerShoot;
  OnGameResultCallback m_onGameResult;
  OnRestartRequestCallback m_onRestartRequest;
  OnErrorCallback m_onError;
  OnNpcActivateCallback m_onNpcActivate;
  OnNpcUpdateCallback m_onNpcUpdate;
  OnNpcShootCallback m_onNpcShoot;
  OnNpcDamageCallback m_onNpcDamage;
  OnPlayerLeftCallback m_onPlayerLeft;
};
