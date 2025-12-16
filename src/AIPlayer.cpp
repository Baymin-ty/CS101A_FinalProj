#include "include/AIPlayer.hpp"
#include "include/Utils.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <fstream>
#include <sstream>

// ============================================================================
// RuleBasedAI Implementation (基线AI)
// ============================================================================

AIAction RuleBasedAI::decide(const AIObservation& obs)
{
  AIAction action;
  action.moveX = 0.f;
  action.moveY = 0.f;
  action.turretAngle = obs.turretRotation;
  action.shoot = false;
  action.activateNPC = false;
  
  // 更新冷却时间
  if (m_shootCooldown > 0.f) {
    m_shootCooldown -= obs.deltaTime;
  }
  
  // 更新躲避摆动计时器（用于左右摆动躲子弹）
  m_dodgeTimer += obs.deltaTime;
  if (m_dodgeTimer > m_dodgeInterval) {
    m_dodgeTimer = 0.f;
    m_dodgeDirection *= -1;
    // 随机化下次摆动间隔（0.2-0.5秒）
    m_dodgeInterval = 0.2f + static_cast<float>(rand() % 30) / 100.f;
  }
  
  // 更新侧向移动定时器（用于战斗中的横向移动）
  m_lateralTimer += obs.deltaTime;
  if (m_lateralTimer > 1.5f) {
    m_lateralTimer = 0.f;
  }
  
  // 检测是否被困
  float distMoved = std::hypot(obs.position.x - m_lastPosition.x, 
                                obs.position.y - m_lastPosition.y);
  if (distMoved < 1.f) {
    m_stuckTimer += obs.deltaTime;
  } else {
    m_stuckTimer = 0.f;
  }
  m_lastPosition = obs.position;
  
  const float EXIT_DANGER_ZONE = 60.f;
  
  // ==========================================================
  // 优先级1：远离终点（核心约束 - 最高优先级）
  // ==========================================================
  if (obs.exitDistance < EXIT_DANGER_ZONE) {
    sf::Vector2f awayFromExit = obs.position - obs.exitPosition;
    float len = std::hypot(awayFromExit.x, awayFromExit.y);
    if (len > 0.f) {
      action.moveX = awayFromExit.x / len;
      action.moveY = awayFromExit.y / len;
    }
    // 逃离时也要攻击
    if (obs.hasBestTarget && obs.canShootTarget) {
      action.turretAngle = getOptimalTurretAngle(obs);
      action.shoot = shouldShoot(obs);
    }
    return action;
  }
  
  // ==========================================================
  // 优先级2：躲避非常近的敌方子弹（只躲即将命中的）
  // ==========================================================
  if (shouldDodge(obs)) {
    sf::Vector2f dodgeDir = getDodgeDirection(obs);
    
    // 躲避时也要继续追击！混合躲避和追击方向
    if (obs.hasBestTarget) {
      sf::Vector2f toTarget = obs.bestTarget - obs.position;
      float dist = std::hypot(toTarget.x, toTarget.y);
      if (dist > 0.f) {
        toTarget.x /= dist;
        toTarget.y /= dist;
        // 70%躲避 + 30%追击
        action.moveX = dodgeDir.x * 0.7f + toTarget.x * 0.3f;
        action.moveY = dodgeDir.y * 0.7f + toTarget.y * 0.3f;
      } else {
        action.moveX = dodgeDir.x;
        action.moveY = dodgeDir.y;
      }
    } else {
      action.moveX = dodgeDir.x;
      action.moveY = dodgeDir.y;
    }
    
    // 躲避时也尝试瞄准和射击
    if (obs.hasBestTarget && obs.canShootTarget) {
      action.turretAngle = getOptimalTurretAngle(obs);
      action.shoot = shouldShoot(obs);
    }
    return action;
  }
  
  // ==========================================================
  // 优先级3：激活附近的中立NPC（花费金币获得帮手）
  // ==========================================================
  if (obs.hasNearbyNeutralNPC && obs.coins >= 3) {
    action.activateNPC = true;
    // 靠近中立NPC
    sf::Vector2f toNPC = obs.nearestNeutralNPCPos - obs.position;
    float npcDist = std::hypot(toNPC.x, toNPC.y);
    if (npcDist > 30.f) {
      action.moveX = toNPC.x / npcDist;
      action.moveY = toNPC.y / npcDist;
    }
    // 同时攻击
    if (obs.hasBestTarget && obs.canShootTarget) {
      action.turretAngle = getOptimalTurretAngle(obs);
      action.shoot = shouldShoot(obs);
    }
    return action;
  }
  
  // ==========================================================
  // 优先级4：如果被困太久，换方向并强制移动
  // ==========================================================
  if (m_stuckTimer > 1.0f) {
    m_dodgeDirection *= -1;
    m_stuckTimer = 0.f;
    
    // 被困时，如果有目标就朝目标方向强制移动（使用路径）
    if (obs.hasBestTarget) {
      action.turretAngle = getOptimalTurretAngle(obs);
      
      // 使用路径规划移动
      if (obs.hasPathToEnemy && !obs.pathToEnemy.empty()) {
        sf::Vector2f toWaypoint = obs.nextWaypoint - obs.position;
        float waypointDist = std::hypot(toWaypoint.x, toWaypoint.y);
        if (waypointDist > 1.f) {
          action.moveX = toWaypoint.x / waypointDist;
          action.moveY = toWaypoint.y / waypointDist;
        }
      } else {
        // 没有路径，直接朝向目标
        sf::Vector2f toTarget = obs.bestTarget - obs.position;
        float dist = std::hypot(toTarget.x, toTarget.y);
        if (dist > 1.f) {
          action.moveX = toTarget.x / dist;
          action.moveY = toTarget.y / dist;
        }
      }
      
      if (obs.canShootTarget) {
        action.shoot = shouldShoot(obs);
      }
      return action;
    }
    
    // 没有目标，选择墙距离最远的方向
    int bestDir = 0;
    float maxDist = 0.f;
    for (int i = 0; i < 8; ++i) {
      if (obs.wallDistances[i] > maxDist) {
        maxDist = obs.wallDistances[i];
        bestDir = i;
      }
    }
    float angle = bestDir * 45.f * M_PI / 180.f;
    action.moveX = std::cos(angle);
    action.moveY = std::sin(angle);
    return action;
  }
  
  // ==========================================================
  // 主要行为：像NPC一样追击玩家！
  // ==========================================================
  if (obs.hasBestTarget) {
    sf::Vector2f movement(0.f, 0.f);
    
    // 计算到目标的方向
    sf::Vector2f toPlayer = obs.bestTarget - obs.position;
    float distToPlayer = std::hypot(toPlayer.x, toPlayer.y);
    sf::Vector2f toPlayerDir(0.f, 0.f);
    if (distToPlayer > 0.f) {
      toPlayerDir.x = toPlayer.x / distToPlayer;
      toPlayerDir.y = toPlayer.y / distToPlayer;
    }
    
    // 检测是否与玩家处于同一直线（玩家可能瞄准我们）
    // 如果水平或垂直方向的偏移很小，说明在同一直线上
    bool inLineWithPlayer = false;
    float horizontalOffset = std::abs(toPlayer.x);
    float verticalOffset = std::abs(toPlayer.y);
    const float LINE_THRESHOLD = 40.f;  // 同一直线的阈值（迷宫通道宽度约80）
    
    if ((horizontalOffset < LINE_THRESHOLD && verticalOffset > 50.f) ||
        (verticalOffset < LINE_THRESHOLD && horizontalOffset > 50.f)) {
      inLineWithPlayer = true;
    }
    
    // 沿A*路径移动（和NPC一样）
    if (obs.hasPathToEnemy && !obs.pathToEnemy.empty()) {
      sf::Vector2f toWaypoint = obs.nextWaypoint - obs.position;
      float waypointDist = std::hypot(toWaypoint.x, toWaypoint.y);
      if (waypointDist > 5.f) {
        movement.x = toWaypoint.x / waypointDist;
        movement.y = toWaypoint.y / waypointDist;
      }
    } else {
      // 没有路径，直接朝向目标
      if (distToPlayer > 1.f) {
        movement.x = toPlayerDir.x;
        movement.y = toPlayerDir.y;
      }
    }
    
    // 战斗移动策略 - 更激进！
    if (obs.bulletPathToEnemy == 0) {
      // 有直接视线时的战斗移动
      
      if (distToPlayer < 80.f) {
        // 只有非常近时才后退，同时横向移动
        sf::Vector2f lateral(-toPlayerDir.y * m_dodgeDirection, toPlayerDir.x * m_dodgeDirection);
        movement.x = -toPlayerDir.x * 0.5f + lateral.x * 0.7f;
        movement.y = -toPlayerDir.y * 0.5f + lateral.y * 0.7f;
      } else if (distToPlayer < 120.f) {
        // 近距离，主要横向移动，不后退
        sf::Vector2f lateral(-toPlayerDir.y * m_dodgeDirection, toPlayerDir.x * m_dodgeDirection);
        movement.x = lateral.x * 0.8f + toPlayerDir.x * 0.2f;  // 还继续靠近！
        movement.y = lateral.y * 0.8f + toPlayerDir.y * 0.2f;
      } else if (inLineWithPlayer && distToPlayer < 200.f) {
        // 在同一直线上，横向移动同时追击
        sf::Vector2f lateral(-toPlayerDir.y * m_dodgeDirection, toPlayerDir.x * m_dodgeDirection);
        movement.x = lateral.x * 0.5f + toPlayerDir.x * 0.5f;
        movement.y = lateral.y * 0.5f + toPlayerDir.y * 0.5f;
      }
      // 更远距离继续沿A*路径追击
    } else if (inLineWithPlayer && distToPlayer < 150.f) {
      // 有墙阻挡但在同一直线上且较近，稍微横向移动
      sf::Vector2f lateral(-toPlayerDir.y * m_dodgeDirection, toPlayerDir.x * m_dodgeDirection);
      // 主要追击，稍微横向
      movement.x = movement.x * 0.7f + lateral.x * 0.3f;
      movement.y = movement.y * 0.7f + lateral.y * 0.3f;
    }
    
    // 避免进入终点危险区
    sf::Vector2f predictPos = obs.position + movement * 30.f;
    float predictExitDist = std::hypot(predictPos.x - obs.exitPosition.x,
                                        predictPos.y - obs.exitPosition.y);
    if (predictExitDist < EXIT_DANGER_ZONE) {
      sf::Vector2f awayFromExit = obs.position - obs.exitPosition;
      float len = std::hypot(awayFromExit.x, awayFromExit.y);
      if (len > 0.f) {
        awayFromExit.x /= len;
        awayFromExit.y /= len;
      }
      movement.x = movement.x * 0.3f + awayFromExit.x * 0.7f;
      movement.y = movement.y * 0.3f + awayFromExit.y * 0.7f;
    }
    
    action.moveX = movement.x;
    action.moveY = movement.y;
    
    // 瞄准并射击（和NPC一样的逻辑）
    if (obs.canShootTarget) {
      action.turretAngle = getOptimalTurretAngle(obs);
      action.shoot = shouldShoot(obs);
    } else {
      // 没有有效目标时，炮塔朝向移动方向/目标方向
      sf::Vector2f toTarget = obs.bestTarget - obs.position;
      action.turretAngle = std::atan2(toTarget.y, toTarget.x) * 180.f / M_PI + 90.f;
    }
    
    return action;
  }
  
  // ==========================================================
  // 没有目标时：寻找玩家（朝玩家方向移动）
  // ==========================================================
  if (obs.enemyVisible) {
    sf::Vector2f toEnemy = obs.enemyPosition - obs.position;
    float len = std::hypot(toEnemy.x, toEnemy.y);
    if (len > 0.f) {
      action.moveX = toEnemy.x / len;
      action.moveY = toEnemy.y / len;
    }
    
    // 尝试瞄准玩家
    action.turretAngle = std::atan2(toEnemy.y, toEnemy.x) * 180.f / M_PI + 90.f;
    // 没有目标信息，不盲目射击（有墙阻挡）
    action.shoot = false;
    return action;
  }
  
  // ==========================================================
  // 默认行为：巡逻寻找目标
  // ==========================================================
  int bestDir = 0;
  float maxScore = -1000.f;
  
  for (int i = 0; i < 8; ++i) {
    float angle = i * 45.f * M_PI / 180.f;
    sf::Vector2f dirVec(std::cos(angle), std::sin(angle));
    sf::Vector2f predictPos = obs.position + dirVec * 50.f;
    float predictExitDist = std::hypot(predictPos.x - obs.exitPosition.x,
                                        predictPos.y - obs.exitPosition.y);
    
    float score = obs.wallDistances[i];
    if (predictExitDist < EXIT_DANGER_ZONE) {
      score -= 50.f;
    }
    
    if (score > maxScore) {
      maxScore = score;
      bestDir = i;
    }
  }
  
  float angle = bestDir * 45.f * M_PI / 180.f;
  action.moveX = std::cos(angle);
  action.moveY = std::sin(angle);
  
  return action;
}

