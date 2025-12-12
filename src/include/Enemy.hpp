#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include "Utils.hpp"
#include "HealthBar.hpp"

// 前向声明
class Maze;

class Enemy
{
public:
  Enemy();

  bool loadTextures(const std::string &hullPath, const std::string &turretPath);

  void setPosition(sf::Vector2f position);
  void setTarget(sf::Vector2f targetPos);
  void update(float dt, const Maze &maze); // 添加迷宫参数用于碰撞检测
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
  float getCollisionRadius() const { return 18.f; }

  // 激活状态（多人模式需要手动激活，单人模式自动激活）
  bool isActivated() const { return m_activated; }
  void activate(int team) { m_activated = true; m_team = team; }
  
  // 检查玩家是否在激活范围内（用于显示提示）
  bool isPlayerInRange(sf::Vector2f playerPos) const;
  float getActivationRange() const { return m_activationRange; }
  
  // 单人模式：自动激活检测
  void checkAutoActivation(sf::Vector2f playerPos);

  // 阵营系统（0=未激活/中立，1=玩家1阵营，2=玩家2阵营）
  int getTeam() const { return m_team; }
  void setTeam(int team) { m_team = team; }

  // 设置多个目标（用于追踪敌方阵营的所有目标）
  void setTargets(const std::vector<sf::Vector2f>& targets);
  
  // 设置边界（迷宫大小）
  void setBounds(sf::Vector2f bounds) { m_bounds = bounds; }

  // NPC ID（用于网络同步）
  int getId() const { return m_id; }
  void setId(int id) { m_id = id; }
  
  // 网络同步需要的getter/setter
  float getRotation() const { return m_hullAngle; }
  void setRotation(float angle) { m_hullAngle = angle; if (m_hull) m_hull->setRotation(sf::degrees(angle)); }
  float getTurretRotation() const;
  void setTurretRotation(float angle);
  float getHealth() const { return m_healthBar.getHealth(); }
  void setHealth(float health) { m_healthBar.setHealth(health); }

private:
  sf::Texture m_hullTexture;
  sf::Texture m_turretTexture;
  std::unique_ptr<sf::Sprite> m_hull;
  std::unique_ptr<sf::Sprite> m_turret;

  HealthBar m_healthBar;

  sf::Vector2f m_targetPos;
  sf::Vector2f m_moveDirection;
  sf::Vector2f m_bounds = {1280.f, 720.f};

  // A* 寻路
  std::vector<sf::Vector2f> m_path;
  size_t m_currentPathIndex = 0;
  sf::Clock m_pathUpdateClock;
  const float m_pathUpdateInterval = 0.5f; // 每0.5秒更新路径

  float m_hullAngle = 0.f;
  sf::Clock m_shootClock;
  sf::Clock m_directionChangeClock;

  bool m_activated = false; // 是否被激活
  int m_team = 0;           // 阵营：0=中立，1=玩家1，2=玩家2
  int m_id = 0;             // NPC唯一ID

  // 多目标追踪
  std::vector<sf::Vector2f> m_targets;

  // 配置
  const float m_moveSpeed = 120.f;
  const float m_rotationSpeed = 3.f;
  const float m_scale = 0.25f;
  const float m_gunLength = 25.f;
  const float m_shootCooldown = 1.0f;
  const float m_directionChangeInterval = 2.0f;
  const float m_activationRange = 60.f; // 激活距离（需要接近才能激活）
};
