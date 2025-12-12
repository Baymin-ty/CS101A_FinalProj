#include "NetworkManager.hpp"
#include <iostream>
#include <cstring>

NetworkManager& NetworkManager::getInstance()
{
  static NetworkManager instance;
  return instance;
}

bool NetworkManager::connect(const std::string& host, unsigned short port)
{
  m_socket.setBlocking(true);
  auto address = sf::IpAddress::resolve(host);
  if (!address.has_value())
  {
    if (m_onError)
    {
      m_onError("Invalid IP address");
    }
    return false;
  }
  
  sf::Socket::Status status = m_socket.connect(address.value(), port, sf::seconds(5));
  m_socket.setBlocking(false);

  if (status != sf::Socket::Status::Done)
  {
    if (m_onError)
    {
      m_onError("Failed to connect to server");
    }
    return false;
  }

  m_connected = true;

  // 发送连接消息
  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::Connect));
  sendPacket(data);

  if (m_onConnected)
  {
    m_onConnected();
  }

  return true;
}

void NetworkManager::disconnect()
{
  if (m_connected)
  {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(NetMessageType::Disconnect));
    sendPacket(data);
  }

  m_socket.disconnect();
  m_connected = false;
  m_roomCode.clear();
  m_receiveBuffer.clear();
  
  if (m_onDisconnected)
  {
    m_onDisconnected();
  }
}

void NetworkManager::createRoom(int mazeWidth, int mazeHeight)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::CreateRoom));
  
  // 添加迷宫尺寸
  data.push_back(static_cast<uint8_t>(mazeWidth & 0xFF));
  data.push_back(static_cast<uint8_t>((mazeWidth >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(mazeHeight & 0xFF));
  data.push_back(static_cast<uint8_t>((mazeHeight >> 8) & 0xFF));

  sendPacket(data);
}

void NetworkManager::joinRoom(const std::string& roomCode)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::JoinRoom));
  data.push_back(static_cast<uint8_t>(roomCode.length()));
  for (char c : roomCode)
  {
    data.push_back(static_cast<uint8_t>(c));
  }

  sendPacket(data);
}

void NetworkManager::sendPosition(const PlayerState& state)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::PlayerUpdate));

  // 添加位置数据 (float -> bytes)
  auto addFloat = [&data](float value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addFloat(state.x);
  addFloat(state.y);
  addFloat(state.rotation);
  addFloat(state.turretAngle);
  addFloat(state.health);
  data.push_back(state.reachedExit ? 1 : 0);

  sendPacket(data);
}

void NetworkManager::sendShoot(float x, float y, float angle)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::PlayerShoot));

  auto addFloat = [&data](float value) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&value);
    for (int i = 0; i < 4; i++)
      data.push_back(bytes[i]);
  };

  addFloat(x);
  addFloat(y);
  addFloat(angle);

  sendPacket(data);
}

void NetworkManager::sendReachExit()
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::ReachExit));
  sendPacket(data);
}

void NetworkManager::sendGameResult(bool localWin)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::GameResult));
  data.push_back(localWin ? 1 : 0);  // 我赢了
  sendPacket(data);
}

void NetworkManager::sendRestartRequest()
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::RestartRequest));
  sendPacket(data);
}

void NetworkManager::sendNpcActivate(int npcId, int team)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcActivate));
  data.push_back(static_cast<uint8_t>(npcId));
  data.push_back(static_cast<uint8_t>(team));
  sendPacket(data);
}

void NetworkManager::sendNpcUpdate(const NpcState& state)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcUpdate));
  data.push_back(static_cast<uint8_t>(state.id));
  
  // 位置
  auto pushFloat = [&data](float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(float));
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  };
  
  pushFloat(state.x);
  pushFloat(state.y);
  pushFloat(state.rotation);
  pushFloat(state.turretAngle);
  pushFloat(state.health);
  data.push_back(static_cast<uint8_t>(state.team));
  data.push_back(state.activated ? 1 : 0);
  sendPacket(data);
}

void NetworkManager::sendNpcShoot(int npcId, float x, float y, float angle)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcShoot));
  data.push_back(static_cast<uint8_t>(npcId));
  
  auto pushFloat = [&data](float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(float));
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  };
  
  pushFloat(x);
  pushFloat(y);
  pushFloat(angle);
  sendPacket(data);
}

