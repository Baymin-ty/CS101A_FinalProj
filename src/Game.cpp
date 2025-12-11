#include "Game.hpp"
#include <algorithm>
#include <cmath>

Game::Game()
    : m_window(sf::VideoMode({m_screenWidth, m_screenHeight}), "Tank Maze Game"),
      m_gameView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)})),
      m_uiView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)}))
{
  m_window.setFramerateLimit(60);
}

bool Game::init()
{
  // 定义迷宫地图
  // '#' = 不可破坏墙, '*' = 可破坏墙, '.' = 空地, 'S' = 起点, 'E' = 出口, 'X' = 敌人
  std::vector<std::string> mazeMap = {
      "###############################################",
      "#S..#.....#.....*.....#.......#.....#.......E#",
      "#...#.###.#.###.*.###.#.#####.#.###.#.#####.##",
      "#.###.#...#.#.#.*.#.....#...#.#.#...#.#.....##",
      "#.....#.###.#.#.#.#####.#.#.#.#.#.###.#.######",
      "#####.#.#...#...#.#X....#.#.#...#.#...#......#",
      "#.....#.#.#####.###.###.#.#.#####.#.#######..#",
      "#.#####.#.....#.....#.#.#.#.....#.#.....#....#",
      "#.#.....#####.#######.#.#.#####.#.#####.#.####",
      "#.#.#####...#.........#.#.....#.#.....#.#....#",
      "#.#.....#.#.###########.#####.#.#####.#.####.#",
      "#.#####.#.#.....*.....#.....#.#...X.#.#....#.#",
      "#.....#.#.#####.*.###.#####.#####.#.#.####.#.#",
      "#####.#.#.......*.#.....X...#.....#.#....#.#.#",
      "#.....#.###########.#######.#.#####.####.#.#.#",
      "#.###.#.....#.....#.#.....#.#.#.........X#.#.#",
      "#.#...#####.#.###.#.#.###*#.#.#.#########.#.#",
      "#.#.#.....#.#.#X..#...#.#...#...#.........#.#",
      "#.#.#.###.#.#.#.#####.#.#####.###.#########.#",
      "#.#.#.#...#.#.#.....#.#.....#.#...#.........#",
      "#.#.#.#.###.#.#####.#.#####.#.#.###.#########",
      "#...#.#.....#.......*.....#.#.#.....*.......#",
      "#.###.#######.#####.*.###.#.#.#######.#####.#",
      "#.#...........#...#.*.#...#.#.......#.#...#.#",
      "#.#############.#.#.#.#.###.#######.#.#.#.#.#",
      "#...............#...#.#.............#...#...#",
      "###############################################",
  };

  m_maze.loadFromString(mazeMap);

  // 创建玩家
  m_player = std::make_unique<Tank>();

  // 加载玩家坦克纹理
  if (!m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                              "tank_assets/PNG/Weapon_Color_A/Gun_01.png"))
  {
    return false;
  }

  // 设置玩家到起点
  m_player->setPosition(m_maze.getStartPosition());

  // 加载子弹纹理
  if (!m_bulletTexture.loadFromFile("tank_assets/PNG/Effects/Medium_Shell.png"))
  {
    return false;
  }

  m_bulletManager.setTexture(m_bulletTexture);

  // 在指定位置生成敌人
  spawnEnemies();

  return true;
}

void Game::spawnEnemies()
{
  m_enemies.clear();
  const auto &spawnPoints = m_maze.getEnemySpawnPoints();

  for (const auto &pos : spawnPoints)
  {
    auto enemy = std::make_unique<Enemy>();
    if (enemy->loadTextures("tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                            "tank_assets/PNG/Weapon_Color_B/Gun_01.png"))
    {
      enemy->setPosition(pos);
      enemy->setBounds(m_maze.getSize());
      m_enemies.push_back(std::move(enemy));
    }
  }
}

void Game::resetGame()
{
  m_gameOver = false;
  m_gameWon = false;
  m_enemies.clear();

  m_player = std::make_unique<Tank>();
  m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                         "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
  m_player->setPosition(m_maze.getStartPosition());
  spawnEnemies();
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

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);

  // 保存旧位置用于碰撞检测
  sf::Vector2f oldPos = m_player->getPosition();

  // 获取移动向量
  sf::Vector2f movement = m_player->getMovement(dt);

  // 更新玩家
  m_player->update(dt, mouseWorldPos);

  // 检查玩家与墙壁的碰撞并实现墙壁滑动
  sf::Vector2f newPos = m_player->getPosition();
  float radius = m_player->getCollisionRadius();

  if (m_maze.checkCollision(newPos, radius))
  {
    // 尝试只在 X 方向移动
    sf::Vector2f posX = {oldPos.x + movement.x, oldPos.y};
    // 尝试只在 Y 方向移动
    sf::Vector2f posY = {oldPos.x, oldPos.y + movement.y};

    bool canMoveX = !m_maze.checkCollision(posX, radius);
    bool canMoveY = !m_maze.checkCollision(posY, radius);

    if (canMoveX && canMoveY)
    {
      // 两个方向都能走，选择移动量大的
      if (std::abs(movement.x) > std::abs(movement.y))
        m_player->setPosition(posX);
      else
        m_player->setPosition(posY);
    }
    else if (canMoveX)
    {
      m_player->setPosition(posX);
    }
    else if (canMoveY)
    {
      m_player->setPosition(posY);
    }
    else
    {
      // 两个方向都不能走，退回原位
      m_player->setPosition(oldPos);
    }
  }

  // 检查是否到达出口
  if (m_maze.isAtExit(m_player->getPosition(), m_player->getCollisionRadius()))
  {
    m_gameWon = true;
    m_gameOver = true;
  }

  // 更新视角
  updateCamera();

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
    // 检查激活
    enemy->checkActivation(m_player->getPosition());

    enemy->setTarget(m_player->getPosition());
    enemy->update(dt, m_maze);

    // 只有激活的敌人才射击
    if (enemy->shouldShoot())
    {
      m_bulletManager.spawn(enemy->getGunPosition(),
                            enemy->getTurretAngle(),
                            m_bulletSpeed * 0.8f,
                            BulletOwner::Enemy,
                            5.f); // 敌人伤害较低
    }
  }

  // 更新迷宫
  m_maze.update(dt);

  // 更新子弹（使用迷宫大小作为边界）
  sf::Vector2f mazeSize = m_maze.getSize();
  m_bulletManager.update(dt, mazeSize.x, mazeSize.y);

  // 检查碰撞
  checkCollisions();

  // 移除死亡的敌人
  m_enemies.erase(
      std::remove_if(m_enemies.begin(), m_enemies.end(),
                     [](const std::unique_ptr<Enemy> &e)
                     { return e->isDead(); }),
      m_enemies.end());

  // 检查玩家是否死亡
  if (m_player->isDead())
  {
    m_gameOver = true;
  }
}

