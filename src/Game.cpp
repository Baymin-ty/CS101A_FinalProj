#include "Game.hpp"
#include <algorithm>

Game::Game()
    : m_window(sf::VideoMode({m_screenWidth, m_screenHeight}), "Tank Game")
{
  m_window.setFramerateLimit(60);
}

bool Game::init()
{
  // 创建玩家
  m_player = std::make_unique<Tank>();

  // 加载玩家坦克纹理
  if (!m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                              "tank_assets/PNG/Weapon_Color_A/Gun_01.png"))
  {
    return false;
  }

  // 加载子弹纹理
  if (!m_bulletTexture.loadFromFile("tank_assets/PNG/Effects/Medium_Shell.png"))
  {
    return false;
  }

  m_bulletManager.setTexture(m_bulletTexture);

  // 生成初始敌人
  spawnEnemy();

  return true;
}

void Game::spawnEnemy()
{
  auto enemy = std::make_unique<Enemy>();
  if (enemy->loadTextures("tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                          "tank_assets/PNG/Weapon_Color_B/Gun_01.png"))
  {
    // 在随机位置生成敌人（避开玩家位置）
    float x = 100.f + static_cast<float>(rand() % 1000);
    float y = 100.f + static_cast<float>(rand() % 500);
    enemy->setPosition({x, y});
    m_enemies.push_back(std::move(enemy));
  }
}

void Game::resetGame()
{
  m_gameOver = false;
  m_enemies.clear();

  m_player = std::make_unique<Tank>();
  m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                         "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
  spawnEnemy();
}

void Game::run()
{
  while (m_window.isOpen())
  {
    float dt = m_clock.restart().asSeconds();
    processEvents();
    if (!m_gameOver)
    {
      update(dt);
    }
    render();
  }
}

void Game::processEvents()
{
  while (const auto event = m_window.pollEvent())
  {
    if (event->is<sf::Event::Closed>())
    {
      m_window.close();
    }

    // 按 R 重新开始
    if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
    {
      if (keyPressed->code == sf::Keyboard::Key::R && m_gameOver)
      {
        resetGame();
      }
    }

    if (!m_gameOver && m_player)
    {
      m_player->handleInput(*event);
    }
  }
}

void Game::update(float dt)
{
  if (!m_player)
    return;

  // 获取鼠标位置
  sf::Vector2i mousePos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mousePosF{static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)};

  // 更新玩家
  m_player->update(dt, mousePosF);

  // 玩家射击
  if (m_player->isShooting() && m_shootClock.getElapsedTime().asSeconds() > m_shootCooldown)
  {
    m_bulletManager.spawn(m_player->getGunPosition(),
                          m_player->getTurretAngle(),
                          m_bulletSpeed,
                          BulletOwner::Player);
    m_shootClock.restart();
  }

  // 更新敌人
  for (auto &enemy : m_enemies)
  {
    enemy->setTarget(m_player->getPosition());
    enemy->update(dt);

    // 敌人射击
    if (enemy->shouldShoot())
    {
      m_bulletManager.spawn(enemy->getGunPosition(),
                            enemy->getTurretAngle(),
                            m_bulletSpeed * 0.8f,
                            BulletOwner::Enemy);
    }
  }

  // 更新子弹
  m_bulletManager.update(dt, static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));

  // 检查碰撞
  checkCollisions();

  // 移除死亡的敌人
  size_t enemyCountBefore = m_enemies.size();
  m_enemies.erase(
      std::remove_if(m_enemies.begin(), m_enemies.end(),
                     [](const std::unique_ptr<Enemy> &e)
                     { return e->isDead(); }),
      m_enemies.end());

  // 如果有敌人被消灭，生成新敌人
  if (m_enemies.size() < enemyCountBefore)
  {
    spawnEnemy();
  }

  // 检查玩家是否死亡
  if (m_player->isDead())
  {
    m_gameOver = true;
  }
}

void Game::checkCollisions()
{
  if (!m_player)
    return;

  // 检查子弹与玩家的碰撞
  float playerDamage = m_bulletManager.checkCollision(
      m_player->getPosition(),
      m_player->getCollisionRadius(),
      BulletOwner::Player); // 忽略玩家自己的子弹

  if (playerDamage > 0)
  {
    m_player->takeDamage(playerDamage);
  }

  // 检查子弹与敌人的碰撞
  for (auto &enemy : m_enemies)
  {
    float enemyDamage = m_bulletManager.checkCollision(
        enemy->getPosition(),
        enemy->getCollisionRadius(),
        BulletOwner::Enemy); // 忽略敌人自己的子弹

    if (enemyDamage > 0)
    {
      enemy->takeDamage(enemyDamage);
    }
  }
}

void Game::render()
{
  m_window.clear(sf::Color(50, 50, 50));

  m_bulletManager.draw(m_window);

  if (m_player)
  {
    m_player->draw(m_window);
  }

  // 绘制敌人
  for (const auto &enemy : m_enemies)
  {
    enemy->draw(m_window);
  }

  // 绘制 UI
  if (m_player)
  {
    m_player->drawUI(m_window);
  }

  m_window.display();
}