void RuleBasedAI::reset()
{
  m_shootCooldown = 0.f;
  m_lastPosition = sf::Vector2f(0.f, 0.f);
  m_stuckTimer = 0.f;
  m_dodgeDirection = 1;
  m_lateralTimer = 0.f;
  m_dodgeTimer = 0.f;
  m_dodgeInterval = 0.3f;
  m_hasValidShootTarget = false;
}

bool RuleBasedAI::shouldDodge(const AIObservation& obs)
{
  // 只躲避非常近且即将命中的子弹（更激进的AI）
  for (size_t i = 0; i < obs.visibleBulletPositions.size(); ++i) {
    // 只躲避敌方子弹
    if (i < obs.bulletIsEnemy.size() && !obs.bulletIsEnemy[i]) {
      continue;
    }
    
    const auto& bulletPos = obs.visibleBulletPositions[i];
    float dist = std::hypot(bulletPos.x - obs.position.x, bulletPos.y - obs.position.y);
    
    // 只检测150像素内的子弹（更近才躲）
    if (dist < 150.f) {
      // 检查子弹是否正在朝我们飞来
      if (i < obs.visibleBulletVelocities.size()) {
        sf::Vector2f bulletVel = obs.visibleBulletVelocities[i];
        sf::Vector2f toMe = obs.position - bulletPos;
        float toMeLen = std::hypot(toMe.x, toMe.y);
        if (toMeLen > 0.f) {
          toMe.x /= toMeLen;
          toMe.y /= toMeLen;
        }
        
        float bulletSpeed = std::hypot(bulletVel.x, bulletVel.y);
        if (bulletSpeed > 0.f) {
          sf::Vector2f bulletDir(bulletVel.x / bulletSpeed, bulletVel.y / bulletSpeed);
          
          // 计算子弹速度方向和到我的方向的点积
          float dot = bulletDir.x * toMe.x + bulletDir.y * toMe.y;
          
          // 点积大于0.5表示子弹直接朝我们飞来（更严格）
          if (dot > 0.5f) {
            // 预测子弹是否会击中我们（计算最近距离）
            sf::Vector2f bulletToMe = obs.position - bulletPos;
            float projLen = bulletToMe.x * bulletDir.x + bulletToMe.y * bulletDir.y;
            sf::Vector2f closestPoint(bulletPos.x + bulletDir.x * projLen, 
                                       bulletPos.y + bulletDir.y * projLen);
            float minDist = std::hypot(closestPoint.x - obs.position.x, 
                                        closestPoint.y - obs.position.y);
            
            // 只有子弹几乎要命中时才躲（50像素内）
            if (minDist < 50.f && projLen > 0.f && projLen < 200.f) {
              return true;
            }
          }
        }
      } else {
        // 没有速度信息，非常近才躲避
        if (dist < 80.f) {
          return true;
        }
      }
    }
  }
  return false;
}