void Game::updateCamera()
{
  if (!m_player)
    return;

  sf::Vector2f playerPos = m_player->getPosition();

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);

  // 计算鼠标与玩家的距离
  sf::Vector2f toMouse = mouseWorldPos - playerPos;
  float mouseDist = std::sqrt(toMouse.x * toMouse.x + toMouse.y * toMouse.y);

  // 根据鼠标距离计算视角偏移量（距离越远偏移越大）
  const float minDist = 50.f;  // 小于此距离不偏移
  const float maxDist = 300.f; // 大于此距离最大偏移
  float distFactor = 0.f;
  if (mouseDist > minDist)
  {
    distFactor = std::min((mouseDist - minDist) / (maxDist - minDist), 1.f);
  }

  // 获取炮塔瞄准方向，视角向瞄准方向偏移
  float turretAngle = m_player->getTurretAngle();
  float angleRad = (turretAngle - 90.f) * 3.14159f / 180.f;
  sf::Vector2f lookDir = {std::cos(angleRad), std::sin(angleRad)};

  // 视角偏移量随鼠标距离变化
  float actualLookAhead = m_cameraLookAhead * distFactor;
  sf::Vector2f cameraTarget = playerPos + lookDir * actualLookAhead;

  // 限制视角不超出迷宫边界
  sf::Vector2f mazeSize = m_maze.getSize();
  float halfWidth = m_screenWidth / 2.f;
  float halfHeight = m_screenHeight / 2.f;

  cameraTarget.x = std::max(halfWidth, std::min(cameraTarget.x, mazeSize.x - halfWidth));
  cameraTarget.y = std::max(halfHeight, std::min(cameraTarget.y, mazeSize.y - halfHeight));

  m_gameView.setCenter(cameraTarget);
}

void Game::checkCollisions()
{
  if (!m_player)
    return;

  // 检查子弹与墙壁的碰撞
  m_bulletManager.checkWallCollision(m_maze);

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
  m_window.clear(sf::Color(30, 30, 30));

  // 使用游戏视图绘制游戏世界
  m_window.setView(m_gameView);

  // 绘制迷宫
  m_maze.draw(m_window);

  // 绘制子弹
  m_bulletManager.draw(m_window);

  // 绘制玩家
  if (m_player)
  {
    m_player->draw(m_window);
  }

  // 绘制敌人
  for (const auto &enemy : m_enemies)
  {
    enemy->draw(m_window);
  }

  // 切换到 UI 视图绘制 UI
  m_window.setView(m_uiView);

  // 绘制玩家 UI（血条）
  if (m_player)
  {
    m_player->drawUI(m_window);
  }

  // 显示游戏结束/胜利信息
  if (m_gameOver)
  {
    sf::Font font;
    // 尝试加载系统字体，如果失败则跳过文字显示
    if (font.openFromFile("/System/Library/Fonts/Helvetica.ttc"))
    {
      sf::Text text(font);
      if (m_gameWon)
      {
        text.setString("YOU WIN!\nPress R to restart");
        text.setFillColor(sf::Color::Green);
      }
      else
      {
        text.setString("GAME OVER\nPress R to restart");
        text.setFillColor(sf::Color::Red);
      }
      text.setCharacterSize(48);
      text.setPosition({m_screenWidth / 2.f - 150.f, m_screenHeight / 2.f - 50.f});
      m_window.draw(text);
    }
  }

  m_window.display();
}
