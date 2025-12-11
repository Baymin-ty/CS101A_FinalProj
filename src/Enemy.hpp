#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "Utils.hpp"
#include "HealthBar.hpp"

class Enemy
{
public:
  Enemy();

  bool loadTextures(const std::string &hullPath, const std::string &turretPath);

  void setPosition(sf::Vector2f position);
  void setTarget(sf::Vector2f targetPos);
  void update(float dt);
  void draw(sf::RenderWindow &window) const;

  sf::Vector2f getPosition() const;
  float getTurretAngle() const;
  sf::Vector2f getGunPosition() const;

  // 检查是否应该射击
  bool shouldShoot();

  // 受到伤害
  void takeDamage(float damage);
  bool isDead() const { return m_healthBar.isDead(); }

  // 获取碰撞半径
  float getCollisionRadius() const { return 25.f; }

private:
  sf::Texture m_hullTexture;
  sf::Texture m_turretTexture;
  std::unique_ptr<sf::Sprite> m_hull;
  std::unique_ptr<sf::Sprite> m_turret;

  HealthBar m_healthBar;

  sf::Vector2f m_targetPos;
  sf::Vector2f m_moveDirection;

  float m_hullAngle = 0.f;
  sf::Clock m_shootClock;
  sf::Clock m_directionChangeClock;

  // 配置
  const float m_moveSpeed = 100.f;
  const float m_rotationSpeed = 3.f;
  const float m_scale = 0.35f;
  const float m_gunLength = 35.f;
  const float m_shootCooldown = 1.0f;
  const float m_directionChangeInterval = 2.0f;
};
