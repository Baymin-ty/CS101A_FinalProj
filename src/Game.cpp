#include "Game.hpp"
#include <algorithm>
#include <cmath>

Game::Game()
    : m_window(sf::VideoMode({m_screenWidth, m_screenHeight}), "Tank Maze Game"),
      m_gameView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)})),
      m_uiView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)})),
      m_mazeGenerator(31, 21) // 默认中等尺寸
{
  m_window.setFramerateLimit(60);
}

bool Game::init()
{
  // 加载字体 - 跨平台支持
  bool fontLoaded = false;

#ifdef _WIN32
  // Windows 字体路径
  if (m_font.openFromFile("C:\\Windows\\Fonts\\arial.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("C:\\Windows\\Fonts\\times.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("C:\\Windows\\Fonts\\segoeui.ttf"))
  {
    fontLoaded = true;
  }
#elif __APPLE__
  // macOS 字体路径
  if (m_font.openFromFile("/System/Library/Fonts/Helvetica.ttc"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/System/Library/Fonts/Arial.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/Library/Fonts/Arial.ttf"))
  {
    fontLoaded = true;
  }
#else
  // Linux 字体路径
  if (m_font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/usr/share/fonts/TTF/DejaVuSans.ttf"))
  {
    fontLoaded = true;
  }
  else if (m_font.openFromFile("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"))
  {
    fontLoaded = true;
  }
#endif

  if (!fontLoaded)
  {
    return false;
  }

  // 设置网络回调
  setupNetworkCallbacks();

  return true;
}

void Game::generateRandomMaze()
{
  int width = m_widthOptions[m_widthIndex];
  int height = m_heightOptions[m_heightIndex];
  m_mazeGenerator = MazeGenerator(width, height);

  // 使用菜单中选择的敌人数量
  int enemyCount = m_enemyOptions[m_enemyIndex];
  m_mazeGenerator.setEnemyCount(enemyCount);
  m_mazeGenerator.setDestructibleRatio(0.15f);

  auto mazeMap = m_mazeGenerator.generate();
  m_maze.loadFromString(mazeMap);
}

void Game::startGame()
{
  if (m_useRandomMap)
  {
    generateRandomMaze();
  }
  else
  {
    // 使用固定地图
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
  }

  // 创建玩家
  m_player = std::make_unique<Tank>();

  // 加载玩家坦克纹理
  m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                         "tank_assets/PNG/Weapon_Color_A/Gun_01.png");

  // 设置玩家到起点
  m_player->setPosition(m_maze.getStartPosition());

  // 在指定位置生成敌人
  spawnEnemies();

  m_gameState = GameState::Playing;
  m_gameOver = false;
  m_gameWon = false;
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
  m_gameState = GameState::MainMenu;
  m_gameOver = false;
  m_gameWon = false;
  m_enemies.clear();
  m_bullets.clear();
  m_player.reset();
  m_otherPlayer.reset();
  m_isMultiplayer = false;
  m_isHost = false;
  m_localPlayerReachedExit = false;
  m_otherPlayerReachedExit = false;
  m_roomCode.clear();
  m_connectionStatus = "Enter server IP:";
  m_inputText.clear();
  m_inputMode = InputMode::None;

  // 断开网络连接
  NetworkManager::getInstance().disconnect();
}

void Game::restartMultiplayer()
{
  // 重新开始多人游戏，使用已有的迷宫数据
  if (!m_generatedMazeData.empty()) {
    m_maze.loadFromString(m_generatedMazeData);
  }
  
  // 设置玩家位置
  sf::Vector2f startPos = m_maze.getPlayerStartPosition();
  
  // 重新创建本地玩家
  m_player = std::make_unique<Tank>();
  m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                         "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
  m_player->setPosition(startPos);
  m_player->setScale(m_tankScale);
  
  // 重新创建其他玩家
  m_otherPlayer = std::make_unique<Tank>();
  m_otherPlayer->loadTextures("tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                               "tank_assets/PNG/Weapon_Color_B/Gun_01.png");
  m_otherPlayer->setPosition(startPos);
  m_otherPlayer->setScale(m_tankScale);
  
  // 重置状态
  m_localPlayerReachedExit = false;
  m_otherPlayerReachedExit = false;
  m_gameOver = false;
  m_gameWon = false;
  m_bullets.clear();
  
  // 初始化相机位置
  m_gameView.setCenter(startPos);
  
  m_gameState = GameState::Multiplayer;
}

void Game::run()
{
  while (m_window.isOpen())
  {
    float dt = m_clock.restart().asSeconds();

    // 处理网络消息
    NetworkManager::getInstance().update();

    processEvents();

    switch (m_gameState)
    {
    case GameState::MainMenu:
      // 菜单不需要 update
      break;
    case GameState::Playing:
      update(dt);
      break;
    case GameState::Paused:
      // 暂停状态不需要 update
      break;
    case GameState::Connecting:
    case GameState::WaitingForPlayer:
      // 等待状态不需要 update
      break;
    case GameState::Multiplayer:
      updateMultiplayer(dt);
      break;
    case GameState::GameOver:
    case GameState::Victory:
      // 游戏结束/胜利状态不需要 update
      break;
    }

    render();
  }
}

void Game::processMenuEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    int optionCount = static_cast<int>(MenuOption::Count);

    switch (keyPressed->code)
    {
    case sf::Keyboard::Key::Up:
    case sf::Keyboard::Key::W:
    {
      int current = static_cast<int>(m_selectedOption);
      current = (current - 1 + optionCount) % optionCount;
      m_selectedOption = static_cast<MenuOption>(current);
      break;
    }
    case sf::Keyboard::Key::Down:
    case sf::Keyboard::Key::S:
    {
      int current = static_cast<int>(m_selectedOption);
      current = (current + 1) % optionCount;
      m_selectedOption = static_cast<MenuOption>(current);
      break;
    }
    case sf::Keyboard::Key::Enter:
    case sf::Keyboard::Key::Space:
      switch (m_selectedOption)
      {
      case MenuOption::StartGame:
        startGame();
        break;
      case MenuOption::Multiplayer:
        m_gameState = GameState::Connecting;
        m_inputText = m_serverIP;
        m_inputMode = InputMode::ServerIP;
        break;
      case MenuOption::ToggleRandomMap:
        m_useRandomMap = !m_useRandomMap;
        break;
      case MenuOption::MapWidth:
        m_widthIndex = (m_widthIndex + 1) % m_widthOptions.size();
        break;
      case MenuOption::MapHeight:
        m_heightIndex = (m_heightIndex + 1) % m_heightOptions.size();
        break;
      case MenuOption::EnemyCount:
        m_enemyIndex = (m_enemyIndex + 1) % m_enemyOptions.size();
        break;
      case MenuOption::Exit:
        m_window.close();
        break;
      default:
        break;
      }
      break;
    case sf::Keyboard::Key::Left:
    case sf::Keyboard::Key::A:
      switch (m_selectedOption)
      {
      case MenuOption::MapWidth:
        m_widthIndex = (m_widthIndex - 1 + m_widthOptions.size()) % m_widthOptions.size();
        break;
      case MenuOption::MapHeight:
        m_heightIndex = (m_heightIndex - 1 + m_heightOptions.size()) % m_heightOptions.size();
        break;
      case MenuOption::EnemyCount:
        m_enemyIndex = (m_enemyIndex - 1 + m_enemyOptions.size()) % m_enemyOptions.size();
        break;
      default:
        break;
      }
      break;
    case sf::Keyboard::Key::Right:
    case sf::Keyboard::Key::D:
      switch (m_selectedOption)
      {
      case MenuOption::MapWidth:
        m_widthIndex = (m_widthIndex + 1) % m_widthOptions.size();
        break;
      case MenuOption::MapHeight:
        m_heightIndex = (m_heightIndex + 1) % m_heightOptions.size();
        break;
      case MenuOption::EnemyCount:
        m_enemyIndex = (m_enemyIndex + 1) % m_enemyOptions.size();
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
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

    switch (m_gameState)
    {
    case GameState::MainMenu:
      processMenuEvents(*event);
      break;

    case GameState::Playing:
      if (m_player)
      {
        m_player->handleInput(*event);
      }
      // 按 ESC 返回菜单，按 P 暂停
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          resetGame();
        }
        else if (keyPressed->code == sf::Keyboard::Key::P)
        {
          m_gameState = GameState::Paused;
        }
      }
      break;

    case GameState::Paused:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::P ||
            keyPressed->code == sf::Keyboard::Key::Escape)
        {
          m_gameState = GameState::Playing;
        }
        else if (keyPressed->code == sf::Keyboard::Key::Q)
        {
          resetGame(); // 返回菜单
        }
      }
      break;

    case GameState::GameOver:
    case GameState::Victory:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::R)
        {
          if (m_isMultiplayer) {
            restartMultiplayer(); // 多人模式重新开始
          } else {
            startGame(); // 单机模式重新开始
          }
        }
        else if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          resetGame(); // 返回菜单
        }
      }
      break;

    case GameState::Connecting:
      processConnectingEvents(*event);
      break;

    case GameState::WaitingForPlayer:
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          NetworkManager::getInstance().disconnect();
          resetGame();
        }
      }
      break;

    case GameState::Multiplayer:
      if (m_player)
      {
        m_player->handleInput(*event);
      }
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          NetworkManager::getInstance().disconnect();
          resetGame();
        }
      }
      break;
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
    m_gameState = GameState::Victory;
  }

  // 更新视角
  updateCamera();

  // 玩家射击
  if (m_player->hasFiredBullet())
  {
    sf::Vector2f bulletPos = m_player->getBulletSpawnPosition();
    float bulletAngle = m_player->getTurretRotation();
    m_bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, true));
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
      sf::Vector2f bulletPos = enemy->getGunPosition();
      float bulletAngle = enemy->getTurretAngle();
      m_bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, false, sf::Color::Red));
    }
  }

  // 更新迷宫
  m_maze.update(dt);

  // 更新子弹
  for (auto &bullet : m_bullets)
  {
    bullet->update(dt);
  }

  // 删除超出范围的子弹
  sf::Vector2f mazeSize = m_maze.getSize();
  m_bullets.erase(
      std::remove_if(m_bullets.begin(), m_bullets.end(),
                     [&mazeSize](const std::unique_ptr<Bullet> &b)
                     {
                       if (!b->isAlive())
                         return true;
                       sf::Vector2f pos = b->getPosition();
                       return pos.x < -50 || pos.x > mazeSize.x + 50 ||
                              pos.y < -50 || pos.y > mazeSize.y + 50;
                     }),
      m_bullets.end());

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
    m_gameState = GameState::GameOver;
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

  // 检查子弹与墙壁、玩家、敌人的碰撞
  for (auto &bullet : m_bullets)
  {
    if (!bullet->isAlive())
      continue;

    sf::Vector2f bulletPos = bullet->getPosition();

    // 检查与墙壁碰撞
    if (m_maze.bulletHit(bulletPos, bullet->getDamage()))
    {
      bullet->setInactive();
      continue;
    }

    // 检查与玩家的碰撞（敌人子弹）
    if (bullet->getOwner() == BulletOwner::Enemy)
    {
      float dist = std::hypot(bulletPos.x - m_player->getPosition().x,
                               bulletPos.y - m_player->getPosition().y);
      if (dist < m_player->getCollisionRadius() + 5.f)
      {
        m_player->takeDamage(bullet->getDamage());
        bullet->setInactive();
        continue;
      }
    }

    // 检查与敌人的碰撞（玩家子弹）
    if (bullet->getOwner() == BulletOwner::Player)
    {
      for (auto &enemy : m_enemies)
      {
        float dist = std::hypot(bulletPos.x - enemy->getPosition().x,
                                 bulletPos.y - enemy->getPosition().y);
        if (dist < enemy->getCollisionRadius() + 5.f)
        {
          enemy->takeDamage(bullet->getDamage());
          bullet->setInactive();
          break;
        }
      }
    }
  }

  // 删除无效子弹
  m_bullets.erase(
      std::remove_if(m_bullets.begin(), m_bullets.end(),
                     [](const std::unique_ptr<Bullet> &b)
                     { return !b->isAlive(); }),
      m_bullets.end());
}