sf::Vector2f RuleBasedAI::getDodgeDirection(const AIObservation& obs)
{
  // 找到最危险的子弹（正在飞向我们的最近子弹）
  sf::Vector2f dangerousBulletPos = obs.position;
  sf::Vector2f dangerousBulletVel(0.f, 0.f);
  float minTimeToHit = 1000000.f;  // 用命中时间而不是距离来判断危险程度
  bool foundDanger = false;
  
  for (size_t i = 0; i < obs.visibleBulletPositions.size(); ++i) {
    // 只考虑敌方子弹
    if (i < obs.bulletIsEnemy.size() && !obs.bulletIsEnemy[i]) {
      continue;
    }
    
    const auto& bulletPos = obs.visibleBulletPositions[i];
    
    if (i < obs.visibleBulletVelocities.size()) {
      sf::Vector2f bulletVel = obs.visibleBulletVelocities[i];
      sf::Vector2f toMe = obs.position - bulletPos;
      float dot = bulletVel.x * toMe.x + bulletVel.y * toMe.y;
      
      if (dot > 0) {  // 正在飞向我们
        float bulletSpeed = std::hypot(bulletVel.x, bulletVel.y);
        if (bulletSpeed > 0.f) {
          float dist = std::hypot(toMe.x, toMe.y);
          float timeToHit = dist / bulletSpeed;
          
          if (timeToHit < minTimeToHit) {
            minTimeToHit = timeToHit;
            dangerousBulletPos = bulletPos;
            dangerousBulletVel = bulletVel;
            foundDanger = true;
          }
        }
      }
    }
  }
  
  if (!foundDanger) {
    // 没有找到危险子弹，使用当前躲避方向横向移动
    return sf::Vector2f(m_dodgeDirection * 1.0f, 0.f);
  }
  
  // 计算垂直于子弹飞行方向的躲避方向
  float bulletSpeed = std::hypot(dangerousBulletVel.x, dangerousBulletVel.y);
  sf::Vector2f bulletDir(0.f, 0.f);
  if (bulletSpeed > 0.f) {
    bulletDir.x = dangerousBulletVel.x / bulletSpeed;
    bulletDir.y = dangerousBulletVel.y / bulletSpeed;
  }
  
  // 垂直于子弹飞行方向（左右摆动）
  sf::Vector2f perpendicular(-bulletDir.y * m_dodgeDirection, bulletDir.x * m_dodgeDirection);
  
  // 检查这个方向是否有墙，如果有就换方向
  // 预测移动后的位置
  sf::Vector2f predictPos = obs.position + perpendicular * 30.f;
  int checkDir = 0;
  if (std::abs(perpendicular.x) > std::abs(perpendicular.y)) {
    checkDir = perpendicular.x > 0 ? 0 : 4;  // 东或西
  } else {
    checkDir = perpendicular.y > 0 ? 2 : 6;  // 南或北
  }
  
  if (obs.wallDistances[checkDir] < 30.f) {
    // 这个方向有墙，换方向
    perpendicular.x = -perpendicular.x;
    perpendicular.y = -perpendicular.y;
  }
  
  // 标准化
  float len = std::hypot(perpendicular.x, perpendicular.y);
  if (len > 0.f) {
    perpendicular.x /= len;
    perpendicular.y /= len;
  }
  
  return perpendicular;
}

// 基于路径规划的移动（学习自NPC）
sf::Vector2f RuleBasedAI::getPathFollowingMovement(const AIObservation& obs)
{
  sf::Vector2f movement(0.f, 0.f);
  
  // 如果有路径，沿路径移动
  if (obs.hasPathToEnemy && !obs.pathToEnemy.empty()) {
    sf::Vector2f toWaypoint = obs.nextWaypoint - obs.position;
    float dist = std::hypot(toWaypoint.x, toWaypoint.y);
    
    if (dist > 0.f) {
      movement.x = toWaypoint.x / dist;
      movement.y = toWaypoint.y / dist;
    }
  } else if (obs.hasBestTarget) {
    // 没有路径，直接朝向最佳目标
    sf::Vector2f toTarget = obs.bestTarget - obs.position;
    float dist = std::hypot(toTarget.x, toTarget.y);
    if (dist > 0.f) {
      movement.x = toTarget.x / dist;
      movement.y = toTarget.y / dist;
    }
  }
  
  return movement;
}

// 当有墙阻挡时，寻找可以射击的位置
sf::Vector2f RuleBasedAI::getFindShootPositionMovement(const AIObservation& obs)
{
  if (!obs.hasBestTarget) return sf::Vector2f(0.f, 0.f);
  
  sf::Vector2f movement(0.f, 0.f);
  
  // 优先使用路径规划
  if (obs.hasPathToEnemy && !obs.pathToEnemy.empty()) {
    sf::Vector2f toWaypoint = obs.nextWaypoint - obs.position;
    float waypointDist = std::hypot(toWaypoint.x, toWaypoint.y);
    if (waypointDist > 10.f) {
      movement.x = toWaypoint.x / waypointDist;
      movement.y = toWaypoint.y / waypointDist;
      return movement;
    }
  }
  
  // 没有路径或已到达路径点，使用绕行策略
  sf::Vector2f toTarget = obs.bestTarget - obs.position;
  float dist = std::hypot(toTarget.x, toTarget.y);
  
  if (dist < 1.f) return sf::Vector2f(0.f, 0.f);
  
  sf::Vector2f toTargetNorm(toTarget.x / dist, toTarget.y / dist);
  
  // 绕着目标移动，同时靠近
  sf::Vector2f lateral(-toTargetNorm.y * m_dodgeDirection, toTargetNorm.x * m_dodgeDirection);
  
  // 如果距离较远，以接近为主
  if (dist > 300.f) {
    movement.x = toTargetNorm.x * 0.9f + lateral.x * 0.3f;
    movement.y = toTargetNorm.y * 0.9f + lateral.y * 0.3f;
  } else if (dist > 150.f) {
    movement.x = toTargetNorm.x * 0.6f + lateral.x * 0.6f;
    movement.y = toTargetNorm.y * 0.6f + lateral.y * 0.6f;
  } else {
    // 近距离主要侧向移动
    movement.x = toTargetNorm.x * 0.3f + lateral.x * 0.8f;
    movement.y = toTargetNorm.y * 0.3f + lateral.y * 0.8f;
  }
  
  // 检查移动方向是否有墙
  float moveAngle = std::atan2(movement.y, movement.x);
  int dirIndex = static_cast<int>((moveAngle * 180.f / M_PI + 180.f + 22.5f) / 45.f) % 8;
  if (dirIndex < 0) dirIndex += 8;
  
  if (obs.wallDistances[dirIndex] < 50.f) {
    m_dodgeDirection *= -1;
    lateral.x = -lateral.x;
    lateral.y = -lateral.y;
    movement.x = toTargetNorm.x * 0.5f + lateral.x * 0.7f;
    movement.y = toTargetNorm.y * 0.5f + lateral.y * 0.7f;
  }
  
  return movement;
}

