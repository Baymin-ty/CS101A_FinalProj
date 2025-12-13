#include "Enemy.hpp"
#include "Maze.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>

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

bool Enemy::loadActivatedTextures()
{
  // 加载激活状态的贴图（Color_C）
  if (!m_hullTexture.loadFromFile("tank_assets/PNG/Hulls_Color_C/Hull_01.png"))
    return false;
  if (!m_turretTexture.loadFromFile("tank_assets/PNG/Weapon_Color_C/Gun_01.png"))
    return false;

  // 保存当前位置和旋转
  sf::Vector2f pos = m_hull ? m_hull->getPosition() : sf::Vector2f{0.f, 0.f};
  float hullRot = m_hull ? m_hull->getRotation().asDegrees() : 0.f;
  float turretRot = m_turret ? m_turret->getRotation().asDegrees() : 0.f;

  // 重新创建精灵
  m_hull = std::make_unique<sf::Sprite>(m_hullTexture);
  m_hull->setOrigin(sf::Vector2f(m_hullTexture.getSize()) / 2.f);
  m_hull->setScale({m_scale, m_scale});
  m_hull->setPosition(pos);
  m_hull->setRotation(sf::degrees(hullRot));

  m_turret = std::make_unique<sf::Sprite>(m_turretTexture);
  m_turret->setOrigin(sf::Vector2f(m_turretTexture.getSize()) / 2.f);
  m_turret->setScale({m_scale, m_scale});
  m_turret->setPosition(pos);
  m_turret->setRotation(sf::degrees(turretRot));

  return true;
}

void Enemy::activate(int team)
{
  if (!m_activated)
  {
    m_activated = true;
    m_team = team;
    // 切换到激活状态贴图
    loadActivatedTextures();
  }
}

void Enemy::setPosition(sf::Vector2f position)
{
  if (m_hull)
  {
    m_hull->setPosition(position);
  }
  // 同步更新炮塔位置
  if (m_turret)
  {
    m_turret->setPosition(position);
  }
  // 同步更新血条位置
  sf::Vector2f healthBarPos = position;
  healthBarPos.x -= 25.f;
  healthBarPos.y -= 45.f;
  m_healthBar.setPosition(healthBarPos);
}

void Enemy::setTarget(sf::Vector2f targetPos)
{
  m_targetPos = targetPos;
}