void Game::checkMultiplayerCollisions()
{
  if (!m_player || !m_otherPlayer)
    return;

  // 检查子弹碰撞
  for (auto &bullet : m_bullets)
  {
    if (!bullet->isAlive())
      continue;

    sf::Vector2f bulletPos = bullet->getPosition();

    // 检查与墙壁碰撞（可拆墙）
    if (m_maze.bulletHit(bulletPos, bullet->getDamage()))
    {
      bullet->setInactive();
      continue;
    }

    // 检查与本地玩家的碰撞（对方子弹，BulletOwner::Enemy代表对方玩家）
    if (bullet->getOwner() == BulletOwner::Enemy)
    {
      float dist = std::hypot(bulletPos.x - m_player->getPosition().x,
                               bulletPos.y - m_player->getPosition().y);
      if (dist < m_player->getCollisionRadius() + 5.f)
      {
        m_player->takeDamage(bullet->getDamage());
        bullet->setInactive();
        continue;
      }
    }
    
    // 检查与对方玩家的碰撞（本地玩家子弹，BulletOwner::Player）
    // 注意：这是本地模拟，实际判定应该由对方客户端处理
    // 这里只是为了显示效果
    if (bullet->getOwner() == BulletOwner::Player)
    {
      float dist = std::hypot(bulletPos.x - m_otherPlayer->getPosition().x,
                               bulletPos.y - m_otherPlayer->getPosition().y);
      if (dist < m_otherPlayer->getCollisionRadius() + 5.f)
      {
        // 对方玩家被击中的效果可以在这里处理
        // 但实际伤害判定应该由对方客户端处理
        bullet->setInactive();
        continue;
      }
    }
  }
}

