#include "Enemy.hpp"
#include <cstdlib>
#include <cmath>

Enemy::Enemy()
    : m_healthBar(50.f, 6.f)
{
  m_healthBar.setMaxHealth(100.f);
  m_healthBar.setHealth(100.f);

  // 随机初始移动方向
  float angle = static_cast<float>(rand() % 360) * Utils::PI / 180.f;
  m_moveDirection = {std::cos(angle), std::sin(angle)};
}

bool Enemy::loadTextures(const std::string &hullPath, const std::string &turretPath)
{
  if (!m_hullTexture.loadFromFile(hullPath))
    return false;
  if (!m_turretTexture.loadFromFile(turretPath))
    return false;

  m_hull = std::make_unique<sf::Sprite>(m_hullTexture);
  m_hull->setOrigin(sf::Vector2f(m_hullTexture.getSize()) / 2.f);
  m_hull->setScale({m_scale, m_scale});

  m_turret = std::make_unique<sf::Sprite>(m_turretTexture);
  m_turret->setOrigin(sf::Vector2f(m_turretTexture.getSize()) / 2.f);
  m_turret->setScale({m_scale, m_scale});

  return true;
}

void Enemy::setPosition(sf::Vector2f position)
{
  if (m_hull)
  {
    m_hull->setPosition(position);
  }
}

void Enemy::setTarget(sf::Vector2f targetPos)
{
  m_targetPos = targetPos;
}

void Enemy::update(float dt)
{
  if (!m_hull || !m_turret)
    return;

  // 定期改变移动方向
  if (m_directionChangeClock.getElapsedTime().asSeconds() > m_directionChangeInterval)
  {
    float angle = static_cast<float>(rand() % 360) * Utils::PI / 180.f;
    m_moveDirection = {std::cos(angle), std::sin(angle)};
    m_directionChangeClock.restart();
  }

  // 移动
  sf::Vector2f movement = m_moveDirection * m_moveSpeed * dt;
  sf::Vector2f newPos = m_hull->getPosition() + movement;

  // 边界检查（保持在屏幕内）
  newPos.x = std::max(50.f, std::min(newPos.x, 1230.f));
  newPos.y = std::max(50.f, std::min(newPos.y, 670.f));

  // 如果碰到边界，反弹
  if (newPos.x <= 50.f || newPos.x >= 1230.f)
  {
    m_moveDirection.x = -m_moveDirection.x;
  }
  if (newPos.y <= 50.f || newPos.y >= 670.f)
  {
    m_moveDirection.y = -m_moveDirection.y;
  }

  m_hull->setPosition(newPos);

  // 车身转向移动方向
  if (movement.x != 0.f || movement.y != 0.f)
  {
    float targetAngle = Utils::getDirectionAngle(movement);
    m_hullAngle = Utils::lerpAngle(m_hullAngle, targetAngle, m_rotationSpeed * dt);
    m_hull->setRotation(sf::degrees(m_hullAngle));
  }

  // 炮塔跟随车身位置
  m_turret->setPosition(m_hull->getPosition());

  // 炮塔朝向玩家
  float angle = Utils::getAngle(m_turret->getPosition(), m_targetPos);
  m_turret->setRotation(sf::degrees(angle));

  // 更新血条位置（在坦克上方）
  sf::Vector2f healthBarPos = m_hull->getPosition();
  healthBarPos.x -= 25.f; // 居中
  healthBarPos.y -= 45.f; // 在坦克上方
  m_healthBar.setPosition(healthBarPos);
}

void Enemy::draw(sf::RenderWindow &window) const
{
  if (m_hull && m_turret)
  {
    window.draw(*m_hull);
    window.draw(*m_turret);
    m_healthBar.draw(window);
  }
}

sf::Vector2f Enemy::getPosition() const
{
  return m_hull ? m_hull->getPosition() : sf::Vector2f{0.f, 0.f};
}

float Enemy::getTurretAngle() const
{
  return m_turret ? m_turret->getRotation().asDegrees() : 0.f;
}

sf::Vector2f Enemy::getGunPosition() const
{
  if (!m_turret || !m_hull)
    return {0.f, 0.f};

  float angleRad = (m_turret->getRotation().asDegrees() - 90.f) * Utils::PI / 180.f;
  sf::Vector2f offset = {std::cos(angleRad) * m_gunLength,
                         std::sin(angleRad) * m_gunLength};
  return m_hull->getPosition() + offset;
}

bool Enemy::shouldShoot()
{
  if (m_shootClock.getElapsedTime().asSeconds() > m_shootCooldown)
  {
    m_shootClock.restart();
    return true;
  }
  return false;
}

void Enemy::takeDamage(float damage)
{
  m_healthBar.setHealth(m_healthBar.getHealth() - damage);
}