void NetworkManager::sendNpcDamage(int npcId, float damage)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::NpcDamage));
  data.push_back(static_cast<uint8_t>(npcId));
  
  uint32_t bits;
  std::memcpy(&bits, &damage, sizeof(float));
  data.push_back(static_cast<uint8_t>(bits & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
  sendPacket(data);
}

void NetworkManager::sendMazeData(const std::vector<std::string>& mazeData)
{
  if (!m_connected) return;

  std::vector<uint8_t> data;
  data.push_back(static_cast<uint8_t>(NetMessageType::MazeData));
  
  // 迷宫行数
  uint16_t rows = static_cast<uint16_t>(mazeData.size());
  data.push_back(static_cast<uint8_t>(rows & 0xFF));
  data.push_back(static_cast<uint8_t>((rows >> 8) & 0xFF));
  
  // 每行数据
  for (const auto& row : mazeData) {
    uint16_t len = static_cast<uint16_t>(row.length());
    data.push_back(static_cast<uint8_t>(len & 0xFF));
    data.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    for (char c : row) {
      data.push_back(static_cast<uint8_t>(c));
    }
  }
  
  sendPacket(data);
}

void NetworkManager::update()
{
  if (!m_connected) return;

  receiveData();
}

void NetworkManager::sendPacket(const std::vector<uint8_t>& data)
{
  if (!m_connected) return;

  // 添加长度前缀 (2 bytes)
  std::vector<uint8_t> packet;
  uint16_t len = static_cast<uint16_t>(data.size());
  packet.push_back(static_cast<uint8_t>(len & 0xFF));
  packet.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
  packet.insert(packet.end(), data.begin(), data.end());

  m_socket.setBlocking(true);
  std::size_t sent = 0;
  [[maybe_unused]] auto status = m_socket.send(packet.data(), packet.size(), sent);
  m_socket.setBlocking(false);
}

void NetworkManager::receiveData()
{
  uint8_t buffer[1024];
  std::size_t received = 0;

  sf::Socket::Status status = m_socket.receive(buffer, sizeof(buffer), received);

  if (status == sf::Socket::Status::Done && received > 0)
  {
    m_receiveBuffer.insert(m_receiveBuffer.end(), buffer, buffer + received);

    // 处理完整的消息
    while (m_receiveBuffer.size() >= 2)
    {
      uint16_t len = m_receiveBuffer[0] | (m_receiveBuffer[1] << 8);

      if (m_receiveBuffer.size() >= 2 + len)
      {
        std::vector<uint8_t> message(m_receiveBuffer.begin() + 2,
                                      m_receiveBuffer.begin() + 2 + len);
        m_receiveBuffer.erase(m_receiveBuffer.begin(),
                              m_receiveBuffer.begin() + 2 + len);
        processMessage(message);
      }
      else
      {
        break; // 等待更多数据
      }
    }
  }
  else if (status == sf::Socket::Status::Disconnected)
  {
    m_connected = false;
    if (m_onDisconnected)
    {
      m_onDisconnected();
    }
  }
}

void NetworkManager::processMessage(const std::vector<uint8_t>& data)
{
  if (data.empty()) return;

  NetMessageType type = static_cast<NetMessageType>(data[0]);

  auto readFloat = [&data](size_t offset) -> float {
    if (offset + 4 > data.size()) return 0.0f;
    float value;
    std::memcpy(&value, &data[offset], sizeof(float));
    return value;
  };

  switch (type)
  {
  case NetMessageType::RoomCreated:
  {
    if (data.size() > 2)
    {
      uint8_t codeLen = data[1];
      m_roomCode = std::string(data.begin() + 2, data.begin() + 2 + codeLen);
      if (m_onRoomCreated)
      {
        m_onRoomCreated(m_roomCode);
      }
    }
    break;
  }
  case NetMessageType::RoomJoined:
  {
    if (data.size() > 2)
    {
      uint8_t codeLen = data[1];
      m_roomCode = std::string(data.begin() + 2, data.begin() + 2 + codeLen);
      if (m_onRoomJoined)
      {
        m_onRoomJoined(m_roomCode);
      }
    }
    break;
  }
  case NetMessageType::RoomError:
  {
    if (data.size() > 2)
    {
      uint8_t errorLen = data[1];
      std::string error(data.begin() + 2, data.begin() + 2 + errorLen);
      if (m_onError)
      {
        m_onError(error);
      }
    }
    break;
  }
  case NetMessageType::GameStart:
  {
    // 新版本：GameStart 只是一个信号，迷宫数据已经通过 MazeData 传输
    if (m_onGameStart)
    {
      m_onGameStart();
    }
    break;
  }
  case NetMessageType::MazeData:
  {
    // 解析迷宫数据
    if (data.size() >= 3)
    {
      size_t offset = 1;
      uint16_t rows = data[offset] | (data[offset + 1] << 8);
      offset += 2;
      
      std::vector<std::string> mazeData;
      for (uint16_t i = 0; i < rows && offset + 2 <= data.size(); i++)
      {
        uint16_t len = data[offset] | (data[offset + 1] << 8);
        offset += 2;
        if (offset + len <= data.size())
        {
          std::string row(data.begin() + offset, data.begin() + offset + len);
          mazeData.push_back(row);
          offset += len;
        }
      }
      
      if (m_onMazeData)
      {
        m_onMazeData(mazeData);
      }
    }
    break;
  }
  case NetMessageType::RequestMaze:
  {
    // 服务器请求房主发送迷宫数据
    if (m_onRequestMaze)
    {
      m_onRequestMaze();
    }
    break;
  }
  case NetMessageType::PlayerUpdate:
  {
    if (data.size() >= 22)
    {
      PlayerState state;
      state.x = readFloat(1);
      state.y = readFloat(5);
      state.rotation = readFloat(9);
      state.turretAngle = readFloat(13);
      state.health = readFloat(17);
      state.reachedExit = data[21] != 0;
      if (m_onPlayerUpdate)
      {
        m_onPlayerUpdate(state);
      }
    }
    break;
  }
  case NetMessageType::PlayerShoot:
  {
    if (data.size() >= 13)
    {
      float x = readFloat(1);
      float y = readFloat(5);
      float angle = readFloat(9);
      if (m_onPlayerShoot)
      {
        m_onPlayerShoot(x, y, angle);
      }
    }
    break;
  }
  case NetMessageType::GameWin:
  {
    // 游戏胜利，两个玩家都到达终点
    // 可以在这里触发胜利回调
    break;
  }
  case NetMessageType::GameResult:
  {
    // 游戏结果 - 对方发来的结果（对方赢意味着本地输）
    if (data.size() >= 2)
    {
      bool otherPlayerWon = data[1] != 0;
      if (m_onGameResult)
      {
        // 对方赢 = 我输，对方输 = 我赢
        m_onGameResult(!otherPlayerWon);
      }
    }
    break;
  }
  case NetMessageType::RestartRequest:
  {
    // 对方请求重新开始
    if (m_onRestartRequest)
    {
      m_onRestartRequest();
    }
    break;
  }
  case NetMessageType::NpcActivate:
  {
    // NPC激活消息
    if (data.size() >= 3 && m_onNpcActivate)
    {
      int npcId = data[1];
      int team = data[2];
      m_onNpcActivate(npcId, team);
    }
    break;
  }
  case NetMessageType::NpcUpdate:
  {
    // NPC状态更新
    if (data.size() >= 24 && m_onNpcUpdate)
    {
      NpcState state;
      state.id = data[1];
      state.x = readFloat(2);
      state.y = readFloat(6);
      state.rotation = readFloat(10);
      state.turretAngle = readFloat(14);
      state.health = readFloat(18);
      state.team = data[22];
      state.activated = data[23] != 0;
      m_onNpcUpdate(state);
    }
    break;
  }
  case NetMessageType::NpcShoot:
  {
    // NPC射击
    if (data.size() >= 14 && m_onNpcShoot)
    {
      int npcId = data[1];
      float x = readFloat(2);
      float y = readFloat(6);
      float angle = readFloat(10);
      m_onNpcShoot(npcId, x, y, angle);
    }
    break;
  }
  case NetMessageType::NpcDamage:
  {
    // NPC受伤
    if (data.size() >= 6 && m_onNpcDamage)
    {
      int npcId = data[1];
      float damage = readFloat(2);
      m_onNpcDamage(npcId, damage);
    }
    break;
  }
  case NetMessageType::PlayerLeft:
  {
    // 对方玩家离开房间
    if (m_onPlayerLeft)
    {
      m_onPlayerLeft();
    }
    break;
  }
  default:
    break;
  }
}