void Game::render()
{
  m_window.clear(sf::Color(30, 30, 30));

  switch (m_gameState)
  {
  case GameState::MainMenu:
    renderMenu();
    break;
  case GameState::Playing:
    renderGame();
    break;
  case GameState::Paused:
    renderGame();
    renderPaused();
    break;
  case GameState::Connecting:
    renderConnecting();
    return; // renderConnecting 已经调用了 display
  case GameState::WaitingForPlayer:
    renderWaitingForPlayer();
    return; // renderWaitingForPlayer 已经调用了 display
  case GameState::Multiplayer:
    renderMultiplayer();
    return; // renderMultiplayer 已经调用了 display
  case GameState::GameOver:
  case GameState::Victory:
    renderGame();
    renderGameOver();
    break;
  }

  m_window.display();
}

void Game::renderMenu()
{
  m_window.setView(m_uiView);

  // 标题
  sf::Text title(m_font);
  title.setString("TANK MAZE");
  title.setCharacterSize(72);
  title.setFillColor(sf::Color::White);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(m_screenWidth - titleBounds.size.x) / 2.f, 80.f});
  m_window.draw(title);

  // 菜单选项
  float startY = 200.f;
  float spacing = 50.f;

  std::vector<std::string> options = {
      "Start Game",
      "Multiplayer",
      std::string("Random Map: ") + (m_useRandomMap ? "ON" : "OFF"),
      std::string("Map Width: < ") + std::to_string(m_widthOptions[m_widthIndex]) + " >",
      std::string("Map Height: < ") + std::to_string(m_heightOptions[m_heightIndex]) + " >",
      std::string("Enemies: < ") + std::to_string(m_enemyOptions[m_enemyIndex]) + " >",
      "Exit"};

  for (size_t i = 0; i < options.size(); ++i)
  {
    sf::Text optionText(m_font);
    optionText.setString(options[i]);
    optionText.setCharacterSize(32);

    if (static_cast<int>(i) == static_cast<int>(m_selectedOption))
    {
      optionText.setFillColor(sf::Color::Yellow);
      optionText.setString("> " + options[i] + " <");
    }
    else
    {
      optionText.setFillColor(sf::Color(180, 180, 180));
    }

    sf::FloatRect bounds = optionText.getLocalBounds();
    optionText.setPosition({(m_screenWidth - bounds.size.x) / 2.f, startY + i * spacing});
    m_window.draw(optionText);
  }

  // 地图预览信息
  sf::Text mapInfo(m_font);
  int totalCells = m_widthOptions[m_widthIndex] * m_heightOptions[m_heightIndex];
  mapInfo.setString("Map: " + std::to_string(m_widthOptions[m_widthIndex]) + " x " +
                    std::to_string(m_heightOptions[m_heightIndex]) + " = " +
                    std::to_string(totalCells) + " cells");
  mapInfo.setCharacterSize(20);
  mapInfo.setFillColor(sf::Color(100, 180, 100));
  sf::FloatRect mapInfoBounds = mapInfo.getLocalBounds();
  mapInfo.setPosition({(m_screenWidth - mapInfoBounds.size.x) / 2.f, m_screenHeight - 120.f});
  m_window.draw(mapInfo);

  // 提示
  sf::Text hint(m_font);
  hint.setString("W/S: Navigate | A/D or Left/Right: Adjust values | Enter: Select");
  hint.setCharacterSize(18);
  hint.setFillColor(sf::Color(120, 120, 120));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(m_screenWidth - hintBounds.size.x) / 2.f, m_screenHeight - 60.f});
  m_window.draw(hint);
}