void Enemy::update(float dt, const Maze &maze)
{
  if (!m_hull || !m_turret)
    return;

  // 如果未激活，只是待机（不移动不攻击）
  if (!m_activated)
  {
    // 炮塔跟随车身位置
    m_turret->setPosition(m_hull->getPosition());
    // 更新血条位置
    sf::Vector2f healthBarPos = m_hull->getPosition();
    healthBarPos.x -= 25.f;
    healthBarPos.y -= 45.f;
    m_healthBar.setPosition(healthBarPos);
    return;
  }

  // 保存旧位置
  sf::Vector2f oldPos = m_hull->getPosition();

  // 定期更新路径（使用智能路径，考虑可破坏墙）
  if (m_pathUpdateClock.getElapsedTime().asSeconds() > m_pathUpdateInterval || m_path.empty())
  {
    // 首先尝试普通路径
    auto normalPath = maze.findPath(oldPos, m_targetPos);

    // 然后尝试穿过可破坏墙的路径
    auto smartPathResult = maze.findPathThroughDestructible(oldPos, m_targetPos, 3.0f);

    // 比较两条路径，选择更优的
    // 如果智能路径明显更短（考虑到可破坏墙的额外代价），则使用智能路径
    bool useSmartPath = false;

    if (!smartPathResult.path.empty())
    {
      if (normalPath.empty())
      {
        // 普通路径找不到，使用智能路径
        useSmartPath = true;
      }
      else if (smartPathResult.hasDestructibleWall)
      {
        // 如果智能路径穿过可破坏墙，比较实际长度
        // 智能路径需要比普通路径短很多才值得（因为需要花时间打墙）
        float normalLen = static_cast<float>(normalPath.size());
        float smartLen = static_cast<float>(smartPathResult.path.size());

        // 如果智能路径比普通路径短50%以上，使用智能路径
        if (smartLen < normalLen * 0.5f)
        {
          useSmartPath = true;
        }
      }
      else
      {
        // 智能路径没有可破坏墙，且不为空，说明和普通路径一样
        useSmartPath = false;
      }
    }

    if (useSmartPath)
    {
      m_path = smartPathResult.path;
      m_hasDestructibleWallOnPath = smartPathResult.hasDestructibleWall;
      m_destructibleWallTarget = smartPathResult.firstDestructibleWallPos;
    }
    else
    {
      m_path = normalPath;
      m_hasDestructibleWallOnPath = false;
      m_destructibleWallTarget = {0.f, 0.f};
    }

    m_currentPathIndex = 0;
    m_pathUpdateClock.restart();
  }

  // 沿路径移动
  sf::Vector2f moveTarget = m_targetPos; // 默认直接朝向玩家

  if (!m_path.empty() && m_currentPathIndex < m_path.size())
  {
    moveTarget = m_path[m_currentPathIndex];

    // 检查是否接近当前路径点
    sf::Vector2f toWaypoint = moveTarget - oldPos;
    float distToWaypoint = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.y * toWaypoint.y);

    if (distToWaypoint < 20.f)
    {
      // 移动到下一个路径点
      m_currentPathIndex++;
      if (m_currentPathIndex < m_path.size())
      {
        moveTarget = m_path[m_currentPathIndex];
      }
    }
  }

  // 计算移动方向
  sf::Vector2f toTarget = moveTarget - oldPos;
  float distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

  if (distToTarget > 5.f)
  {
    m_moveDirection = toTarget / distToTarget; // 归一化
  }

  // 计算到玩家的距离
  sf::Vector2f toPlayer = m_targetPos - oldPos;
  float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

  // 先进行视线检测，确定是否有直接视线到玩家
  // 使用 m_lastLineOfSightResult 作为缓存（每次路径更新时刷新）
  int losToPlayer = m_lastLineOfSightResult;

  // 只有在有直接视线（无墙阻挡）的情况下才保持距离
  if (losToPlayer == 0)
  {
    if (distToPlayer < 80.f)
    {
      // 太近了，后退
      m_moveDirection = -toPlayer / distToPlayer;
    }
    else if (distToPlayer < 120.f && distToPlayer > 80.f)
    {
      // 在合适距离，横向移动
      m_moveDirection = {-toPlayer.y / distToPlayer, toPlayer.x / distToPlayer};
    }
  }
  // 如果有墙阻挡（losToPlayer != 0），继续沿A*路径移动

  // 移动
  sf::Vector2f movement = m_moveDirection * m_moveSpeed * dt;
  sf::Vector2f newPos = oldPos + movement;

  // 边界检查
  newPos.x = std::max(50.f, std::min(newPos.x, m_bounds.x - 50.f));
  newPos.y = std::max(50.f, std::min(newPos.y, m_bounds.y - 50.f));

  // 检查墙壁碰撞并实现滑动
  if (!maze.checkCollision(newPos, getCollisionRadius()))
  {
    m_hull->setPosition(newPos);
  }
  else
  {
    // 尝试只在 X 或 Y 方向移动（滑动）
    sf::Vector2f newPosX = {oldPos.x + movement.x, oldPos.y};
    sf::Vector2f newPosY = {oldPos.x, oldPos.y + movement.y};

    bool canMoveX = !maze.checkCollision(newPosX, getCollisionRadius());
    bool canMoveY = !maze.checkCollision(newPosY, getCollisionRadius());

    if (canMoveX && canMoveY)
    {
      // 选择主要移动方向
      if (std::abs(movement.x) > std::abs(movement.y))
        m_hull->setPosition(newPosX);
      else
        m_hull->setPosition(newPosY);
    }
    else if (canMoveX)
    {
      m_hull->setPosition(newPosX);
    }
    else if (canMoveY)
    {
      m_hull->setPosition(newPosY);
    }
    // 如果两个方向都碰撞，则不移动
  }

  // 车身转向移动方向
  sf::Vector2f actualMovement = m_hull->getPosition() - oldPos;
  if (actualMovement.x != 0.f || actualMovement.y != 0.f)
  {
    float targetAngle = Utils::getDirectionAngle(actualMovement);
    m_hullAngle = Utils::lerpAngle(m_hullAngle, targetAngle, m_rotationSpeed * dt);
    m_hull->setRotation(sf::degrees(m_hullAngle));
  }

  // 炮塔跟随车身位置
  m_turret->setPosition(m_hull->getPosition());

  // 选择最佳目标和射击策略
  m_hasValidTarget = false;
  sf::Vector2f bestTarget = m_targetPos;
  int bestLOS = 2; // 默认假设最差情况（不可拆墙阻挡）
  float bestDist = 999999.f;

  // 如果有多个目标，选择最容易攻击的
  std::vector<sf::Vector2f> allTargets = m_targets;
  if (allTargets.empty())
  {
    allTargets.push_back(m_targetPos);
  }

  for (const auto &target : allTargets)
  {
    sf::Vector2f toTarget = target - m_hull->getPosition();
    float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    int los = maze.checkLineOfSight(m_hull->getPosition(), target);

    // 优先选择：无阻挡 > 可拆墙 > 不可拆墙，距离作为次要因素
    if (los < bestLOS || (los == bestLOS && dist < bestDist))
    {
      bestLOS = los;
      bestDist = dist;
      bestTarget = target;
    }
  }

  m_lastLineOfSightResult = bestLOS;

  // 根据视线检测结果决定射击目标
  if (bestLOS == 0)
  {
    // 无阻挡，直接瞄准目标
    m_shootTarget = bestTarget;
    m_hasValidTarget = true;
  }
  else if (bestLOS == 1)
  {
    // 有可拆墙阻挡，瞄准可拆墙
    m_shootTarget = maze.getFirstBlockedPosition(m_hull->getPosition(), bestTarget);
    m_hasValidTarget = true;
  }
  else
  {
    // 不可拆墙阻挡
    // 检查是否有智能路径上的可破坏墙可以攻击
    if (m_hasDestructibleWallOnPath)
    {
      // 检查是否能看到智能路径上的可破坏墙
      int losToWall = maze.checkLineOfSight(m_hull->getPosition(), m_destructibleWallTarget);
      if (losToWall != 2) // 不是被不可破坏墙完全挡住
      {
        // 可以攻击智能路径上的可破坏墙
        if (losToWall == 0)
        {
          // 可以直接看到目标墙
          m_shootTarget = m_destructibleWallTarget;
          m_hasValidTarget = true;
        }
        else
        {
          // 中间还有其他可破坏墙，攻击第一个
          m_shootTarget = maze.getFirstBlockedPosition(m_hull->getPosition(), m_destructibleWallTarget);
          m_hasValidTarget = true;
        }
      }
    }

    // 如果还是没有有效目标，不射击，继续移动寻找更好的位置
  }

  // 炮塔朝向射击目标（如果有有效目标）
  if (m_hasValidTarget)
  {
    float angle = Utils::getAngle(m_turret->getPosition(), m_shootTarget);
    m_turret->setRotation(sf::degrees(angle));
  }
  else
  {
    // 没有有效目标时，炮塔朝向移动方向
    float angle = Utils::getAngle(m_turret->getPosition(), bestTarget);
    m_turret->setRotation(sf::degrees(angle));
  }

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