// 战斗中的移动策略（中距离战斗）
sf::Vector2f RuleBasedAI::getCombatMovement(const AIObservation& obs)
{
  if (!obs.hasBestTarget) return sf::Vector2f(0.f, 0.f);
  
  sf::Vector2f toTarget = obs.bestTarget - obs.position;
  float dist = std::hypot(toTarget.x, toTarget.y);
  
  if (dist > 0.f) {
    toTarget.x /= dist;
    toTarget.y /= dist;
  }
  
  // 根据是否能直接射击决定移动策略
  if (obs.canShootTarget) {
    // 可以直接射击，保持距离边打边动
    if (dist < 150.f) {
      // 太近，后退 + 侧向移动
      sf::Vector2f lateral(-toTarget.y * m_dodgeDirection, toTarget.x * m_dodgeDirection);
      return sf::Vector2f(-toTarget.x * 0.4f + lateral.x * 0.6f, -toTarget.y * 0.4f + lateral.y * 0.6f);
    } else if (dist < 250.f) {
      // 合适距离，侧向移动保持射击
      sf::Vector2f lateral(-toTarget.y * m_dodgeDirection, toTarget.x * m_dodgeDirection);
      return sf::Vector2f(toTarget.x * 0.2f + lateral.x * 0.8f, toTarget.y * 0.2f + lateral.y * 0.8f);
    } else {
      // 较远，接近但带侧向移动
      sf::Vector2f lateral(-toTarget.y * m_dodgeDirection * 0.3f, toTarget.x * m_dodgeDirection * 0.3f);
      return sf::Vector2f(toTarget.x * 0.8f + lateral.x, toTarget.y * 0.8f + lateral.y);
    }
  } else if (obs.bulletPathToEnemy == 1) {
    // 有可破坏墙阻挡
    // 如果正在打墙，可以停下来专心打墙
    if (obs.hasDestructibleWallOnPath) {
      // 面向要打的墙，稍微移动找更好的射击角度
      sf::Vector2f toWall = obs.destructibleWallTarget - obs.position;
      float wallDist = std::hypot(toWall.x, toWall.y);
      if (wallDist > 0.f) {
        toWall.x /= wallDist;
        toWall.y /= wallDist;
      }
      // 稍微接近墙以获得更好的射击角度
      if (wallDist > 150.f) {
        return toWall;
      } else {
        // 距离合适，侧向移动找角度
        sf::Vector2f lateral(-toWall.y * m_dodgeDirection * 0.3f, toWall.x * m_dodgeDirection * 0.3f);
        return lateral;
      }
    }
    // 没有特别要打的墙，继续沿路径移动
    return getPathFollowingMovement(obs);
  } else {
    // 有不可破坏墙阻挡，必须绕路
    return getPathFollowingMovement(obs);
  }
}

bool RuleBasedAI::shouldShoot(const AIObservation& obs)
{
  if (m_shootCooldown > 0.f) return false;
  if (!obs.hasBestTarget) return false;
  
  // 只有在能直接射击目标时才开火（无墙阻挡）
  if (!obs.canShootTarget) {
    return false;  // 有墙阻挡，不射击
  }
  
  // 计算炮塔指向射击目标的角度差 - 使用Utils::getAngle的计算方式
  sf::Vector2f toTarget = obs.shootTarget - obs.position;
  float angleToTarget = std::atan2(toTarget.y, toTarget.x) * 180.f / M_PI + 90.f;
  float angleDiff = angleToTarget - obs.turretRotation;
  
  // 归一化角度差到[-180, 180]
  while (angleDiff > 180.f) angleDiff -= 360.f;
  while (angleDiff < -180.f) angleDiff += 360.f;
  
  // 根据目标距离调整精度要求
  float maxAngleDiff = 20.f;
  float targetDist = std::hypot(toTarget.x, toTarget.y);
  
  if (targetDist < 150.f) {
    maxAngleDiff = 35.f;  // 近距离更宽松
  } else if (targetDist < 300.f) {
    maxAngleDiff = 25.f;  // 中距离
  } else {
    maxAngleDiff = 15.f;  // 远距离需要更精确
  }
  
  // 瞄准精度在容忍度以内
  if (std::abs(angleDiff) < maxAngleDiff) {
    m_shootCooldown = 0.2f;  // 快速射击
    return true;
  }
  
  return false;
}

float RuleBasedAI::getOptimalTurretAngle(const AIObservation& obs)
{
  if (!obs.hasBestTarget) return obs.turretRotation;
  
  // 瞄准最佳射击目标 - 使用Utils::getAngle的计算方式（+90度偏移）
  sf::Vector2f toTarget = obs.shootTarget - obs.position;
  return std::atan2(toTarget.y, toTarget.x) * 180.f / M_PI + 90.f;
}

// ============================================================================
// RLAgent Implementation (强化学习AI)
// ============================================================================

RLAgent::RLAgent()
{
  // 初始化fallback AI
  m_fallbackAI.reset();
}

RLAgent::~RLAgent() = default;

AIAction RLAgent::decide(const AIObservation& obs)
{
  // 如果模型未加载，使用规则AI
  if (!m_network.loaded) {
    return m_fallbackAI.decide(obs);
  }
  
  // 转换观察为向量
  std::vector<float> obsVec = observationToVector(obs);
  
  // Epsilon-greedy探索
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dis(0.f, 1.f);
  
  if (m_training && dis(gen) < m_epsilon) {
    // 探索：使用规则AI或随机动作
    if (dis(gen) < 0.5f) {
      return m_fallbackAI.decide(obs);
    } else {
      // 随机动作
      AIAction action;
      action.moveX = dis(gen) * 2.f - 1.f;
      action.moveY = dis(gen) * 2.f - 1.f;
      float len = std::hypot(action.moveX, action.moveY);
      if (len > 0.f) {
        action.moveX /= len;
        action.moveY /= len;
      }
      action.turretAngle = dis(gen) * 360.f;
      action.shoot = dis(gen) < 0.2f;
      action.activateNPC = dis(gen) < 0.1f;
      return action;
    }
  }
  
  // 利用：使用神经网络
  std::vector<float> actionVec = forward(obsVec);
  return vectorToAction(actionVec);
}

void RLAgent::reset()
{
  // 重置状态
  m_fallbackAI.reset();
}

void RLAgent::recordTransition(const AIObservation& obs, const AIAction& action,
                               float reward, bool done, const AIObservation& nextObs)
{
  if (!m_training) return;
  
  RLTrainingData data;
  data.observation = observationToVector(obs);
  data.action = {action.moveX, action.moveY, action.turretAngle / 360.f, 
                 action.shoot ? 1.f : 0.f, action.activateNPC ? 1.f : 0.f};
  data.reward = reward;
  data.done = done;
  data.nextObservation = observationToVector(nextObs);
  
  m_trainingBuffer.push_back(data);
}