void Game::renderGame()
{
  // 使用游戏视图绘制游戏世界
  m_window.setView(m_gameView);

  // 绘制迷宫
  m_maze.draw(m_window);

  // 绘制子弹
  for (const auto &bullet : m_bullets)
  {
    bullet->draw(m_window);
  }

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
}

void Game::renderPaused()
{
  m_window.setView(m_uiView);

  // 半透明背景
  sf::RectangleShape overlay({static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)});
  overlay.setFillColor(sf::Color(0, 0, 0, 180));
  m_window.draw(overlay);

  // 暂停标题
  sf::Text title(m_font);
  title.setString("PAUSED");
  title.setCharacterSize(72);
  title.setFillColor(sf::Color::Yellow);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(m_screenWidth - titleBounds.size.x) / 2.f, m_screenHeight / 2.f - 100.f});
  m_window.draw(title);

  // 提示信息
  sf::Text hint(m_font);
  hint.setString("Press P or ESC to resume\nPress Q to quit to menu");
  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(m_screenWidth - hintBounds.size.x) / 2.f, m_screenHeight / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::renderGameOver()
{
  m_window.setView(m_uiView);

  // 半透明背景
  sf::RectangleShape overlay({static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight)});
  overlay.setFillColor(sf::Color(0, 0, 0, 150));
  m_window.draw(overlay);

  sf::Text text(m_font);
  if (m_gameWon)
  {
    text.setString("YOU WIN!");
    text.setFillColor(sf::Color::Green);
  }
  else
  {
    text.setString("GAME OVER");
    text.setFillColor(sf::Color::Red);
  }
  text.setCharacterSize(64);
  sf::FloatRect bounds = text.getLocalBounds();
  text.setPosition({(m_screenWidth - bounds.size.x) / 2.f, m_screenHeight / 2.f - 80.f});
  m_window.draw(text);

  sf::Text hint(m_font);
  hint.setString("Press R to restart, ESC for menu");
  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(m_screenWidth - hintBounds.size.x) / 2.f, m_screenHeight / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::setupNetworkCallbacks()
{
  auto& net = NetworkManager::getInstance();
  
  net.setOnConnected([this]() {
    m_connectionStatus = "Connected! Choose action:";
  });
  
  net.setOnDisconnected([this]() {
    if (m_gameState == GameState::Multiplayer || 
        m_gameState == GameState::WaitingForPlayer ||
        m_gameState == GameState::Connecting) {
      m_connectionStatus = "Disconnected from server";
      resetGame();
    }
  });
  
  net.setOnRoomCreated([this](const std::string& roomCode) {
    m_roomCode = roomCode;
    m_isHost = true;
    m_gameState = GameState::WaitingForPlayer;
    m_connectionStatus = "Room created! Code: " + roomCode;
    
    // 房主立即生成迷宫并发送
    m_maze.generateRandomMaze(m_mazeWidth, m_mazeHeight, 0);
    m_generatedMazeData = m_maze.getMazeData();
    NetworkManager::getInstance().sendMazeData(m_generatedMazeData);
  });
  
  net.setOnRoomJoined([this](const std::string& roomCode) {
    m_roomCode = roomCode;
    m_isHost = false;
    m_gameState = GameState::WaitingForPlayer;
    m_connectionStatus = "Joined room: " + roomCode + " - Waiting for maze...";
  });
  
  net.setOnMazeData([this](const std::vector<std::string>& mazeData) {
    // 收到迷宫数据（非房主）
    m_generatedMazeData = mazeData;
    m_connectionStatus = "Maze received! Waiting for game start...";
  });
  
  net.setOnRequestMaze([this]() {
    // 服务器请求迷宫数据（房主收到）
    if (m_isHost && !m_generatedMazeData.empty()) {
      NetworkManager::getInstance().sendMazeData(m_generatedMazeData);
    }
  });
  
  net.setOnGameStart([this]() {
    m_isMultiplayer = true;
    
    // 使用已接收/生成的迷宫数据
    if (!m_generatedMazeData.empty()) {
      m_maze.loadFromString(m_generatedMazeData);
    }
    
    // 设置玩家位置
    sf::Vector2f startPos = m_maze.getPlayerStartPosition();
    
    // 创建本地玩家并加载贴图（与单机模式一致）
    m_player = std::make_unique<Tank>();
    m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                           "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
    m_player->setPosition(startPos);
    m_player->setScale(m_tankScale);
    
    // 设置第二个玩家（另一个客户端）- 使用不同颜色贴图
    m_otherPlayer = std::make_unique<Tank>();
    m_otherPlayer->loadTextures("tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                                "tank_assets/PNG/Weapon_Color_B/Gun_01.png");
    m_otherPlayer->setPosition(startPos);
    m_otherPlayer->setScale(m_tankScale);
    
    m_localPlayerReachedExit = false;
    m_otherPlayerReachedExit = false;
    
    // 多人模式没有敌人
    m_enemies.clear();
    m_bullets.clear();
    
    // 初始化相机位置
    m_gameView.setCenter(startPos);
    
    m_gameState = GameState::Multiplayer;
  });
  
  net.setOnPlayerUpdate([this](const PlayerState& state) {
    if (m_otherPlayer) {
      m_otherPlayer->setPosition({state.x, state.y});
      m_otherPlayer->setRotation(state.rotation);
      m_otherPlayer->setTurretRotation(state.turretAngle);
      m_otherPlayerReachedExit = state.reachedExit;
      
      // 检查是否双方都到达终点
      if (m_localPlayerReachedExit && m_otherPlayerReachedExit) {
        m_gameWon = true;
        m_gameState = GameState::Victory;
      }
    }
  });
  
  net.setOnPlayerShoot([this](float x, float y, float angle) {
    // 创建另一个玩家的子弹
    m_bullets.push_back(std::make_unique<Bullet>(x, y, angle, false, sf::Color::Cyan));
  });
  
  net.setOnError([this](const std::string& error) {
    m_connectionStatus = "Error: " + error;
  });
}

void Game::processConnectingEvents(const sf::Event& event)
{
  if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
    if (keyPressed->code == sf::Keyboard::Key::Escape) {
      NetworkManager::getInstance().disconnect();
      resetGame();
      return;
    }
    
    if (keyPressed->code == sf::Keyboard::Key::Enter) {
      switch (m_inputMode) {
        case InputMode::ServerIP:
          m_serverIP = m_inputText;
          if (NetworkManager::getInstance().connect(m_serverIP, 9999)) {
            m_connectionStatus = "Connected! Enter room code or press C to create:";
            m_inputMode = InputMode::RoomCode;
            m_inputText = "";
          } else {
            m_connectionStatus = "Failed to connect to " + m_serverIP;
          }
          break;
        case InputMode::RoomCode:
          if (!m_inputText.empty()) {
            NetworkManager::getInstance().joinRoom(m_inputText);
          }
          break;
        default:
          break;
      }
    }
    
    // 按 C 创建房间
    if (keyPressed->code == sf::Keyboard::Key::C && 
        m_inputMode == InputMode::RoomCode &&
        NetworkManager::getInstance().isConnected()) {
      NetworkManager::getInstance().createRoom(m_mazeWidth, m_mazeHeight);
    }
    
    // 退格键
    if (keyPressed->code == sf::Keyboard::Key::Backspace && !m_inputText.empty()) {
      m_inputText.pop_back();
    }
  }
  
  // 文本输入
  if (const auto* textEntered = event.getIf<sf::Event::TextEntered>()) {
    if (textEntered->unicode >= 32 && textEntered->unicode < 127) {
      m_inputText += static_cast<char>(textEntered->unicode);
    }
  }
}

