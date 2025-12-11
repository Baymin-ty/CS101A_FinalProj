#include <SFML/Graphics.hpp>
#include <cmath>

constexpr float PI = 3.14159265f;

// 计算向量的角度（度）
float getAngle(sf::Vector2f from, sf::Vector2f to)
{
  sf::Vector2f diff = to - from;
  return std::atan2(diff.y, diff.x) * 180.f / PI + 90.f;
}

// 计算向量方向的角度（度）
float getDirectionAngle(sf::Vector2f dir)
{
  return std::atan2(dir.y, dir.x) * 180.f / PI + 90.f;
}

// 平滑插值角度（处理 -180 到 180 的跨越）
float lerpAngle(float current, float target, float t)
{
  float diff = target - current;

  // 处理角度跨越 180/-180 的情况
  while (diff > 180.f)
    diff -= 360.f;
  while (diff < -180.f)
    diff += 360.f;

  return current + diff * t;
}

int main()
{
  // 创建窗口
  sf::RenderWindow window(sf::VideoMode({1280, 720}), "Tank Game");
  window.setFramerateLimit(60);

  // 加载坦克车身纹理
  sf::Texture hullTexture;
  if (!hullTexture.loadFromFile("tank_assets/PNG/Hulls_Color_A/Hull_01.png"))
  {
    return -1;
  }

  // 加载炮塔纹理
  sf::Texture turretTexture;
  if (!turretTexture.loadFromFile("tank_assets/PNG/Weapon_Color_A/Gun_01.png"))
  {
    return -1;
  }

  // 加载子弹纹理
  sf::Texture bulletTexture;
  if (!bulletTexture.loadFromFile("tank_assets/PNG/Effects/Medium_Shell.png"))
  {
    return -1;
  }

  // 创建坦克车身精灵
  sf::Sprite hull(hullTexture);
  hull.setOrigin(sf::Vector2f(hullTexture.getSize()) / 2.f);
  hull.setPosition({640.f, 360.f});
  hull.setScale({0.35f, 0.35f});

  // 创建炮塔精灵
  sf::Sprite turret(turretTexture);
  turret.setOrigin({static_cast<float>(turretTexture.getSize().x) / 2.f,
                    static_cast<float>(turretTexture.getSize().y) / 2.f});
  turret.setScale({0.35f, 0.35f});

  // 子弹结构
  struct Bullet
  {
    sf::Sprite sprite;
    sf::Vector2f velocity;
    bool active = true;

    Bullet(const sf::Texture &tex) : sprite(tex), velocity{0.f, 0.f} {}
  };
  std::vector<Bullet> bullets;

  // 坦克移动速度
  const float moveSpeed = 200.f;
  const float bulletSpeed = 500.f;
  const float hullRotationSpeed = 5.f; // 车身转向速度

  // 车身当前角度
  float hullAngle = 0.f;

  // 记录按键状态
  bool keyW = false, keyS = false, keyA = false, keyD = false;
  bool mouseHeld = false; // 鼠标是否按住

  // 时钟用于计算 delta time
  sf::Clock clock;
  sf::Clock shootClock; // 射击冷却

  while (window.isOpen())
  {
    float dt = clock.restart().asSeconds();

    // 事件处理
    while (const auto event = window.pollEvent())
    {
      if (event->is<sf::Event::Closed>())
      {
        window.close();
      }

      // 键盘按下
      if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
      {
        if (keyPressed->code == sf::Keyboard::Key::W)
          keyW = true;
        if (keyPressed->code == sf::Keyboard::Key::S)
          keyS = true;
        if (keyPressed->code == sf::Keyboard::Key::A)
          keyA = true;
        if (keyPressed->code == sf::Keyboard::Key::D)
          keyD = true;
      }

      // 键盘释放
      if (const auto *keyReleased = event->getIf<sf::Event::KeyReleased>())
      {
        if (keyReleased->code == sf::Keyboard::Key::W)
          keyW = false;
        if (keyReleased->code == sf::Keyboard::Key::S)
          keyS = false;
        if (keyReleased->code == sf::Keyboard::Key::A)
          keyA = false;
        if (keyReleased->code == sf::Keyboard::Key::D)
          keyD = false;
      }

      // 鼠标按下
      if (const auto *mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
      {
        if (mousePressed->button == sf::Mouse::Button::Left)
          mouseHeld = true;
      }

      // 鼠标释放
      if (const auto *mouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
      {
        if (mouseReleased->button == sf::Mouse::Button::Left)
          mouseHeld = false;
      }
    }

    // 长按鼠标持续发射子弹
    if (mouseHeld && shootClock.getElapsedTime().asSeconds() > 0.3f)
    {
      Bullet bullet(bulletTexture);
      bullet.sprite.setOrigin(sf::Vector2f(bulletTexture.getSize()) / 2.f);

      // 计算枪管口位置（从坦克中心沿炮塔方向偏移）
      float angleRad = (turret.getRotation().asDegrees() - 90.f) * PI / 180.f;
      float gunLength = 35.f; // 枪管长度偏移
      sf::Vector2f gunOffset = {std::cos(angleRad) * gunLength,
                                std::sin(angleRad) * gunLength};
      bullet.sprite.setPosition(hull.getPosition() + gunOffset);

      bullet.sprite.setRotation(turret.getRotation());
      bullet.sprite.setScale({0.35f, 0.35f});

      // 计算子弹速度方向
      bullet.velocity = {std::cos(angleRad) * bulletSpeed,
                         std::sin(angleRad) * bulletSpeed};

      bullets.push_back(std::move(bullet));
      shootClock.restart();
    }

    // WASD 控制坦克移动
    sf::Vector2f movement{0.f, 0.f};
    if (keyW)
    {
      movement.y -= moveSpeed * dt;
    }
    if (keyS)
    {
      movement.y += moveSpeed * dt;
    }
    if (keyA)
    {
      movement.x -= moveSpeed * dt;
    }
    if (keyD)
    {
      movement.x += moveSpeed * dt;
    }

    // 确保有移动
    if (movement.x != 0.f || movement.y != 0.f)
    {
      hull.move(movement);

      // 计算目标角度（移动方向）
      float targetAngle = getDirectionAngle(movement);

      // 平滑转向移动方向
      hullAngle = lerpAngle(hullAngle, targetAngle, hullRotationSpeed * dt);
      hull.setRotation(sf::degrees(hullAngle));
    }

    // 炮塔跟随车身位置
    turret.setPosition(hull.getPosition());

    // 鼠标控制炮塔旋转
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    sf::Vector2f mousePosF{static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)};
    float angle = getAngle(turret.getPosition(), mousePosF);
    turret.setRotation(sf::degrees(angle));

    // 更新子弹位置
    for (auto &bullet : bullets)
    {
      bullet.sprite.move(bullet.velocity * dt);

      // 检查子弹是否出界
      sf::Vector2f pos = bullet.sprite.getPosition();
      if (pos.x < -50 || pos.x > 1330 || pos.y < -50 || pos.y > 770)
      {
        bullet.active = false;
      }
    }

    // 移除不活跃的子弹
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
                       [](const Bullet &b)
                       { return !b.active; }),
        bullets.end());

    // 渲染
    window.clear(sf::Color(50, 50, 50)); // 深灰色背景

    // 绘制子弹
    for (const auto &bullet : bullets)
    {
      window.draw(bullet.sprite);
    }

    // 绘制坦克（先车身后炮塔）
    window.draw(hull);
    window.draw(turret);

    window.display();
  }

  return 0;
}