#include "Tank.hpp"

Tank::Tank()
    : m_healthBar(200.f, 20.f)
{
  m_healthBar.setMaxHealth(100.f);
  m_healthBar.setHealth(100.f);
  m_healthBar.setPosition({20.f, 20.f});
}

bool Tank::loadTextures(const std::string &hullPath, const std::string &turretPath)
{
  if (!m_hullTexture.loadFromFile(hullPath))
    return false;
  if (!m_turretTexture.loadFromFile(turretPath))
    return false;

  // 创建并设置车身
  m_hull = std::make_unique<sf::Sprite>(m_hullTexture);
  m_hull->setOrigin(sf::Vector2f(m_hullTexture.getSize()) / 2.f);
  m_hull->setPosition({640.f, 360.f});
  m_hull->setScale({m_scale, m_scale});

  // 创建并设置炮塔
  m_turret = std::make_unique<sf::Sprite>(m_turretTexture);
  m_turret->setOrigin(sf::Vector2f(m_turretTexture.getSize()) / 2.f);
  m_turret->setScale({m_scale, m_scale});

  return true;
}
void Tank::handleInput(const sf::Event &event)
{
  // 键盘按下
  if (const auto *keyPressed = event.getIf<sf::Event::KeyPressed>())
  {
    if (keyPressed->code == sf::Keyboard::Key::W)
      m_keyW = true;
    if (keyPressed->code == sf::Keyboard::Key::S)
      m_keyS = true;
    if (keyPressed->code == sf::Keyboard::Key::A)
      m_keyA = true;
    if (keyPressed->code == sf::Keyboard::Key::D)
      m_keyD = true;
  }

  // 键盘释放
  if (const auto *keyReleased = event.getIf<sf::Event::KeyReleased>())
  {
    if (keyReleased->code == sf::Keyboard::Key::W)
      m_keyW = false;
    if (keyReleased->code == sf::Keyboard::Key::S)
      m_keyS = false;
    if (keyReleased->code == sf::Keyboard::Key::A)
      m_keyA = false;
    if (keyReleased->code == sf::Keyboard::Key::D)
      m_keyD = false;
  }

  // 鼠标按下
  if (const auto *mousePressed = event.getIf<sf::Event::MouseButtonPressed>())
  {
    if (mousePressed->button == sf::Mouse::Button::Left)
      m_mouseHeld = true;
  }

  // 鼠标释放
  if (const auto *mouseReleased = event.getIf<sf::Event::MouseButtonReleased>())
  {
    if (mouseReleased->button == sf::Mouse::Button::Left)
      m_mouseHeld = false;
  }
}

void Tank::update(float dt, sf::Vector2f mousePos)
{
  // 计算移动
  sf::Vector2f movement{0.f, 0.f};
  if (m_keyW)
    movement.y -= m_moveSpeed * dt;
  if (m_keyS)
    movement.y += m_moveSpeed * dt;
  if (m_keyA)
    movement.x -= m_moveSpeed * dt;
  if (m_keyD)
    movement.x += m_moveSpeed * dt;

  // 移动并转向
  if (movement.x != 0.f || movement.y != 0.f)
  {
    m_hull->move(movement);

    float targetAngle = Utils::getDirectionAngle(movement);
    m_hullAngle = Utils::lerpAngle(m_hullAngle, targetAngle, m_rotationSpeed * dt);
    m_hull->setRotation(sf::degrees(m_hullAngle));
  }

  // 炮塔跟随车身位置
  m_turret->setPosition(m_hull->getPosition());

  // 炮塔朝向鼠标
  float angle = Utils::getAngle(m_turret->getPosition(), mousePos);
  m_turret->setRotation(sf::degrees(angle));
}

void Tank::draw(sf::RenderWindow &window) const
{
  window.draw(*m_hull);
  window.draw(*m_turret);
}

void Tank::drawUI(sf::RenderWindow &window) const
{
  m_healthBar.draw(window);
}

sf::Vector2f Tank::getPosition() const
{
  return m_hull->getPosition();
}

float Tank::getTurretAngle() const
{
  return m_turret->getRotation().asDegrees();
}

sf::Vector2f Tank::getGunPosition() const
{
  float angleRad = (m_turret->getRotation().asDegrees() - 90.f) * Utils::PI / 180.f;
  sf::Vector2f offset = {std::cos(angleRad) * m_gunLength,
                         std::sin(angleRad) * m_gunLength};
  return m_hull->getPosition() + offset;
}

void Tank::takeDamage(float damage)
{
  m_healthBar.setHealth(m_healthBar.getHealth() - damage);
}
