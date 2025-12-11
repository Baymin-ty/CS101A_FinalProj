#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Tank.hpp"
#include "Bullet.hpp"
#include "Enemy.hpp"

class Game
{
public:
  Game();

  bool init();
  void run();

private:
  void processEvents();
  void update(float dt);
  void render();
  void checkCollisions();
  void spawnEnemy();
  void resetGame();

  // 配置（放在前面以便初始化列表使用）
  const unsigned int m_screenWidth = 1280;
  const unsigned int m_screenHeight = 720;
  const float m_shootCooldown = 0.3f;
  const float m_bulletSpeed = 500.f;

  sf::RenderWindow m_window;
  std::unique_ptr<Tank> m_player;
  std::vector<std::unique_ptr<Enemy>> m_enemies;
  BulletManager m_bulletManager;

  sf::Texture m_bulletTexture;

  sf::Clock m_clock;
  sf::Clock m_shootClock;

  bool m_gameOver = false;
};