void Game::updateMultiplayer(float dt)
{
  if (!m_player) return;
  
  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(m_window);
  sf::Vector2f mouseWorldPos = m_window.mapPixelToCoords(mousePixelPos, m_gameView);
  
  // 保存旧位置
  sf::Vector2f oldPos = m_player->getPosition();
  sf::Vector2f movement = m_player->getMovement(dt);
  
  // 更新本地玩家
  m_player->update(dt, mouseWorldPos);
  
  // 碰撞检测（与单人模式相同）
  sf::Vector2f newPos = m_player->getPosition();
  float radius = m_player->getCollisionRadius();
  
  if (m_maze.checkCollision(newPos, radius)) {
    sf::Vector2f posX = {oldPos.x + movement.x, oldPos.y};
    sf::Vector2f posY = {oldPos.x, oldPos.y + movement.y};
    
    bool canMoveX = !m_maze.checkCollision(posX, radius);
    bool canMoveY = !m_maze.checkCollision(posY, radius);
    
    if (canMoveX && canMoveY) {
      if (std::abs(movement.x) > std::abs(movement.y))
        m_player->setPosition(posX);
      else
        m_player->setPosition(posY);
    } else if (canMoveX) {
      m_player->setPosition(posX);
    } else if (canMoveY) {
      m_player->setPosition(posY);
    } else {
      m_player->setPosition(oldPos);
    }
  }
  
  // 发送位置到服务器
  auto& net = NetworkManager::getInstance();
  PlayerState state;
  state.x = m_player->getPosition().x;
  state.y = m_player->getPosition().y;
  state.rotation = m_player->getRotation();
  state.turretAngle = m_player->getTurretRotation();
  state.health = m_player->getHealth();
  state.reachedExit = m_localPlayerReachedExit;
  net.sendPosition(state);
  
  // 处理射击
  if (m_player->hasFiredBullet()) {
    sf::Vector2f bulletPos = m_player->getBulletSpawnPosition();
    float bulletAngle = m_player->getTurretRotation();
    m_bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, true));
    net.sendShoot(bulletPos.x, bulletPos.y, bulletAngle);
  }
  
  // 更新迷宫（可拆墙动画等）
  m_maze.update(dt);
  
  // 更新子弹
  for (auto& bullet : m_bullets) {
    bullet->update(dt);
  }
  
  // 子弹碰撞检测（与单人模式类似）
  checkMultiplayerCollisions();
  
  // 删除超出范围的子弹
  m_bullets.erase(
    std::remove_if(m_bullets.begin(), m_bullets.end(),
      [](const std::unique_ptr<Bullet>& b) { return !b->isAlive(); }),
    m_bullets.end());
  
  // 检查玩家是否到达终点
  sf::Vector2f exitPos = m_maze.getExitPosition();
  float distToExit = std::hypot(m_player->getPosition().x - exitPos.x,
                                 m_player->getPosition().y - exitPos.y);
  
  if (distToExit < TILE_SIZE && !m_localPlayerReachedExit) {
    m_localPlayerReachedExit = true;
    net.sendReachExit();
    
    // 检查是否双方都到达终点
    if (m_otherPlayerReachedExit) {
      m_gameWon = true;
      m_gameState = GameState::Victory;
    }
  }
  
  // 检查玩家是否死亡
  if (m_player->isDead()) {
    m_gameOver = true;
    m_gameState = GameState::GameOver;
  }
  
  // 更新相机
  sf::Vector2f playerPos = m_player->getPosition();
  m_gameView.setCenter(playerPos);
}