bool RLAgent::loadModel(const std::string& path)
{
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "[AI] Failed to open model file: " << path << std::endl;
    return false;
  }
  
  // 简单的文本格式：每行是一个权重或偏置
  // 实际应该用更好的格式（如JSON或二进制）
  m_network.weights.clear();
  m_network.biases.clear();
  
  // TODO: 实现实际的模型加载逻辑
  // 这里需要与Python训练脚本的保存格式匹配
  
  m_network.loaded = true;
  std::cout << "[AI] Model loaded from: " << path << std::endl;
  return true;
}

bool RLAgent::saveModel(const std::string& path)
{
  std::ofstream file(path);
  if (!file.is_open()) {
    std::cerr << "[AI] Failed to open model file for writing: " << path << std::endl;
    return false;
  }
  
  // TODO: 实现实际的模型保存逻辑
  
  std::cout << "[AI] Model saved to: " << path << std::endl;
  return true;
}

std::vector<float> RLAgent::observationToVector(const AIObservation& obs)
{
  std::vector<float> vec;
  vec.reserve(64);  // 预估大小
  
  // 自身状态 (归一化到[-1, 1]或[0, 1])
  vec.push_back(obs.position.x / 1000.f);
  vec.push_back(obs.position.y / 1000.f);
  vec.push_back(std::cos(obs.rotation * M_PI / 180.f));
  vec.push_back(std::sin(obs.rotation * M_PI / 180.f));
  vec.push_back(std::cos(obs.turretRotation * M_PI / 180.f));
  vec.push_back(std::sin(obs.turretRotation * M_PI / 180.f));
  vec.push_back(obs.health / 100.f);
  
  // 敌人信息
  if (obs.enemyVisible) {
    vec.push_back(1.f);
    vec.push_back((obs.enemyPosition.x - obs.position.x) / 1000.f);
    vec.push_back((obs.enemyPosition.y - obs.position.y) / 1000.f);
    vec.push_back(obs.enemyHealth / 100.f);
    vec.push_back(obs.enemyDistance / 1000.f);
  } else {
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
  }
  
  // 墙壁距离 (8个方向)
  for (float dist : obs.wallDistances) {
    vec.push_back(dist / 200.f);  // 归一化
  }
  
  // 出口信息
  vec.push_back((obs.exitPosition.x - obs.position.x) / 1000.f);
  vec.push_back((obs.exitPosition.y - obs.position.y) / 1000.f);
  vec.push_back(obs.exitDistance / 1000.f);
  
  // NPC信息（最多考虑5个最近的）
  int npcCount = std::min(5, static_cast<int>(obs.visibleNPCPositions.size()));
  for (int i = 0; i < npcCount; ++i) {
    vec.push_back((obs.visibleNPCPositions[i].x - obs.position.x) / 1000.f);
    vec.push_back((obs.visibleNPCPositions[i].y - obs.position.y) / 1000.f);
    vec.push_back(obs.visibleNPCTeams[i] / 2.f);  // 0, 0.5, 1
    vec.push_back(obs.visibleNPCHealths[i] / 100.f);
  }
  // 补齐
  for (int i = npcCount; i < 5; ++i) {
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
  }
  
  // 子弹信息（最多考虑3个最近的）
  int bulletCount = std::min(3, static_cast<int>(obs.visibleBulletPositions.size()));
  for (int i = 0; i < bulletCount; ++i) {
    vec.push_back((obs.visibleBulletPositions[i].x - obs.position.x) / 1000.f);
    vec.push_back((obs.visibleBulletPositions[i].y - obs.position.y) / 1000.f);
    vec.push_back(obs.visibleBulletVelocities[i].x / 500.f);
    vec.push_back(obs.visibleBulletVelocities[i].y / 500.f);
  }
  // 补齐
  for (int i = bulletCount; i < 3; ++i) {
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
    vec.push_back(0.f);
  }
  
  return vec;
}

AIAction RLAgent::vectorToAction(const std::vector<float>& actionVec)
{
  AIAction action;
  
  if (actionVec.size() >= 5) {
    action.moveX = std::tanh(actionVec[0]);
    action.moveY = std::tanh(actionVec[1]);
    action.turretAngle = actionVec[2] * 360.f;
    action.shoot = actionVec[3] > 0.5f;
    action.activateNPC = actionVec[4] > 0.5f;
  } else {
    // 默认动作
    action.moveX = 0.f;
    action.moveY = 0.f;
    action.turretAngle = 0.f;
    action.shoot = false;
    action.activateNPC = false;
  }
  
  return action;
}

std::vector<float> RLAgent::forward(const std::vector<float>& input)
{
  // 简单的前向传播（实际应该使用更复杂的网络）
  // 这里只是占位符，实际推理可能需要调用Python或使用C++推理库
  
  if (m_network.weights.empty()) {
    // 返回随机动作
    return {0.f, 0.f, 0.f, 0.f, 0.f};
  }
  
  // TODO: 实现实际的神经网络前向传播
  // 可以使用libtorch或其他C++推理库
  
  return {0.f, 0.f, 0.f, 0.f, 0.f};
}

// ============================================================================
// AIPlayer Implementation
// ============================================================================

AIPlayer::AIPlayer(std::shared_ptr<Tank> tank, AIStrategy* strategy)
  : m_tank(tank), m_strategy(strategy)
{
}

void AIPlayer::setEnvironment(Maze* maze, Tank* opponent,
                             std::vector<std::unique_ptr<Enemy>>* npcs,
                             std::vector<std::unique_ptr<Bullet>>* bullets)
{
  m_maze = maze;
  m_opponent = opponent;
  m_npcs = npcs;
  m_bullets = bullets;
}

void AIPlayer::update(float dt)
{
  if (!m_tank || !m_strategy) return;
  
  // 生成观察
  AIObservation obs = getObservation();
  obs.deltaTime = dt;
  
  // 执行决策
  AIAction action = m_strategy->decide(obs);
  
  // 应用动作
  applyAction(action, dt);
  
  // 记录（用于训练）
  m_lastObservation = obs;
  m_lastAction = action;
}

