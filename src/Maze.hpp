#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <queue>
#include <unordered_map>

// 墙体类型
enum class WallType
{
  None,         // 空地
  Destructible, // 可破坏墙体
  Solid,        // 不可破坏墙体
  Exit          // 出口
};

// 单个墙体
struct Wall
{
  sf::RectangleShape shape;
  WallType type;
  float health;
  float maxHealth;

  Wall() : type(WallType::None), health(0), maxHealth(0) {}
};

// 网格坐标
struct GridPos
{
  int x, y;
  bool operator==(const GridPos &other) const { return x == other.x && y == other.y; }
  bool operator!=(const GridPos &other) const { return !(*this == other); }
};

// GridPos 哈希函数
struct GridPosHash
{
  std::size_t operator()(const GridPos &pos) const
  {
    return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 16);
  }
};

class Maze
{
public:
  Maze();

  // 从字符串地图加载迷宫
  // '#' = 不可破坏墙
  // '*' = 可破坏墙
  // '.' = 空地
  // 'S' = 起点
  // 'E' = 出口
  // 'X' = 敌人位置
  void loadFromString(const std::vector<std::string> &map);

  void update(float dt);
  void draw(sf::RenderWindow &window) const;

  // 碰撞检测
  bool checkCollision(sf::Vector2f position, float radius) const;

  // 子弹与墙壁碰撞，返回是否击中，同时处理可破坏墙
  bool bulletHit(sf::Vector2f bulletPos, float damage);

  // 获取起点位置
  sf::Vector2f getStartPosition() const { return m_startPosition; }

  // 获取出口位置
  sf::Vector2f getExitPosition() const { return m_exitPosition; }

  // 检查是否到达出口
  bool isAtExit(sf::Vector2f position, float radius) const;

  // 获取敌人生成点
  const std::vector<sf::Vector2f> &getEnemySpawnPoints() const { return m_enemySpawnPoints; }

  // 获取迷宫尺寸（像素）
  sf::Vector2f getSize() const { return sf::Vector2f(m_cols * m_tileSize, m_rows * m_tileSize); }

  // 获取单元格大小
  float getTileSize() const { return m_tileSize; }

  // A* 寻路：返回从 start 到 target 的路径（世界坐标点列表）
  std::vector<sf::Vector2f> findPath(sf::Vector2f start, sf::Vector2f target) const;

  // 检查某个网格是否可通行
  bool isWalkable(int row, int col) const;

  // 世界坐标转网格坐标
  GridPos worldToGrid(sf::Vector2f pos) const;

  // 网格坐标转世界坐标（返回格子中心）
  sf::Vector2f gridToWorld(GridPos grid) const;

private:
  std::vector<std::vector<Wall>> m_walls;
  sf::Vector2f m_startPosition;
  sf::Vector2f m_exitPosition;
  std::vector<sf::Vector2f> m_enemySpawnPoints;

  int m_rows = 0;
  int m_cols = 0;
  float m_tileSize = 50.f;

  // 颜色
  const sf::Color m_solidColor = sf::Color(80, 80, 80);
  const sf::Color m_destructibleColor = sf::Color(139, 90, 43);
  const sf::Color m_destructibleDamagedColor = sf::Color(100, 60, 30);
  const sf::Color m_exitColor = sf::Color(0, 200, 0, 180);
};
