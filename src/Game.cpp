#include "Game.hpp"
#include "include/CollisionSystem.hpp"
#include "include/UIHelper.hpp"
#include "include/MultiplayerHandler.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

Game::Game()
    : m_mazeGenerator(31, 21) // 默认中等尺寸
{
  // 获取桌面尺寸，计算最大窗口大小（保持16:9比例）
  auto desktop = sf::VideoMode::getDesktopMode();
  unsigned int maxWidth = desktop.size.x * 9 / 10; // 留10%边距
  unsigned int maxHeight = desktop.size.y * 9 / 10;

  // 根据宽高比计算实际窗口尺寸
  if (static_cast<float>(maxWidth) / maxHeight > ASPECT_RATIO)
  {
    // 屏幕太宽，以高度为准
    m_screenHeight = maxHeight;
    m_screenWidth = static_cast<unsigned int>(m_screenHeight * ASPECT_RATIO);
  }
  else
  {
    // 屏幕太高，以宽度为准
    m_screenWidth = maxWidth;
    m_screenHeight = static_cast<unsigned int>(m_screenWidth / ASPECT_RATIO);
  }

  // 创建窗口（使用实际尺寸）
  m_window.create(sf::VideoMode({m_screenWidth, m_screenHeight}), "Tank Maze Game");
  m_window.setFramerateLimit(60);

  // 初始化视图 - 使用固定的逻辑分辨率，保证所有屏幕看到的范围相同
  m_gameView = sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)}));
  m_uiView = sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)}));
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

  // 初始化音频系统
  if (!AudioManager::getInstance().init("music_assets/"))
  {
    std::cerr << "Warning: Failed to initialize audio system" << std::endl;
    // 音频初始化失败不阻止游戏运行
  }
  
  // 设置听音范围（基于视野大小）
  AudioManager::getInstance().setListeningRange(LOGICAL_WIDTH * VIEW_ZOOM * 0.6f);

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
  m_exitVisible = false;  // 重置终点可见状态
  
  // 播放游戏开始BGM
  AudioManager::getInstance().playBGM(BGMType::Start);
}