AIObservation AIPlayer::getObservation() const
{
  AIObservation obs;
  
  if (!m_tank) return obs;
  
  // 自身状态
  obs.position = m_tank->getPosition();
  obs.rotation = m_tank->getRotation();
  obs.turretRotation = m_tank->getTurretRotation();
  obs.health = m_tank->getHealth();
  obs.coins = m_tank->getCoins();
  obs.myTeam = m_tank->getTeam();
  
  // 初始化目标信息
  obs.hasBestTarget = false;
  obs.bestTargetDistance = 999999.f;
  obs.hasNearbyNeutralNPC = false;
  obs.nearestNeutralNPCDist = 999999.f;
  obs.canShootTarget = false;
  
  // 对手信息（全知）
  obs.enemyVisible = false;
  if (m_opponent && !m_opponent->isDead()) {
    sf::Vector2f enemyPos = m_opponent->getPosition();
    float dist = std::hypot(enemyPos.x - obs.position.x, enemyPos.y - obs.position.y);
    
    // AI全知，总能看到对手
    obs.enemyVisible = true;
    obs.enemyPosition = enemyPos;
    obs.enemyHealth = m_opponent->getHealth();
    obs.enemyDistance = dist;
    
    // 玩家是敌对目标
    obs.allEnemyTargets.push_back(enemyPos);
    obs.allEnemyDistances.push_back(dist);
    
    // 玩家优先作为最佳目标
    if (dist < obs.bestTargetDistance) {
      obs.bestTarget = enemyPos;
      obs.bestTargetDistance = dist;
      obs.hasBestTarget = true;
    }
  }
  
  // NPC信息（全知）
  if (m_npcs) {
    for (const auto& npc : *m_npcs) {
      if (!npc || npc->isDead()) continue;
      
      sf::Vector2f npcPos = npc->getPosition();
      int npcTeam = npc->getTeam();
      float dist = std::hypot(npcPos.x - obs.position.x, npcPos.y - obs.position.y);
      
      obs.visibleNPCPositions.push_back(npcPos);
      obs.visibleNPCTeams.push_back(npcTeam);
      obs.visibleNPCHealths.push_back(npc->getHealth());
      
      // 中立NPC（可激活）
      if (npcTeam == 0 && !npc->isActivated()) {
        if (dist < 80.f && dist < obs.nearestNeutralNPCDist) {
          obs.hasNearbyNeutralNPC = true;
          obs.nearestNeutralNPCPos = npcPos;
          obs.nearestNeutralNPCDist = dist;
        }
      }
      // 敌对NPC（玩家阵营的NPC）
      else if (npcTeam != 0 && npcTeam != obs.myTeam && npc->isActivated()) {
        obs.allEnemyTargets.push_back(npcPos);
        obs.allEnemyDistances.push_back(dist);
        
        // NPC作为候选目标（如果比当前最佳目标更近且玩家不可见或很远）
        if (dist < obs.bestTargetDistance * 0.7f) {
          obs.bestTarget = npcPos;
          obs.bestTargetDistance = dist;
          obs.hasBestTarget = true;
        }
      }
    }
  }
  
  // 子弹信息（全知）- 识别敌方子弹
  if (m_bullets) {
    for (const auto& bullet : *m_bullets) {
      if (!bullet || !bullet->isAlive()) continue;
      
      sf::Vector2f bulletPos = bullet->getPosition();
      obs.visibleBulletPositions.push_back(bulletPos);
      obs.visibleBulletVelocities.push_back(bullet->getVelocity());
      // 敌方子弹 = 不是我方阵营的子弹
      obs.bulletIsEnemy.push_back(bullet->getTeam() != obs.myTeam);
    }
  }
  
  // 墙壁距离（8方向射线检测）
  obs.wallDistances = calculateWallDistances();
  
  // 出口信息
  if (m_maze) {
    obs.exitPosition = m_maze->getExitPosition();
    obs.exitDistance = std::hypot(obs.exitPosition.x - obs.position.x,
                                   obs.exitPosition.y - obs.position.y);
    obs.exitAngle = std::atan2(obs.exitPosition.y - obs.position.y,
                                obs.exitPosition.x - obs.position.x) * 180.f / M_PI;
  }
  
  // 路径规划信息（完全复制NPC的逻辑）
  obs.hasPathToEnemy = false;
  obs.hasDestructibleWallOnPath = false;
  obs.bulletPathToEnemy = 2;  // 默认假设最差情况
  obs.nextWaypoint = obs.hasBestTarget ? obs.bestTarget : obs.position;
  obs.shootTarget = obs.hasBestTarget ? obs.bestTarget : obs.position;
  
  if (m_maze && obs.hasBestTarget) {
    // 定期更新路径（像NPC一样使用缓存）
    bool needPathUpdate = m_cachedPath.empty() || 
                          m_pathUpdateClock.getElapsedTime().asSeconds() > m_pathUpdateInterval;
    
    if (needPathUpdate) {
      // 首先尝试普通路径
      auto normalPath = m_maze->findPath(obs.position, obs.bestTarget);
      
      // 然后尝试穿过可破坏墙的路径
      auto smartPathResult = m_maze->findPathThroughDestructible(obs.position, obs.bestTarget, 10.0f);
      
      // 比较两条路径，选择更优的（和NPC完全一样的逻辑）
      bool useSmartPath = false;
      
      if (!smartPathResult.path.empty()) {
        if (normalPath.empty()) {
          // 普通路径找不到，使用智能路径
          useSmartPath = true;
        } else if (smartPathResult.hasDestructibleWall) {
          // 如果智能路径穿过可破坏墙，比较实际长度
          float normalLen = static_cast<float>(normalPath.size());
          float smartLen = static_cast<float>(smartPathResult.path.size());
          
          // 如果智能路径比普通路径短50%以上，使用智能路径
          if (smartLen < normalLen * 0.5f) {
            useSmartPath = true;
          }
        }
      }
      
      if (useSmartPath) {
        m_cachedPath = smartPathResult.path;
        m_hasDestructibleWallOnPath = smartPathResult.hasDestructibleWall;
        m_destructibleWallTarget = smartPathResult.firstDestructibleWallPos;
      } else {
        m_cachedPath = normalPath;
        m_hasDestructibleWallOnPath = false;
        m_destructibleWallTarget = {0.f, 0.f};
      }
      
      m_currentPathIndex = 0;
      m_pathUpdateClock.restart();
    }
    
    // 复制缓存路径到观察
    obs.pathToEnemy = m_cachedPath;
    obs.hasDestructibleWallOnPath = m_hasDestructibleWallOnPath;
    obs.destructibleWallTarget = m_destructibleWallTarget;
    obs.hasPathToEnemy = !m_cachedPath.empty();
    
    // 沿路径移动（像NPC一样追踪路径点）
    if (obs.hasPathToEnemy && m_currentPathIndex < m_cachedPath.size()) {
      obs.nextWaypoint = m_cachedPath[m_currentPathIndex];
      
      // 检查是否接近当前路径点
      sf::Vector2f toWaypoint = obs.nextWaypoint - obs.position;
      float distToWaypoint = std::hypot(toWaypoint.x, toWaypoint.y);
      
      if (distToWaypoint < 20.f) {
        // 移动到下一个路径点
        m_currentPathIndex++;
        if (m_currentPathIndex < m_cachedPath.size()) {
          obs.nextWaypoint = m_cachedPath[m_currentPathIndex];
        }
      }
    }
    
    // 检测子弹路径到最佳目标 - 从坦克位置开始检测
    obs.bulletPathToEnemy = m_maze->checkBulletPath(obs.position, obs.bestTarget);
    
    // 决定最佳射击目标和是否可以射击（和NPC完全一样的逻辑）
    obs.canShootTarget = false;  // 默认不能射击
    
    // 打墙锁定逻辑：如果正在打墙，检查墙是否还存在
    if (m_isShootingWall) {
      // 检查目标墙是否还存在（再次检测该位置的路径）
      int wallCheck = m_maze->checkBulletPath(obs.position, m_wallShootTarget);
      
      // 如果路径清晰了（墙被打掉了）或者打墙超时（3秒），解除锁定
      if (wallCheck == 0 || m_wallShootClock.getElapsedTime().asSeconds() > 3.0f) {
        m_isShootingWall = false;
      } else {
        // 继续打墙
        obs.shootTarget = m_wallShootTarget;
        obs.canShootTarget = true;
        obs.bulletPathToEnemy = 1;  // 保持打墙状态
      }
    }
    
    // 如果没有锁定打墙，使用NPC的判断逻辑
    if (!m_isShootingWall) {
      if (obs.bulletPathToEnemy == 0) {
        // 无阻挡，直接瞄准目标
        obs.shootTarget = obs.bestTarget;
        obs.canShootTarget = true;
      } else if (obs.bulletPathToEnemy == 1) {
        // 子弹会先命中可破坏墙
        // 只有当智能路径更优（正在使用穿墙路径）时才打墙
        if (m_hasDestructibleWallOnPath) {
          // 当前正在使用智能路径（穿墙路径更优），射击阻挡的墙
          obs.shootTarget = m_maze->getFirstBlockedPosition(obs.position, obs.bestTarget);
          obs.canShootTarget = true;
          
          // 锁定打墙目标
          m_isShootingWall = true;
          m_wallShootTarget = obs.shootTarget;
          m_wallShootClock.restart();
        } else {
          // 当前使用普通路径（绕路更优），不要射击墙，继续移动
          obs.canShootTarget = false;
        }
      } else {
        // 子弹会先命中不可破坏墙，不能直接射击
        // 检查是否有智能路径上的可破坏墙可以攻击（和NPC一样）
        if (m_hasDestructibleWallOnPath) {
          // 检查子弹是否能打到智能路径上的可破坏墙
          int bulletToWall = m_maze->checkBulletPath(obs.position, m_destructibleWallTarget);
          if (bulletToWall != 2) {  // 不会被不可破坏墙挡住
            if (bulletToWall == 0) {
              // 可以直接打到目标墙
              obs.shootTarget = m_destructibleWallTarget;
              obs.canShootTarget = true;
              
              // 锁定打墙目标
              m_isShootingWall = true;
              m_wallShootTarget = obs.shootTarget;
              m_wallShootClock.restart();
            } else {
              // 会先打到其他可破坏墙
              obs.shootTarget = m_maze->getFirstBlockedPosition(obs.position, m_destructibleWallTarget);
              obs.canShootTarget = true;
              
              // 锁定打墙目标
              m_isShootingWall = true;
              m_wallShootTarget = obs.shootTarget;
              m_wallShootClock.restart();
            }
          }
        }
        // 如果还是没有有效目标，不射击，继续移动寻找更好的位置
      }
    }
    
    // 调试输出
    static int debugCounter = 0;
    if (++debugCounter % 60 == 0) {  // 每秒输出一次
      std::cout << "[AI Debug] path=" << obs.bulletPathToEnemy 
                << " pathLen=" << m_cachedPath.size()
                << " pathIdx=" << m_currentPathIndex
                << " hasWall=" << m_hasDestructibleWallOnPath
                << std::endl;
    }
  }
  
  return obs;
}