void Game::renderConnecting()
{
  m_window.setView(m_uiView);
  m_window.clear(sf::Color(30, 30, 50));
  
  sf::Text title(m_font);
  title.setString("Multiplayer");
  title.setCharacterSize(48);
  title.setFillColor(sf::Color::White);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(m_screenWidth - titleBounds.size.x) / 2.f, 80.f});
  m_window.draw(title);
  
  sf::Text status(m_font);
  status.setString(m_connectionStatus);
  status.setCharacterSize(24);
  status.setFillColor(sf::Color::Yellow);
  sf::FloatRect statusBounds = status.getLocalBounds();
  status.setPosition({(m_screenWidth - statusBounds.size.x) / 2.f, 180.f});
  m_window.draw(status);
  
  sf::Text inputLabel(m_font);
  if (m_inputMode == InputMode::ServerIP) {
    inputLabel.setString("Server IP:");
  } else {
    inputLabel.setString("Room Code (or press C to create):");
  }
  inputLabel.setCharacterSize(24);
  inputLabel.setFillColor(sf::Color::White);
  sf::FloatRect labelBounds = inputLabel.getLocalBounds();
  inputLabel.setPosition({(m_screenWidth - labelBounds.size.x) / 2.f, 260.f});
  m_window.draw(inputLabel);
  
  // 输入框背景
  sf::RectangleShape inputBox({400.f, 50.f});
  inputBox.setFillColor(sf::Color(50, 50, 70));
  inputBox.setOutlineColor(sf::Color::White);
  inputBox.setOutlineThickness(2.f);
  inputBox.setPosition({(m_screenWidth - 400.f) / 2.f, 300.f});
  m_window.draw(inputBox);
  
  sf::Text inputText(m_font);
  inputText.setString(m_inputText + "_");
  inputText.setCharacterSize(28);
  inputText.setFillColor(sf::Color::White);
  inputText.setPosition({(m_screenWidth - 400.f) / 2.f + 10.f, 308.f});
  m_window.draw(inputText);
  
  sf::Text hint(m_font);
  hint.setString("Press ENTER to confirm, ESC to cancel");
  hint.setCharacterSize(20);
  hint.setFillColor(sf::Color(150, 150, 150));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(m_screenWidth - hintBounds.size.x) / 2.f, 400.f});
  m_window.draw(hint);
  
  m_window.display();
}