void Game::spawnEnemies()
{
  m_enemies.clear();
  const auto &spawnPoints = m_maze.getEnemySpawnPoints();

  for (const auto &pos : spawnPoints)
  {
    auto enemy = std::make_unique<Enemy>();
    if (enemy->loadTextures("tank_assets/PNG/Hulls_Color_D/Hull_01.png",
                            "tank_assets/PNG/Weapon_Color_D/Gun_01.png"))
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
  m_mpState.multiplayerWin = false;
  m_enemies.clear();
  m_bullets.clear();
  m_player.reset();
  m_otherPlayer.reset();
  m_mpState.isMultiplayer = false;
  m_mpState.isHost = false;
  m_mpState.localPlayerReachedExit = false;
  m_mpState.otherPlayerReachedExit = false;
  m_mpState.roomCode.clear();
  m_mpState.connectionStatus = "Enter server IP:";
  m_inputText.clear();
  m_inputMode = InputMode::None;
  m_mpState.generatedMazeData.clear();

  // 断开网络连接
  NetworkManager::getInstance().disconnect();
}

void Game::restartMultiplayer()
{
  // 重新开始多人游戏，使用已有的迷宫数据
  if (!m_mpState.generatedMazeData.empty())
  {
    m_maze.loadFromString(m_mpState.generatedMazeData);
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
  m_mpState.localPlayerReachedExit = false;
  m_mpState.otherPlayerReachedExit = false;
  m_mpState.multiplayerWin = false;
  m_gameOver = false;
  m_gameWon = false;
  m_exitVisible = false;  // 重置终点可见状态
  m_bullets.clear();

  // 初始化相机位置和缩放
  m_gameView.setCenter(startPos);
  m_gameView.setSize({LOGICAL_WIDTH * VIEW_ZOOM, LOGICAL_HEIGHT * VIEW_ZOOM});
  m_currentCameraPos = startPos;

  m_gameState = GameState::Multiplayer;
  
  // 播放游戏开始BGM
  AudioManager::getInstance().playBGM(BGMType::Start);
}

void Game::run()
{
  // 开始播放菜单BGM
  AudioManager::getInstance().playBGM(BGMType::Menu);
  
  while (m_window.isOpen())
  {
    float dt = m_clock.restart().asSeconds();

    // 处理网络消息
    NetworkManager::getInstance().update();
    
    // 更新音频系统（清理已播放完的音效）
    AudioManager::getInstance().update();

    processEvents();

    switch (m_gameState)
    {
    case GameState::MainMenu:
      // 菜单播放菜单BGM
      if (AudioManager::getInstance().getCurrentBGM() != BGMType::Menu)
      {
        AudioManager::getInstance().playBGM(BGMType::Menu);
      }
      break;
    case GameState::Playing:
      update(dt);
      // 检查是否看到终点，切换BGM
      if (!m_exitVisible && isExitInView())
      {
        m_exitVisible = true;
        AudioManager::getInstance().playBGM(BGMType::Climax);
      }
      break;
    case GameState::Paused:
      // 暂停状态不需要 update
      break;
    case GameState::Connecting:
    case GameState::WaitingForPlayer:
      // 等待状态播放菜单BGM
      if (AudioManager::getInstance().getCurrentBGM() != BGMType::Menu)
      {
        AudioManager::getInstance().playBGM(BGMType::Menu);
      }
      break;
    case GameState::Multiplayer:
      updateMultiplayer(dt);
      // 检查是否看到终点，切换BGM并同步给对方
      if (!m_exitVisible && isExitInView())
      {
        m_exitVisible = true;
        AudioManager::getInstance().playBGM(BGMType::Climax);
        // 通知对方也开始播放高潮BGM
        NetworkManager::getInstance().sendClimaxStart();
      }
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

    // 处理窗口大小变化，保持宽高比
    if (const auto *resized = event->getIf<sf::Event::Resized>())
    {
      handleWindowResize();
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
          if (m_mpState.isMultiplayer)
          {
            if (m_mpState.isHost)
            {
              // 房主按 R：回到等待玩家界面，通知对方准备重新开始
              NetworkManager::getInstance().sendRestartRequest();

              // 重新生成迷宫（使用菜单选择的NPC数量，联机模式）
              int npcCount = m_enemyOptions[m_enemyIndex];
              m_maze.generateRandomMaze(m_mazeWidth, m_mazeHeight, 0, npcCount, true);
              m_mpState.generatedMazeData = m_maze.getMazeData();
              NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData);

              m_gameState = GameState::WaitingForPlayer;
              m_mpState.connectionStatus = "Waiting for other player to restart...";

              // 重置状态
              m_mpState.localPlayerReachedExit = false;
              m_mpState.otherPlayerReachedExit = false;
              m_mpState.multiplayerWin = false;
              m_gameOver = false;
              m_gameWon = false;
              m_bullets.clear();
            }
            else
            {
              // 非房主按 R：自动重新加入上次的房间
              if (!m_mpState.roomCode.empty())
              {
                NetworkManager::getInstance().joinRoom(m_mpState.roomCode);
                m_gameState = GameState::WaitingForPlayer;
                m_mpState.connectionStatus = "Rejoining room: " + m_mpState.roomCode;
              }
              else
              {
                resetGame(); // 没有房间码，返回主菜单
              }
            }
          }
          else
          {
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
        // R键激活NPC
        if (keyPressed->code == sf::Keyboard::Key::R)
        {
          m_mpState.rKeyJustPressed = true;
          std::cout << "[DEBUG] R key event detected!" << std::endl;
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
    
    // 播放射击音效
    AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, m_player->getPosition());
  }

  // 更新敌人
  for (auto &enemy : m_enemies)
  {
    // 单人模式：自动激活检测
    enemy->checkAutoActivation(m_player->getPosition());

    enemy->setTarget(m_player->getPosition());
    enemy->update(dt, m_maze);

    // 只有激活的敌人才射击
    if (enemy->shouldShoot())
    {
      sf::Vector2f bulletPos = enemy->getGunPosition();
      float bulletAngle = enemy->getTurretAngle();
      m_bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, false, sf::Color::Red));
      
      // 播放射击音效（基于玩家位置的距离衰减）
      AudioManager::getInstance().playSFX(SFXType::Shoot, bulletPos, m_player->getPosition());
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
  const float minDist = 100.f; // 小于此距离不偏移
  const float maxDist = 400.f; // 大于此距离最大偏移
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

  // 缩放后的视野范围
  float zoomedWidth = LOGICAL_WIDTH * VIEW_ZOOM;
  float zoomedHeight = LOGICAL_HEIGHT * VIEW_ZOOM;

  // 限制视角不超出迷宫边界（使用缩放后的视野范围）
  sf::Vector2f mazeSize = m_maze.getSize();
  float halfWidth = zoomedWidth / 2.f;
  float halfHeight = zoomedHeight / 2.f;

  cameraTarget.x = std::max(halfWidth, std::min(cameraTarget.x, mazeSize.x - halfWidth));
  cameraTarget.y = std::max(halfHeight, std::min(cameraTarget.y, mazeSize.y - halfHeight));

  // 平滑插值到目标位置（减少晃动感）
  float dt = 1.f / 60.f; // 假设60fps
  float lerpFactor = 1.f - std::exp(-m_cameraSmoothSpeed * dt);

  // 初始化相机位置
  if (m_currentCameraPos.x == 0.f && m_currentCameraPos.y == 0.f)
  {
    m_currentCameraPos = cameraTarget;
  }
  else
  {
    m_currentCameraPos.x += (cameraTarget.x - m_currentCameraPos.x) * lerpFactor;
    m_currentCameraPos.y += (cameraTarget.y - m_currentCameraPos.y) * lerpFactor;
  }

  // 设置视图中心和缩放
  m_gameView.setCenter(m_currentCameraPos);
  m_gameView.setSize({zoomedWidth, zoomedHeight});
}

void Game::checkCollisions()
{
  CollisionSystem::checkSinglePlayerCollisions(m_player.get(), m_enemies, m_bullets, m_maze);
}

void Game::checkMultiplayerCollisions()
{
  CollisionSystem::checkMultiplayerCollisions(
      m_player.get(), m_otherPlayer.get(), m_enemies, m_bullets, m_maze, m_mpState.isHost);
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
  title.setPosition({(LOGICAL_WIDTH - titleBounds.size.x) / 2.f, 80.f});
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
      std::string("NPCs: < ") + std::to_string(m_enemyOptions[m_enemyIndex]) + " >",
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
    optionText.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, startY + i * spacing});
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
  mapInfo.setPosition({(LOGICAL_WIDTH - mapInfoBounds.size.x) / 2.f, LOGICAL_HEIGHT - 120.f});
  m_window.draw(mapInfo);

  // 提示
  sf::Text hint(m_font);
  hint.setString("W/S: Navigate | A/D or Left/Right: Adjust values | Enter: Select");
  hint.setCharacterSize(18);
  hint.setFillColor(sf::Color(120, 120, 120));
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT - 60.f});
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

  // 绘制敌人（跳过死亡的）
  for (const auto &enemy : m_enemies)
  {
    if (!enemy->isDead())
    {
      enemy->draw(m_window);
    }
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
  sf::RectangleShape overlay({static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)});
  overlay.setFillColor(sf::Color(0, 0, 0, 180));
  m_window.draw(overlay);

  // 暂停标题
  sf::Text title(m_font);
  title.setString("PAUSED");
  title.setCharacterSize(72);
  title.setFillColor(sf::Color::Yellow);
  title.setStyle(sf::Text::Bold);
  sf::FloatRect titleBounds = title.getLocalBounds();
  title.setPosition({(LOGICAL_WIDTH - titleBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f - 100.f});
  m_window.draw(title);

  // 提示信息
  sf::Text hint(m_font);
  hint.setString("Press P or ESC to resume\nPress Q to quit to menu");
  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::renderGameOver()
{
  m_window.setView(m_uiView);

  // 半透明背景
  sf::RectangleShape overlay({static_cast<float>(LOGICAL_WIDTH), static_cast<float>(LOGICAL_HEIGHT)});
  overlay.setFillColor(sf::Color(0, 0, 0, 150));
  m_window.draw(overlay);

  sf::Text text(m_font);

  // 根据游戏模式和结果显示不同文本
  if (m_mpState.isMultiplayer)
  {
    if (m_gameState == GameState::Victory)
    {
      text.setString("VICTORY!");
      text.setFillColor(sf::Color::Green);
    }
    else
    {
      text.setString("DEFEATED!");
      text.setFillColor(sf::Color::Red);
    }
  }
  else
  {
    // 单机模式
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
  }

  text.setCharacterSize(64);
  sf::FloatRect bounds = text.getLocalBounds();
  text.setPosition({(LOGICAL_WIDTH - bounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f - 80.f});
  m_window.draw(text);

  sf::Text hint(m_font);

  // 多人模式显示不同的提示
  if (m_mpState.isMultiplayer)
  {
    if (m_mpState.isHost)
    {
      hint.setString("Press R to restart match, ESC for menu");
    }
    else
    {
      hint.setString("Press R to rejoin room, ESC for menu");
    }
  }
  else
  {
    hint.setString("Press R to restart, ESC for menu");
  }

  hint.setCharacterSize(28);
  hint.setFillColor(sf::Color::White);
  sf::FloatRect hintBounds = hint.getLocalBounds();
  hint.setPosition({(LOGICAL_WIDTH - hintBounds.size.x) / 2.f, LOGICAL_HEIGHT / 2.f + 20.f});
  m_window.draw(hint);
}

void Game::setupNetworkCallbacks()
{
  auto &net = NetworkManager::getInstance();

  net.setOnConnected([this]()
                     { m_mpState.connectionStatus = "Connected! Choose action:"; });

  net.setOnDisconnected([this]()
                        {
    if (m_gameState == GameState::Multiplayer || 
        m_gameState == GameState::WaitingForPlayer ||
        m_gameState == GameState::Connecting) {
      m_mpState.connectionStatus = "Disconnected from server";
      resetGame();
    } });

  // 对方看到出口，同步播放高潮BGM
  net.setOnClimaxStart([this]()
                       {
    if (!m_exitVisible) {
      m_exitVisible = true;
      AudioManager::getInstance().playBGM(BGMType::Climax);
    } });

  // 对方玩家离开房间，返回等待状态
  net.setOnPlayerLeft([this](bool becameHost)
                      {
    if (m_gameState == GameState::Multiplayer) {
      // 清理游戏状态
      m_otherPlayer.reset();
      m_enemies.clear();
      m_bullets.clear();
      
      // 重置多人游戏状态
      m_mpState.localPlayerReachedExit = false;
      m_mpState.otherPlayerReachedExit = false;
      m_mpState.multiplayerWin = false;
      m_gameOver = false;
      m_gameWon = false;
      
      // 更新房主状态
      if (becameHost) {
        m_mpState.isHost = true;
      }
      
      // 返回等待玩家状态
      m_gameState = GameState::WaitingForPlayer;
      if (becameHost) {
        m_mpState.connectionStatus = "Other player left. You are now the host. Waiting...";
      } else {
        m_mpState.connectionStatus = "Other player left. Waiting for new player...";
      }
      
      // 房主需要重新生成地图
      if (m_mpState.isHost) {
        int npcCount = m_enemyOptions[m_enemyIndex];
        m_mazeGenerator = MazeGenerator(m_mazeWidth, m_mazeHeight);
        m_mazeGenerator.setEnemyCount(npcCount);
        auto mazeData = m_mazeGenerator.generate();
        m_maze.loadFromString(mazeData);
        m_mpState.generatedMazeData = mazeData; // 保存地图数据以便后续同步
        NetworkManager::getInstance().sendMazeData(mazeData);
      }
    } });

  net.setOnRoomCreated([this](const std::string &roomCode)
                       {
    m_mpState.roomCode = roomCode;
    m_mpState.isHost = true;
    m_gameState = GameState::WaitingForPlayer;
    m_mpState.connectionStatus = "Room created! Code: " + roomCode;
    
    // 房主立即生成迷宫并发送（使用菜单选择的NPC数量，联机模式生成特殊方块）
    int npcCount = m_enemyOptions[m_enemyIndex];
    std::cout << "[DEBUG] Creating room with " << npcCount << " NPCs" << std::endl;
    m_maze.generateRandomMaze(m_mazeWidth, m_mazeHeight, 0, npcCount, true);
    m_mpState.generatedMazeData = m_maze.getMazeData();
    
    // 检查迷宫数据中是否有敌人标记 'X'
    int xCount = 0;
    for (const auto& row : m_mpState.generatedMazeData) {
      for (char c : row) {
        if (c == 'X') xCount++;
      }
    }
    std::cout << "[DEBUG] Maze data contains " << xCount << " enemy markers (X)" << std::endl;
    
    NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData); });

  net.setOnRoomJoined([this](const std::string &roomCode)
                      {
    m_mpState.roomCode = roomCode;
    m_mpState.isHost = false;
    m_gameState = GameState::WaitingForPlayer;
    m_mpState.connectionStatus = "Joined room: " + roomCode + " - Waiting for maze..."; });

  net.setOnMazeData([this](const std::vector<std::string> &mazeData)
                    {
    // 收到迷宫数据（非房主）
    m_mpState.generatedMazeData = mazeData;
    m_mpState.connectionStatus = "Maze received! Waiting for game start..."; });

  net.setOnRequestMaze([this]()
                       {
    // 服务器请求迷宫数据（房主收到）
    if (m_mpState.isHost && !m_mpState.generatedMazeData.empty()) {
      NetworkManager::getInstance().sendMazeData(m_mpState.generatedMazeData);
    } });

  net.setOnGameStart([this]()
                     {
    m_mpState.isMultiplayer = true;
    
    // 使用已接收/生成的迷宫数据
    if (!m_mpState.generatedMazeData.empty()) {
      m_maze.loadFromString(m_mpState.generatedMazeData);
    }
    
    // 获取两个出生点位置（从迷宫数据中解析的 '1' 和 '2' 标记）
    sf::Vector2f spawn1Pos = m_maze.getSpawn1Position();
    sf::Vector2f spawn2Pos = m_maze.getSpawn2Position();
    
    // 如果没有设置出生点，回退到起点
    if (spawn1Pos.x == 0 && spawn1Pos.y == 0) {
      spawn1Pos = m_maze.getPlayerStartPosition();
    }
    if (spawn2Pos.x == 0 && spawn2Pos.y == 0) {
      spawn2Pos = m_maze.getPlayerStartPosition();
    }
    
    // 房主使用 spawn1，非房主使用 spawn2
    sf::Vector2f mySpawn = m_mpState.isHost ? spawn1Pos : spawn2Pos;
    sf::Vector2f otherSpawn = m_mpState.isHost ? spawn2Pos : spawn1Pos;
    
    // 创建本地玩家并加载贴图
    m_player = std::make_unique<Tank>();
    m_player->loadTextures("tank_assets/PNG/Hulls_Color_A/Hull_01.png",
                           "tank_assets/PNG/Weapon_Color_A/Gun_01.png");
    m_player->setPosition(mySpawn);
    m_player->setScale(m_tankScale);
    m_player->setCoins(10);  // 初始10个金币
    m_player->setTeam(m_mpState.isHost ? 1 : 2);  // 设置阵营
    
    // 设置第二个玩家（另一个客户端）- 使用不同颜色贴图
    m_otherPlayer = std::make_unique<Tank>();
    m_otherPlayer->loadTextures("tank_assets/PNG/Hulls_Color_B/Hull_01.png",
                                "tank_assets/PNG/Weapon_Color_B/Gun_01.png");
    m_otherPlayer->setPosition(otherSpawn);
    m_otherPlayer->setScale(m_tankScale);
    m_otherPlayer->setTeam(m_mpState.isHost ? 2 : 1);  // 对方阵营
    
    m_mpState.localPlayerReachedExit = false;
    m_mpState.otherPlayerReachedExit = false;
    
    // 多人模式：生成中立NPC坦克
    m_enemies.clear();
    
    // 调试：检查敌人生成点数量
    const auto& spawnPoints = m_maze.getEnemySpawnPoints();
    std::cout << "[DEBUG] Multiplayer: Enemy spawn points count = " << spawnPoints.size() << std::endl;
    
    spawnEnemies();  // 使用现有的敌人生成逻辑
    
    std::cout << "[DEBUG] Multiplayer: Enemies spawned = " << m_enemies.size() << std::endl;
    
    // 将所有敌人设置为未激活状态（多人模式需要手动激活）
    int npcId = 0;
    for (auto& enemy : m_enemies) {
      enemy->setId(npcId++);
      // 多人模式下敌人默认未激活，需要玩家花费金币激活
    }
    
    m_bullets.clear();
    m_mpState.nearbyNpcIndex = -1;
    
    // 初始化相机位置和缩放
    m_gameView.setCenter(spawn1Pos);
    m_gameView.setSize({LOGICAL_WIDTH * VIEW_ZOOM, LOGICAL_HEIGHT * VIEW_ZOOM});
    m_currentCameraPos = spawn1Pos;
    
    // 重置终点可见状态并播放开始BGM
    m_exitVisible = false;
    AudioManager::getInstance().playBGM(BGMType::Start);
    
    m_gameState = GameState::Multiplayer; });

  net.setOnPlayerUpdate([this](const PlayerState &state)
                        {
    if (m_otherPlayer) {
      m_otherPlayer->setPosition({state.x, state.y});
      m_otherPlayer->setRotation(state.rotation);
      m_otherPlayer->setTurretRotation(state.turretAngle);
      m_otherPlayer->setHealth(state.health);  // 同步血量
      m_mpState.otherPlayerReachedExit = state.reachedExit;
      
      // 注意：胜利条件改为先到终点或打死对方，不再是双方都到达
    } });

  net.setOnPlayerShoot([this](float x, float y, float angle)
                       {
    // 创建另一个玩家的子弹
    m_bullets.push_back(std::make_unique<Bullet>(x, y, angle, false, sf::Color::Cyan));
    
    // 播放对方射击音效（基于本地玩家位置的距离衰减）
    if (m_player)
    {
      AudioManager::getInstance().playSFX(SFXType::Shoot, {x, y}, m_player->getPosition());
    } });

  net.setOnGameResult([this](bool isWinner)
                      {
    // 收到对方发来的游戏结果
    m_mpState.multiplayerWin = isWinner;
    m_gameState = isWinner ? GameState::Victory : GameState::GameOver; });

  net.setOnRestartRequest([this]()
                          {
    // 对方请求重新开始（房主发起）
    // 非房主收到后自动重新开始游戏
    if (!m_mpState.isHost) {
      restartMultiplayer();
    } });

  // NPC同步回调
  net.setOnNpcActivate([this](int npcId, int team)
                       {
    // 对方激活了NPC
    if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
      m_enemies[npcId]->activate(team);
      std::cout << "[DEBUG] Remote NPC " << npcId << " activated with team " << team << std::endl;
    } });

  net.setOnNpcUpdate([this](const NpcState &state)
                     {
    // 更新NPC状态（仅非房主接收）- 使用插值平滑移动
    if (!m_mpState.isHost && state.id >= 0 && state.id < static_cast<int>(m_enemies.size())) {
      auto& npc = m_enemies[state.id];
      
      // 第一次收到更新时直接设置位置（避免从错误位置插值）
      bool wasRemote = npc->getPosition().x > 0.1f || npc->getPosition().y > 0.1f;
      if (!wasRemote) {
        npc->setPosition({state.x, state.y});
        npc->setRotation(state.rotation);
        npc->setTurretRotation(state.turretAngle);
      }
      
      // 设置为远程控制模式并更新目标状态
      npc->setIsRemote(true);
      npc->setNetworkTarget({state.x, state.y}, state.rotation, state.turretAngle);
      npc->setHealth(state.health);
      if (state.activated && !npc->isActivated()) {
        npc->activate(state.team);
      }
    } });

  net.setOnNpcShoot([this](int npcId, float x, float y, float angle)
                    {
    // NPC射击（创建子弹）- 非房主接收
    if (!m_mpState.isHost) {
      int team = 0;
      if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
        team = m_enemies[npcId]->getTeam();
      }
      sf::Color bulletColor = (team == 1) ? sf::Color::Yellow : sf::Color::Magenta;
      // NPC子弹使用 BulletOwner::Enemy 标识，并设置阵营
      auto bullet = std::make_unique<Bullet>(x, y, angle, false, bulletColor);
      bullet->setTeam(team);
      m_bullets.push_back(std::move(bullet));
      
      // 播放NPC射击音效（基于本地玩家位置的距离衰减）
      if (m_player)
      {
        AudioManager::getInstance().playSFX(SFXType::Shoot, {x, y}, m_player->getPosition());
      }
    } });

  net.setOnNpcDamage([this](int npcId, float damage)
                     {
    // NPC受伤
    if (npcId >= 0 && npcId < static_cast<int>(m_enemies.size())) {
      m_enemies[npcId]->takeDamage(damage);
    } });

  net.setOnError([this](const std::string &error)
                 { m_mpState.connectionStatus = "Error: " + error; });
}