void AIPlayer::applyAction(const AIAction& action, float dt)
{
  if (!m_tank) return;
  
  // 模拟键盘输入
  sf::Vector2f movement(action.moveX, action.moveY);
  float moveLen = std::hypot(movement.x, movement.y);
  if (moveLen > 1.f) {
    movement.x /= moveLen;
    movement.y /= moveLen;
  }
  
  // 计算移动角度并应用移动
  if (moveLen > 0.1f) {
    float targetAngle = std::atan2(movement.y, movement.x) * 180.f / M_PI;
    m_tank->setRotation(targetAngle);
    
    // 应用移动（使用和玩家一样的墙壁滑动逻辑）
    sf::Vector2f oldPos = m_tank->getPosition();
    sf::Vector2f moveVec = movement * 200.f * dt;  // 200是移动速度
    sf::Vector2f newPos = oldPos + moveVec;
    float radius = m_tank->getCollisionRadius();
    
    // 碰撞检测 - 实现墙壁滑动
    if (m_maze && m_maze->checkCollision(newPos, radius)) {
      // 尝试只在 X 方向移动
      sf::Vector2f posX = {oldPos.x + moveVec.x, oldPos.y};
      // 尝试只在 Y 方向移动
      sf::Vector2f posY = {oldPos.x, oldPos.y + moveVec.y};
      
      bool canMoveX = !m_maze->checkCollision(posX, radius);
      bool canMoveY = !m_maze->checkCollision(posY, radius);
      
      if (canMoveX && canMoveY) {
        // 两个方向都能走，选择移动量大的
        if (std::abs(moveVec.x) > std::abs(moveVec.y))
          m_tank->setPosition(posX);
        else
          m_tank->setPosition(posY);
      } else if (canMoveX) {
        m_tank->setPosition(posX);
      } else if (canMoveY) {
        m_tank->setPosition(posY);
      }
      // 两个方向都不能走，保持原位
    } else {
      m_tank->setPosition(newPos);
    }
  }
  
  // 设置炮塔角度
  m_tank->setTurretRotation(action.turretAngle);
  
  // 射击
  if (action.shoot && m_shootCooldown <= 0.f) {
    // 这里需要触发Tank的射击逻辑
    // 由于Tank的射击是基于鼠标输入，我们需要在Game层面处理
    // 标记AI想要射击，由外部检测并创建子弹
    m_shootCooldown = SHOOT_COOLDOWN_TIME;
  }
  
  if (m_shootCooldown > 0.f) {
    m_shootCooldown -= dt;
  }
  
  // 激活NPC（模拟按R键）
  // 这也需要在Game层面处理
}

void AIPlayer::reset()
{
  m_shootCooldown = 0.f;
  m_lastObservation = AIObservation();
  m_lastAction = AIAction();
  m_lastReward = 0.f;
  
  // 重置路径缓存
  m_cachedPath.clear();
  m_currentPathIndex = 0;
  m_hasDestructibleWallOnPath = false;
  m_destructibleWallTarget = {0.f, 0.f};
  m_pathUpdateClock.restart();
  
  // 重置打墙锁定状态
  m_isShootingWall = false;
  m_wallShootTarget = {0.f, 0.f};
  
  if (m_strategy) {
    m_strategy->reset();
  }
}

bool AIPlayer::isInVisionRange(const sf::Vector2f& pos) const
{
  if (!m_tank) return false;
  
  sf::Vector2f myPos = m_tank->getPosition();
  float dist = std::hypot(pos.x - myPos.x, pos.y - myPos.y);
  
  return dist <= m_visionRange;
}

bool AIPlayer::hasLineOfSight(const sf::Vector2f& from, const sf::Vector2f& to) const
{
  if (!m_maze) return true;
  
  // 简单的射线检测
  float dist = std::hypot(to.x - from.x, to.y - from.y);
  int steps = static_cast<int>(dist / 10.f);  // 每10像素检测一次
  
  for (int i = 0; i <= steps; ++i) {
    float t = static_cast<float>(i) / steps;
    sf::Vector2f checkPos(
      from.x + (to.x - from.x) * t,
      from.y + (to.y - from.y) * t
    );
    
    if (m_maze->checkCollision(checkPos, 5.f)) {
      return false;  // 被墙阻挡
    }
  }
  
  return true;
}

std::array<float, 8> AIPlayer::calculateWallDistances() const
{
  std::array<float, 8> distances;
  distances.fill(200.f);  // 默认最大检测距离
  
  if (!m_tank || !m_maze) return distances;
  
  sf::Vector2f pos = m_tank->getPosition();
  
  // 8个方向：0°, 45°, 90°, 135°, 180°, 225°, 270°, 315°
  for (int i = 0; i < 8; ++i) {
    float angle = i * 45.f * M_PI / 180.f;
    sf::Vector2f dir(std::cos(angle), std::sin(angle));
    
    // 射线检测
    for (float dist = 0.f; dist <= 200.f; dist += 5.f) {
      sf::Vector2f checkPos = pos + dir * dist;
      
      if (m_maze->checkCollision(checkPos, 5.f)) {
        distances[i] = dist;
        break;
      }
    }
  }
  
  return distances;
}