void Game::renderWaitingForPlayer()
{
  m_window.setView(m_uiView);
  m_window.clear(sf::Color(30, 30, 50));
  
  sf::Text title(m_font);
  title.setString("Waiting for Player");
  title.setCharacterSize(48);
  title.setFillColor(sf::Color::White);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(m_screenWidth - titleBounds.size.x) / 2.f, 80.f});
  m_window.draw(title);
  
  sf::Text roomInfo(m_font);
  roomInfo.setString("Room Code: " + m_roomCode);
  roomInfo.setCharacterSize(36);
  roomInfo.setFillColor(sf::Color::Green);
  sf::FloatRect roomBounds = roomInfo.getLocalBounds();
  roomInfo.setPosition({(m_screenWidth - roomBounds.size.x) / 2.f, 200.f});
  m_window.draw(roomInfo);
  
  sf::Text hint(m_font);
  hint.setString("Share this code with your friend!");
  hint.setCharacterSize(24);
  hint.setFillColor(sf::Color::Yellow);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(m_screenWidth - hintBounds.size.x) / 2.f, 280.f});
  m_window.draw(hint);
  
  // 动画点
  static float dotTime = 0;
  dotTime += 0.016f;
  int dots = static_cast<int>(dotTime * 2) % 4;
  std::string waiting = "Waiting";
  for (int i = 0; i < dots; i++) waiting += ".";
  
  sf::Text waitText(m_font);
  waitText.setString(waiting);
  waitText.setCharacterSize(28);
  waitText.setFillColor(sf::Color::White);
  sf::FloatRect waitBounds = waitText.getLocalBounds();
  waitText.setPosition({(m_screenWidth - waitBounds.size.x) / 2.f, 360.f});
  m_window.draw(waitText);
  
  sf::Text escHint(m_font);
  escHint.setString("Press ESC to cancel");
  escHint.setCharacterSize(20);
  escHint.setFillColor(sf::Color(150, 150, 150));
  sf::FloatRect escBounds = escHint.getLocalBounds();
  escHint.setPosition({(m_screenWidth - escBounds.size.x) / 2.f, 450.f});
  m_window.draw(escHint);
  
  m_window.display();
}