void Game::processConnectingEvents(const sf::Event &event)
{
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    if (keyPressed->code == sf::Keyboard::Key::Escape)
    {
      NetworkManager::getInstance().disconnect();
      resetGame();
      return;
    }

    if (keyPressed->code == sf::Keyboard::Key::Enter)
    {
      switch (m_inputMode)
      {
      case InputMode::ServerIP:
        m_serverIP = m_inputText;
        if (NetworkManager::getInstance().connect(m_serverIP, 9999))
        {
          m_mpState.connectionStatus = "Connected! Enter room code or press C to create:";
          m_inputMode = InputMode::RoomCode;
          m_inputText = "";
        }
        else
        {
          m_mpState.connectionStatus = "Failed to connect to " + m_serverIP;
        }
        break;
      case InputMode::RoomCode:
        if (!m_inputText.empty())
        {
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
        NetworkManager::getInstance().isConnected())
    {
      NetworkManager::getInstance().createRoom(m_mazeWidth, m_mazeHeight);
    }

    // 退格键
    if (keyPressed->code == sf::Keyboard::Key::Backspace && !m_inputText.empty())
    {
      m_inputText.pop_back();
    }
  }

  // 文本输入
  if (const auto *textEntered = event.getIf<sf::Event::TextEntered>())
  {
    if (m_inputMode == InputMode::RoomCode)
    {
      // 房间号只允许输入数字，最多4位
      if (textEntered->unicode >= '0' && textEntered->unicode <= '9' && m_inputText.length() < 4)
      {
        m_inputText += static_cast<char>(textEntered->unicode);
      }
    }
    else if (textEntered->unicode >= 32 && textEntered->unicode < 127)
    {
      m_inputText += static_cast<char>(textEntered->unicode);
    }
  }
}

MultiplayerContext Game::getMultiplayerContext()
{
  return MultiplayerContext{
      m_window,
      m_gameView,
      m_uiView,
      m_font,
      m_player.get(),
      m_otherPlayer.get(),
      m_enemies,
      m_bullets,
      m_maze,
      LOGICAL_WIDTH,  // 使用逻辑分辨率
      LOGICAL_HEIGHT, // 使用逻辑分辨率
      m_tankScale};
}

void Game::updateMultiplayer(float dt)
{
  // R键状态已经在 processEvents 中通过事件驱动设置

  auto ctx = getMultiplayerContext();
  MultiplayerHandler::update(ctx, m_mpState, dt, [this]()
                             { m_gameState = GameState::Victory; }, [this]()
                             { m_gameState = GameState::GameOver; });
}

void Game::renderConnecting()
{
  MultiplayerHandler::renderConnecting(
      m_window, m_uiView, m_font,
      LOGICAL_WIDTH, LOGICAL_HEIGHT,
      m_mpState.connectionStatus, m_inputText,
      m_inputMode == InputMode::ServerIP);
}

void Game::renderWaitingForPlayer()
{
  MultiplayerHandler::renderWaitingForPlayer(
      m_window, m_uiView, m_font,
      LOGICAL_WIDTH, LOGICAL_HEIGHT,
      m_mpState.roomCode);
}

void Game::renderMultiplayer()
{
  auto ctx = getMultiplayerContext();
  MultiplayerHandler::renderMultiplayer(ctx, m_mpState);
}

void Game::handleWindowResize()
{
  sf::Vector2u windowSize = m_window.getSize();
  float windowRatio = static_cast<float>(windowSize.x) / windowSize.y;

  sf::FloatRect viewport;

  if (windowRatio > ASPECT_RATIO)
  {
    // 窗口太宽，左右留黑边
    float viewportWidth = ASPECT_RATIO / windowRatio;
    viewport = sf::FloatRect({(1.f - viewportWidth) / 2.f, 0.f}, {viewportWidth, 1.f});
  }
  else
  {
    // 窗口太高，上下留黑边
    float viewportHeight = windowRatio / ASPECT_RATIO;
    viewport = sf::FloatRect({0.f, (1.f - viewportHeight) / 2.f}, {1.f, viewportHeight});
  }

  m_gameView.setViewport(viewport);
  m_uiView.setViewport(viewport);
}

bool Game::isExitInView() const
{
  if (!m_player)
    return false;
    
  sf::Vector2f exitPos = m_maze.getExitPosition();
  sf::Vector2f viewCenter = m_gameView.getCenter();
  sf::Vector2f viewSize = m_gameView.getSize();
  
  // 检查终点是否在当前视野范围内
  float halfWidth = viewSize.x / 2.f;
  float halfHeight = viewSize.y / 2.f;
  
  return (exitPos.x >= viewCenter.x - halfWidth && exitPos.x <= viewCenter.x + halfWidth &&
          exitPos.y >= viewCenter.y - halfHeight && exitPos.y <= viewCenter.y + halfHeight);
}
