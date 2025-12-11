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

  // 加载子弹纹理
  if (!m_bulletTexture.loadFromFile("tank_assets/PNG/Effects/Medium_Shell.png"))
  {
    return false;
  }

  m_bulletManager.setTexture(m_bulletTexture);

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
  m_bulletManager.clear();
  m_player.reset();
}

void Game::run()
{
  while (m_window.isOpen())
  {
    float dt = m_clock.restart().asSeconds();
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
          startGame(); // 重新开始
        }
        else if (keyPressed->code == sf::Keyboard::Key::Escape)
        {
          resetGame(); // 返回菜单
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