float Enemy::getTurretRotation() const
{
  return getTurretAngle();
}

void Enemy::setTurretRotation(float angle)
{
  if (m_turret)
  {
    m_turret->setRotation(sf::degrees(angle));
  }
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
  // 只有激活后才会射击
  if (!m_activated)
    return false;

  // 没有有效目标时不射击（不可拆墙阻挡）
  if (!m_hasValidTarget)
    return false;

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

bool Enemy::isPlayerInRange(sf::Vector2f playerPos) const
{
  sf::Vector2f toPlayer = playerPos - getPosition();
  float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
  return dist < m_activationRange;
}

void Enemy::checkAutoActivation(sf::Vector2f playerPos)
{
  // 单人模式使用：距离450内自动激活
  if (m_activated)
    return;

  sf::Vector2f toPlayer = playerPos - getPosition();
  float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

  if (dist < 450.f)
  {
    m_activated = true;
    m_team = 0; // 单人模式中敌人没有阵营，攻击玩家
  }
}

void Enemy::setTargets(const std::vector<sf::Vector2f> &targets)
{
  m_targets = targets;

  // 选择最近的目标
  if (!m_targets.empty())
  {
    float minDist = std::numeric_limits<float>::max();
    for (const auto &target : m_targets)
    {
      sf::Vector2f toTarget = target - getPosition();
      float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
      if (dist < minDist)
      {
        minDist = dist;
        m_targetPos = target;
      }
    }
  }
}

void Enemy::setNetworkTarget(sf::Vector2f pos, float rotation, float turretAngle)
{
  m_networkTargetPos = pos;
  m_networkTargetRotation = rotation;
  m_networkTargetTurretAngle = turretAngle;
}

void Enemy::updateInterpolation(float dt)
{
  if (!m_isRemote)
    return;

  // 使用线性插值（lerp）平滑过渡到目标位置
  // 插值因子：越大越快跟上，0.1-0.3之间比较平滑
  float lerpFactor = std::min(1.0f, m_interpSpeed * dt);

  // 位置插值
  sf::Vector2f currentPos = getPosition();
  sf::Vector2f diff = m_networkTargetPos - currentPos;
  float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

  if (dist > 1.f)
  {
    // 如果距离太远（瞬移情况），直接设置位置
    if (dist > 500.f)
    {
      setPosition(m_networkTargetPos);
    }
    else
    {
      // 线性插值
      sf::Vector2f newPos = currentPos + diff * lerpFactor;
      setPosition(newPos);
    }
  }

  // 角度插值
  float currentRotation = getRotation();
  float rotDiff = m_networkTargetRotation - currentRotation;
  // 处理角度跨越
  while (rotDiff > 180.f)
    rotDiff -= 360.f;
  while (rotDiff < -180.f)
    rotDiff += 360.f;

  if (std::abs(rotDiff) > 1.f)
  {
    setRotation(currentRotation + rotDiff * lerpFactor);
  }

  // 炮塔角度插值
  float currentTurret = getTurretRotation();
  float turretDiff = m_networkTargetTurretAngle - currentTurret;
  while (turretDiff > 180.f)
    turretDiff -= 360.f;
  while (turretDiff < -180.f)
    turretDiff += 360.f;

  if (std::abs(turretDiff) > 1.f)
  {
    setTurretRotation(currentTurret + turretDiff * lerpFactor);
  }
}
