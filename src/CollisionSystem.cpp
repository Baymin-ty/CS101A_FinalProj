#include "include/CollisionSystem.hpp"
#include <cmath>
#include <algorithm>

bool CollisionSystem::checkBulletWallCollision(Bullet* bullet, Maze& maze)
{
  return maze.bulletHit(bullet->getPosition(), bullet->getDamage());
}

WallDestroyResult CollisionSystem::checkBulletWallCollisionWithResult(Bullet* bullet, Maze& maze)
{
  return maze.bulletHitWithResult(bullet->getPosition(), bullet->getDamage());
}

void CollisionSystem::handleWallDestroyEffect(const WallDestroyResult& result, Tank* shooter, Maze& maze)
{
  if (!result.destroyed || !shooter)
    return;
    
  switch (result.attribute)
  {
    case WallAttribute::Gold:
      // 金色墙：获得2金币
      shooter->addCoins(2);
      break;
      
    case WallAttribute::Heal:
      // 治疗墙：恢复25%血量
      shooter->heal(0.25f);
      break;
      
    case WallAttribute::Explosive:
      // 爆炸墙：爆炸效果已在Maze::bulletHitWithResult中处理（清除周围8格墙）
      // 这里可以添加额外的伤害逻辑（杀死范围内的坦克/NPC）
      // 暂时只处理墙体清除，伤害逻辑可以后续添加
      break;
      
    case WallAttribute::None:
    default:
      break;
  }
}

bool CollisionSystem::checkBulletTankCollision(Bullet* bullet, Tank* tank, float extraRadius)
{
  sf::Vector2f bulletPos = bullet->getPosition();
  sf::Vector2f tankPos = tank->getPosition();
  float dist = std::hypot(bulletPos.x - tankPos.x, bulletPos.y - tankPos.y);
  return dist < tank->getCollisionRadius() + extraRadius;
}

bool CollisionSystem::checkBulletNpcCollision(Bullet* bullet, Enemy* npc, float extraRadius)
{
  sf::Vector2f bulletPos = bullet->getPosition();
  sf::Vector2f npcPos = npc->getPosition();
  float dist = std::hypot(bulletPos.x - npcPos.x, bulletPos.y - npcPos.y);
  return dist < npc->getCollisionRadius() + extraRadius;
}

void CollisionSystem::checkSinglePlayerCollisions(
  Tank* player,
  std::vector<std::unique_ptr<Enemy>>& enemies,
  std::vector<std::unique_ptr<Bullet>>& bullets,
  Maze& maze)
{
  if (!player)
    return;

  // 检查子弹与墙壁、玩家、敌人的碰撞
  for (auto& bullet : bullets)
  {
    if (!bullet->isAlive())
      continue;

    // 检查与墙壁碰撞
    if (checkBulletWallCollision(bullet.get(), maze))
    {
      bullet->setInactive();
      continue;
    }

    // 检查与玩家的碰撞（敌人子弹）
    if (bullet->getOwner() == BulletOwner::Enemy)
    {
      if (checkBulletTankCollision(bullet.get(), player))
      {
        player->takeDamage(bullet->getDamage());
        bullet->setInactive();
        continue;
      }
    }

    // 检查与敌人的碰撞（玩家子弹）
    if (bullet->getOwner() == BulletOwner::Player)
    {
      for (auto& enemy : enemies)
      {
        if (checkBulletNpcCollision(bullet.get(), enemy.get()))
        {
          enemy->takeDamage(bullet->getDamage());
          bullet->setInactive();
          break;
        }
      }
    }
  }

  // 删除无效子弹
  bullets.erase(
    std::remove_if(bullets.begin(), bullets.end(),
      [](const std::unique_ptr<Bullet>& b) { return !b->isAlive(); }),
    bullets.end());
}

void CollisionSystem::checkMultiplayerCollisions(
  Tank* player,
  Tank* otherPlayer,
  std::vector<std::unique_ptr<Enemy>>& enemies,
  std::vector<std::unique_ptr<Bullet>>& bullets,
  Maze& maze,
  bool isHost)
{
  if (!player || !otherPlayer)
    return;

  int localTeam = player->getTeam();

  for (auto& bullet : bullets)
  {
    if (!bullet->isAlive())
      continue;

    sf::Vector2f bulletPos = bullet->getPosition();
    int bulletTeam = bullet->getTeam();

    // 检查与墙壁碰撞（使用带属性返回的版本）
    WallDestroyResult wallResult = checkBulletWallCollisionWithResult(bullet.get(), maze);
    // 判断是否击中了墙（包括击中但未摧毁的情况）
    bool hitWall = (wallResult.position.x != 0 || wallResult.position.y != 0);
    
    if (hitWall || wallResult.destroyed)
    {
      // 如果墙被摧毁且有属性效果，处理增益
      if (wallResult.destroyed)
      {
        // 判断子弹是谁发射的，给对应玩家加效果
        bool isLocalPlayerBullet = bullet->getOwner() == BulletOwner::Player;
        if (isLocalPlayerBullet)
        {
          handleWallDestroyEffect(wallResult, player, maze);
        }
        // 注意：对方玩家的增益效果由对方客户端处理
      }
      
      bullet->setInactive();
      continue;
    }

    // 判断子弹是否是本地玩家发射的
    bool isLocalPlayerBullet = bullet->getOwner() == BulletOwner::Player;

    // 检查与本地玩家的碰撞
    bool canHitLocalPlayer = !isLocalPlayerBullet &&
                             (bulletTeam == 0 || bulletTeam != localTeam);
    if (canHitLocalPlayer)
    {
      if (checkBulletTankCollision(bullet.get(), player))
      {
        player->takeDamage(bullet->getDamage());
        bullet->setInactive();
        continue;
      }
    }

    // 检查与对方玩家的碰撞
    bool canHitOtherPlayer = isLocalPlayerBullet || bulletTeam == localTeam;
    if (canHitOtherPlayer)
    {
      if (checkBulletTankCollision(bullet.get(), otherPlayer))
      {
        bullet->setInactive();
        continue;
      }
    }

    // 检查与NPC的碰撞
    for (auto& npc : enemies)
    {
      if (!npc->isActivated() || npc->isDead())
        continue;

      int npcTeam = npc->getTeam();

      // 判断子弹是否能击中这个NPC
      bool canHitNpc = false;

      if (isLocalPlayerBullet)
      {
        canHitNpc = (npcTeam != localTeam && npcTeam != 0);
      }
      else if (bulletTeam == 0)
      {
        canHitNpc = (npcTeam == localTeam && npcTeam != 0);
      }
      else
      {
        canHitNpc = (bulletTeam != npcTeam && npcTeam != 0);
      }

      if (canHitNpc)
      {
        if (checkBulletNpcCollision(bullet.get(), npc.get()))
        {
          // 本地玩家子弹：本地处理伤害并同步
          if (isLocalPlayerBullet)
          {
            npc->takeDamage(bullet->getDamage());
            NetworkManager::getInstance().sendNpcDamage(npc->getId(), bullet->getDamage());
          }
          // NPC子弹伤害NPC：只有房主处理
          else if (bulletTeam != 0 && isHost)
          {
            npc->takeDamage(bullet->getDamage());
            NetworkManager::getInstance().sendNpcDamage(npc->getId(), bullet->getDamage());
          }

          bullet->setInactive();
          break;
        }
      }
    }
  }

  // 删除无效子弹
  bullets.erase(
    std::remove_if(bullets.begin(), bullets.end(),
      [](const std::unique_ptr<Bullet>& b) { return !b->isAlive(); }),
    bullets.end());
}
