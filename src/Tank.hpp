#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "Utils.hpp"
#include "HealthBar.hpp"

class Tank
{
public:
  Tank();

  bool loadTextures(const std::string &hullPath, const std::string &turretPath);

  void handleInput(const sf::Event &event);
  void update(float dt, sf::Vector2f mousePos);
  void draw(sf::RenderWindow &window) const;
  void drawUI(sf::RenderWindow &window) const; // 绘制 UI（血条在左上角）

  void setPosition(sf::Vector2f pos);
  sf::Vector2f getPosition() const;
  float getTurretAngle() const;

  // 获取枪口位置
  sf::Vector2f getGunPosition() const;

  // 检查是否在射击
  bool isShooting() const { return m_mouseHeld; }

  // 受到伤害
  void takeDamage(float damage);
  bool isDead() const { return m_healthBar.isDead(); }

  // 获取碰撞半径
  float getCollisionRadius() const { return 18.f; }

  // 获取当前移动向量（用于墙壁滑动）
  sf::Vector2f getMovement(float dt) const;

private:
  sf::Texture m_hullTexture;
  sf::Texture m_turretTexture;
  std::unique_ptr<sf::Sprite> m_hull;
  std::unique_ptr<sf::Sprite> m_turret;

  HealthBar m_healthBar;

  // 移动状态
  bool m_keyW = false;
  bool m_keyS = false;
  bool m_keyA = false;
  bool m_keyD = false;
  bool m_mouseHeld = false;

  // 车身角度
  float m_hullAngle = 0.f;

  // 配置
  const float m_moveSpeed = 200.f;
  const float m_rotationSpeed = 5.f;
  const float m_scale = 0.25f;
  const float m_gunLength = 25.f;
};
