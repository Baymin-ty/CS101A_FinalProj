#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Tank.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"
#include "Maze.hpp"
#include "MazeGenerator.hpp"

// 游戏状态枚举
enum class GameState
{
  MainMenu,
  Playing,
  GameOver,
  Victory
};

// 菜单选项枚举
enum class MenuOption
{
  StartGame,
  ToggleRandomMap,
  MapWidth,
  MapHeight,
  EnemyCount,
  Exit,
  Count // 用于计数
};

class Game
{
public:
  Game();

  bool init();
  void run();

private:
  void processEvents();
  void processMenuEvents(const sf::Event &event);
  void update(float dt);
  void render();
  void renderMenu();
  void renderGame();
  void renderGameOver();
  void checkCollisions();
  void spawnEnemies();
  void resetGame();
  void startGame();
  void updateCamera();
  void generateRandomMaze();

  // 配置（放在前面以便初始化列表使用）
  const unsigned int m_screenWidth = 1280;
  const unsigned int m_screenHeight = 720;
  const float m_shootCooldown = 0.3f;
  const float m_bulletSpeed = 500.f;
  const float m_cameraLookAhead = 150.f; // 视角向瞄准方向偏移量

  sf::RenderWindow m_window;
  sf::View m_gameView; // 游戏视图（跟随玩家）
  sf::View m_uiView;   // UI 视图（固定）

  std::unique_ptr<Tank> m_player;
  std::vector<std::unique_ptr<Enemy>> m_enemies;
  BulletManager m_bulletManager;
  Maze m_maze;
  MazeGenerator m_mazeGenerator;

  sf::Texture m_bulletTexture;
  sf::Font m_font;

  sf::Clock m_clock;
  sf::Clock m_shootClock;

  // 游戏状态
  GameState m_gameState = GameState::MainMenu;
  MenuOption m_selectedOption = MenuOption::StartGame;
  bool m_useRandomMap = true;

  // 地图尺寸选项
  std::vector<int> m_widthOptions = {21, 31, 41, 51, 61, 71};
  std::vector<int> m_heightOptions = {15, 21, 31, 41, 51};
  int m_widthIndex = 2;  // 默认 41
  int m_heightIndex = 2; // 默认 31

  // 敌人数量选项
  std::vector<int> m_enemyOptions = {3, 5, 8, 10, 15, 20, 30};
  int m_enemyIndex = 3; // 默认 10

  bool m_gameOver = false;
  bool m_gameWon = false;
};
