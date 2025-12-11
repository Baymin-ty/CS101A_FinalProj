#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include "Utils.hpp"

enum class BulletOwner
{
  Player,
  Enemy
};

class Bullet
{
public:
  Bullet(const sf::Texture &texture, sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner);

  void update(float dt);
  void draw(sf::RenderWindow &window) const;

  bool isActive() const { return m_active; }
  void setInactive() { m_active = false; }

  // 检查是否出界
  void checkBounds(float width, float height);

  // 碰撞检测
  sf::Vector2f getPosition() const { return m_sprite.getPosition(); }
  BulletOwner getOwner() const { return m_owner; }

private:
  sf::Sprite m_sprite;
  sf::Vector2f m_velocity;
  bool m_active = true;
  BulletOwner m_owner;
};

// 管理所有子弹
class BulletManager
{
public:
  void setTexture(const sf::Texture &texture) { m_texture = &texture; }

  void spawn(sf::Vector2f position, float angleDegrees, float speed, BulletOwner owner = BulletOwner::Player);
  void update(float dt, float screenWidth, float screenHeight);
  void draw(sf::RenderWindow &window) const;

  // 检查与目标的碰撞，返回伤害值
  float checkCollision(sf::Vector2f targetPos, float targetRadius, BulletOwner ignoreOwner);

private:
  const sf::Texture *m_texture = nullptr;
  std::vector<Bullet> m_bullets;
};
