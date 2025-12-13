#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include "Tank.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "Maze.hpp"
#include "NetworkManager.hpp"

// 多人模式状态
struct MultiplayerState
{
  bool isMultiplayer = false;
  bool isHost = false;
  bool localPlayerReachedExit = false;
  bool otherPlayerReachedExit = false;
  bool multiplayerWin = false;
  std::string roomCode;
  std::string connectionStatus = "Enter server IP:";
  int npcSyncCounter = 0;
  int nearbyNpcIndex = -1;
  bool rKeyJustPressed = false;
  std::vector<std::string> generatedMazeData;
};

// 多人游戏渲染和更新所需的上下文
struct MultiplayerContext
{
  sf::RenderWindow& window;
  sf::View& gameView;
  sf::View& uiView;
  sf::Font& font;
  Tank* player;
  Tank* otherPlayer;
  std::vector<std::unique_ptr<Enemy>>& enemies;
  std::vector<std::unique_ptr<Bullet>>& bullets;
  Maze& maze;
  unsigned int screenWidth;
  unsigned int screenHeight;
  float tankScale;
  bool placementMode;  // 墙壁放置模式
};

// 多人模式处理器
class MultiplayerHandler
{
public:
  // 更新多人游戏逻辑
  static void update(
    MultiplayerContext& ctx,
    MultiplayerState& state,
    float dt,
    std::function<void()> onVictory,
    std::function<void()> onDefeat);

  // 渲染连接界面
  static void renderConnecting(
    sf::RenderWindow& window,
    sf::View& uiView,
    sf::Font& font,
    unsigned int screenWidth,
    unsigned int screenHeight,
    const std::string& connectionStatus,
    const std::string& inputText,
    bool isServerIPMode);

  // 渲染等待玩家界面
  static void renderWaitingForPlayer(
    sf::RenderWindow& window,
    sf::View& uiView,
    sf::Font& font,
    unsigned int screenWidth,
    unsigned int screenHeight,
    const std::string& roomCode);

  // 渲染多人游戏界面
  static void renderMultiplayer(
    MultiplayerContext& ctx,
    MultiplayerState& state);

private:
  // 更新NPC AI逻辑（仅房主执行）
  static void updateNpcAI(
    MultiplayerContext& ctx,
    MultiplayerState& state,
    float dt);

  // 检查玩家接近的NPC（用于激活提示）
  static void checkNearbyNpc(
    MultiplayerContext& ctx,
    MultiplayerState& state);

  // 处理NPC激活
  static void handleNpcActivation(
    MultiplayerContext& ctx,
    MultiplayerState& state);

  // 渲染NPC及其标记
  static void renderNpcs(
    MultiplayerContext& ctx,
    MultiplayerState& state);

  // 渲染UI血条和金币
  static void renderUI(
    MultiplayerContext& ctx,
    MultiplayerState& state);
};