void Game::renderMultiplayer()
{
  m_window.clear(sf::Color(30, 30, 30));
  m_window.setView(m_gameView);
  
  // 渲染迷宫
  m_maze.render(m_window);
  
  // 渲染终点
  sf::Vector2f exitPos = m_maze.getExitPosition();
  sf::RectangleShape exitMarker({TILE_SIZE * 0.8f, TILE_SIZE * 0.8f});
  exitMarker.setFillColor(sf::Color(0, 255, 0, 100));
  exitMarker.setOutlineColor(sf::Color::Green);
  exitMarker.setOutlineThickness(3.f);
  exitMarker.setPosition({exitPos.x - TILE_SIZE * 0.4f, exitPos.y - TILE_SIZE * 0.4f});
  m_window.draw(exitMarker);
  
  // 渲染另一个玩家
  if (m_otherPlayer) {
    m_otherPlayer->render(m_window);
    
    // 如果另一个玩家到达终点，显示标记
    if (m_otherPlayerReachedExit) {
      sf::CircleShape marker(15.f);
      marker.setFillColor(sf::Color(0, 255, 0, 150));
      marker.setPosition({m_otherPlayer->getPosition().x - 15.f, 
                          m_otherPlayer->getPosition().y - 40.f});
      m_window.draw(marker);
    }
  }
  
  // 渲染本地玩家
  if (m_player) {
    m_player->render(m_window);
    
    // 如果本地玩家到达终点，显示标记
    if (m_localPlayerReachedExit) {
      sf::CircleShape marker(15.f);
      marker.setFillColor(sf::Color(0, 255, 0, 150));
      marker.setPosition({m_player->getPosition().x - 15.f, 
                          m_player->getPosition().y - 40.f});
      m_window.draw(marker);
    }
  }
  
  // 渲染子弹
  for (const auto& bullet : m_bullets) {
    bullet->render(m_window);
  }
  
  // UI
  m_window.setView(m_uiView);
  
  // 显示到达终点状态
  sf::Text statusText(m_font);
  std::string status = "You: " + std::string(m_localPlayerReachedExit ? "DONE" : "---");
  status += "  |  Partner: " + std::string(m_otherPlayerReachedExit ? "DONE" : "---");
  statusText.setString(status);
  statusText.setCharacterSize(24);
  statusText.setFillColor(sf::Color::White);
  statusText.setPosition({20.f, 20.f});
  m_window.draw(statusText);
  
  sf::Text goalText(m_font);
  goalText.setString("Both players must reach the green exit!");
  goalText.setCharacterSize(18);
  goalText.setFillColor(sf::Color::Yellow);
  goalText.setPosition({20.f, 50.f});
  m_window.draw(goalText);
  
  m_window.display();
}