// ============================================================================
// RewardCalculator Implementation - AI目标：击败玩家，阻止玩家到达终点，自己永不触碰终点
// ============================================================================

bool RewardCalculator::isBlockingPlayerToExit(const AIObservation& obs)
{
  if (!obs.enemyVisible) return false;
  
  // 检查AI是否在玩家和终点之间
  sf::Vector2f playerToExit = obs.exitPosition - obs.enemyPosition;
  sf::Vector2f playerToAI = obs.position - obs.enemyPosition;
  
  float playerExitDist = std::hypot(playerToExit.x, playerToExit.y);
  float playerAIDist = std::hypot(playerToAI.x, playerToAI.y);
  
  // AI在玩家和终点之间，且比玩家更靠近终点的路径
  if (playerAIDist < playerExitDist * 0.8f) {
    // 计算AI是否在玩家-终点的路径上
    float dot = (playerToAI.x * playerToExit.x + playerToAI.y * playerToExit.y) / (playerExitDist * playerExitDist);
    if (dot > 0.2f && dot < 0.9f) {
      // AI大致在玩家到终点的路线上
      return true;
    }
  }
  return false;
}

float RewardCalculator::getExitBlockingScore(const AIObservation& obs)
{
  if (!obs.enemyVisible) return 0.f;
  
  // 玩家到终点的距离
  sf::Vector2f playerToExit = obs.exitPosition - obs.enemyPosition;
  float playerExitDist = std::hypot(playerToExit.x, playerToExit.y);
  
  // AI到玩家-终点连线的距离（越小越好）
  sf::Vector2f playerToAI = obs.position - obs.enemyPosition;
  float playerAIDist = std::hypot(playerToAI.x, playerToAI.y);
  
  // AI到终点的距离
  float aiExitDist = obs.exitDistance;
  
  // 理想拦截位置：在玩家和终点之间
  float t = std::clamp(playerAIDist / playerExitDist, 0.f, 1.f);
  sf::Vector2f idealPos = obs.enemyPosition + playerToExit * t;
  float distToIdeal = std::hypot(obs.position.x - idealPos.x, obs.position.y - idealPos.y);
  
  // 返回0-1的分数，越高越好
  float score = std::max(0.f, 1.f - distToIdeal / 300.f);
  
  // 如果玩家非常接近终点，增加紧迫性
  if (playerExitDist < 200.f) {
    score *= 2.f;
  }
  
  return score;
}

float RewardCalculator::calculateReward(
  const AIObservation& obs,
  const AIObservation& nextObs,
  const AIAction& action,
  bool aiWon,
  bool aiLost,
  bool playerReachedExit,
  bool aiTouchedExit)
{
  float reward = 0.f;
  
  // ==========================================================
  // 1. 最重要：AI绝对不能触碰终点（严重惩罚）
  // ==========================================================
  if (aiTouchedExit) {
    return AI_TOUCH_EXIT_PENALTY;  // 立即返回最严重惩罚
  }
  
  // AI靠近终点也要惩罚
  if (nextObs.exitDistance < SAFE_EXIT_DISTANCE) {
    float proximityPenalty = EXIT_PROXIMITY_PENALTY * (1.f - nextObs.exitDistance / SAFE_EXIT_DISTANCE);
    reward += proximityPenalty;
  }
  
  // ==========================================================
  // 2. 核心目标：击败玩家
  // ==========================================================
  if (aiWon) {
    reward += KILL_PLAYER_REWARD;
    return reward;  // 获胜直接返回
  }
  
  if (aiLost) {
    reward += AI_DEATH_PENALTY;
  }
  
  // ==========================================================
  // 3. 阻止玩家到达终点
  // ==========================================================
  if (playerReachedExit) {
    reward += PLAYER_EXIT_PENALTY;  // AI重大失败
    return reward;
  }
  
  // 检查玩家是否在接近终点
  if (obs.enemyVisible && nextObs.enemyVisible) {
    sf::Vector2f oldPlayerToExit = obs.exitPosition - obs.enemyPosition;
    sf::Vector2f newPlayerToExit = nextObs.exitPosition - nextObs.enemyPosition;
    float oldPlayerExitDist = std::hypot(oldPlayerToExit.x, oldPlayerToExit.y);
    float newPlayerExitDist = std::hypot(newPlayerToExit.x, newPlayerToExit.y);
    
    // 如果AI成功阻止玩家靠近终点，奖励
    if (newPlayerExitDist >= oldPlayerExitDist) {
      reward += BLOCK_PLAYER_EXIT_REWARD;
    }
    
    // 如果玩家非常接近终点，AI需要更积极地拦截
    if (newPlayerExitDist < 150.f) {
      reward += BLOCK_PLAYER_EXIT_REWARD * 2.f;  // 额外奖励拦截
    }
  }
  
  // 拦截位置奖励
  float blockingScore = getExitBlockingScore(nextObs);
  reward += INTERCEPT_POSITION_REWARD * blockingScore;
  
  // ==========================================================
  // 4. 战斗奖励
  // ==========================================================
  
  // 造成伤害
  if (obs.enemyVisible && nextObs.enemyVisible) {
    float enemyHealthDelta = nextObs.enemyHealth - obs.enemyHealth;
    if (enemyHealthDelta < 0.f) {
      reward += -enemyHealthDelta * DAMAGE_DEALT_REWARD / 25.f;
    }
  }
  
  // 受到伤害
  float healthDelta = nextObs.health - obs.health;
  if (healthDelta < 0.f) {
    reward += healthDelta * DAMAGE_TAKEN_PENALTY / 25.f;
  }
  
  // 追击玩家（在安全距离外）
  if (obs.enemyVisible && nextObs.enemyVisible && nextObs.exitDistance > SAFE_EXIT_DISTANCE) {
    float oldEnemyDist = obs.enemyDistance;
    float newEnemyDist = nextObs.enemyDistance;
    if (newEnemyDist < oldEnemyDist) {
      reward += AGGRESSIVE_PURSUIT_REWARD * (oldEnemyDist - newEnemyDist) / 100.f;
    }
  }
  
  // ==========================================================
  // 5. 其他奖励
  // ==========================================================
  
  // 激活NPC
  int oldFriendlyNPCs = 0;
  int newFriendlyNPCs = 0;
  for (int team : obs.visibleNPCTeams) {
    if (team == 2) oldFriendlyNPCs++;  // AI的队伍
  }
  for (int team : nextObs.visibleNPCTeams) {
    if (team == 2) newFriendlyNPCs++;
  }
  if (newFriendlyNPCs > oldFriendlyNPCs) {
    reward += NPC_ACTIVATED_REWARD;
  }
  
  // 撞墙惩罚
  float minWallDist = *std::min_element(nextObs.wallDistances.begin(), nextObs.wallDistances.end());
  if (minWallDist < 30.f) {
    reward += HIT_WALL_PENALTY;
  }
  
  // 生存奖励
  if (!aiLost) {
    reward += SURVIVAL_REWARD;
  }
  
  return reward;
}
