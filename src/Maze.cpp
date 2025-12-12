#include "Maze.hpp"
#include <cmath>
#include <cstdint>
#include <algorithm>

Maze::Maze()
{
}

void Maze::loadFromString(const std::vector<std::string> &map)
{
  if (map.empty())
    return;

  // 保存原始迷宫数据用于网络传输
  m_mazeData = map;

  m_rows = static_cast<int>(map.size());
  m_cols = 0;
  for (const auto &row : map)
  {
    m_cols = std::max(m_cols, static_cast<int>(row.size()));
  }

  m_walls.clear();
  m_walls.resize(m_rows, std::vector<Wall>(m_cols));
  m_enemySpawnPoints.clear();
  m_spawn1Position = {0.f, 0.f};
  m_spawn2Position = {0.f, 0.f};

  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < static_cast<int>(map[r].size()); ++c)
    {
      char ch = map[r][c];
      Wall &wall = m_walls[r][c];

      float x = c * m_tileSize;
      float y = r * m_tileSize;

      wall.shape.setSize({m_tileSize - 2.f, m_tileSize - 2.f});
      wall.shape.setPosition({x + 1.f, y + 1.f});

      switch (ch)
      {
      case '#': // 不可破坏墙
        wall.type = WallType::Solid;
        wall.shape.setFillColor(m_solidColor);
        wall.shape.setOutlineColor(sf::Color(60, 60, 60));
        wall.shape.setOutlineThickness(1.f);
        break;

      case '*': // 可破坏墙
        wall.type = WallType::Destructible;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_destructibleColor);
        wall.shape.setOutlineColor(sf::Color(100, 60, 20));
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'S': // 起点
        wall.type = WallType::None;
        m_startPosition = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case 'E': // 出口
        wall.type = WallType::Exit;
        wall.shape.setFillColor(m_exitColor);
        m_exitPosition = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case 'X': // 敌人位置
        wall.type = WallType::None;
        m_enemySpawnPoints.push_back({x + m_tileSize / 2.f, y + m_tileSize / 2.f});
        break;

      case '1': // 多人模式出生点1
        wall.type = WallType::None;
        m_spawn1Position = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      case '2': // 多人模式出生点2
        wall.type = WallType::None;
        m_spawn2Position = {x + m_tileSize / 2.f, y + m_tileSize / 2.f};
        break;

      default: // 空地
        wall.type = WallType::None;
        break;
      }
    }
  }
}

void Maze::generateRandomMaze(int width, int height, unsigned int seed, int enemyCount)
{
  MazeGenerator generator(width, height);
  if (seed != 0) {
    generator.setSeed(seed);
  }
  // 设置NPC数量
  generator.setEnemyCount(enemyCount);
  std::vector<std::string> mazeData = generator.generate();
  loadFromString(mazeData);
}

void Maze::update(float dt)
{
  (void)dt;
  // 更新可破坏墙的颜色（根据血量）
  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      Wall &wall = m_walls[r][c];
      if (wall.type == WallType::Destructible)
      {
        float healthRatio = wall.health / wall.maxHealth;
        // 插值颜色
        sf::Color color;
        color.r = static_cast<std::uint8_t>(m_destructibleDamagedColor.r +
                                            (m_destructibleColor.r - m_destructibleDamagedColor.r) * healthRatio);
        color.g = static_cast<std::uint8_t>(m_destructibleDamagedColor.g +
                                            (m_destructibleColor.g - m_destructibleDamagedColor.g) * healthRatio);
        color.b = static_cast<std::uint8_t>(m_destructibleDamagedColor.b +
                                            (m_destructibleColor.b - m_destructibleDamagedColor.b) * healthRatio);
        wall.shape.setFillColor(color);
      }
    }
  }
}

void Maze::draw(sf::RenderWindow &window) const
{
  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      const Wall &wall = m_walls[r][c];
      if (wall.type != WallType::None)
      {
        window.draw(wall.shape);
      }
    }
  }
}

bool Maze::checkCollision(sf::Vector2f position, float radius) const
{
  // 检查位置周围的墙体
  int minC = std::max(0, static_cast<int>((position.x - radius) / m_tileSize));
  int maxC = std::min(m_cols - 1, static_cast<int>((position.x + radius) / m_tileSize));
  int minR = std::max(0, static_cast<int>((position.y - radius) / m_tileSize));
  int maxR = std::min(m_rows - 1, static_cast<int>((position.y + radius) / m_tileSize));

  for (int r = minR; r <= maxR; ++r)
  {
    for (int c = minC; c <= maxC; ++c)
    {
      const Wall &wall = m_walls[r][c];
      if (wall.type == WallType::Solid || wall.type == WallType::Destructible)
      {
        // 矩形与圆形碰撞检测
        float wallLeft = c * m_tileSize;
        float wallRight = wallLeft + m_tileSize;
        float wallTop = r * m_tileSize;
        float wallBottom = wallTop + m_tileSize;

        // 找到最近点
        float closestX = std::max(wallLeft, std::min(position.x, wallRight));
        float closestY = std::max(wallTop, std::min(position.y, wallBottom));

        float dx = position.x - closestX;
        float dy = position.y - closestY;

        if (dx * dx + dy * dy < radius * radius)
        {
          return true;
        }
      }
    }
  }
  return false;
}

bool Maze::bulletHit(sf::Vector2f bulletPos, float damage)
{
  int c = static_cast<int>(bulletPos.x / m_tileSize);
  int r = static_cast<int>(bulletPos.y / m_tileSize);

  if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
    return false;

  Wall &wall = m_walls[r][c];

  if (wall.type == WallType::Solid)
  {
    return true; // 击中不可破坏墙
  }
  else if (wall.type == WallType::Destructible)
  {
    wall.health -= damage;
    if (wall.health <= 0)
    {
      wall.type = WallType::None; // 墙被摧毁
    }
    return true;
  }

  return false;
}

bool Maze::isAtExit(sf::Vector2f position, float radius) const
{
  float dx = position.x - m_exitPosition.x;
  float dy = position.y - m_exitPosition.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  return dist < radius + m_tileSize / 2.f;
}

bool Maze::isWalkable(int row, int col) const
{
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return false;
  WallType type = m_walls[row][col].type;
  return type == WallType::None || type == WallType::Exit;
}

GridPos Maze::worldToGrid(sf::Vector2f pos) const
{
  return {static_cast<int>(pos.x / m_tileSize), static_cast<int>(pos.y / m_tileSize)};
}

sf::Vector2f Maze::gridToWorld(GridPos grid) const
{
  return {grid.x * m_tileSize + m_tileSize / 2.f, grid.y * m_tileSize + m_tileSize / 2.f};
}

std::vector<sf::Vector2f> Maze::findPath(sf::Vector2f start, sf::Vector2f target) const
{
  GridPos startGrid = worldToGrid(start);
  GridPos targetGrid = worldToGrid(target);

  // 如果起点或终点不可通行，返回空路径
  if (!isWalkable(startGrid.y, startGrid.x) || !isWalkable(targetGrid.y, targetGrid.x))
  {
    return {};
  }

  // A* 算法
  auto heuristic = [](GridPos a, GridPos b) -> float
  {
    return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
  };

  struct Node
  {
    GridPos pos;
    float gCost; // 从起点到当前节点的代价
    float fCost; // gCost + heuristic

    bool operator>(const Node &other) const { return fCost > other.fCost; }
  };

  std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
  std::unordered_map<GridPos, GridPos, GridPosHash> cameFrom;
  std::unordered_map<GridPos, float, GridPosHash> gScore;

  openSet.push({startGrid, 0.f, heuristic(startGrid, targetGrid)});
  gScore[startGrid] = 0.f;

  // 四个方向
  const int dx[] = {0, 1, 0, -1};
  const int dy[] = {-1, 0, 1, 0};

  while (!openSet.empty())
  {
    Node current = openSet.top();
    openSet.pop();

    if (current.pos == targetGrid)
    {
      // 重建路径
      std::vector<sf::Vector2f> path;
      GridPos curr = targetGrid;
      while (curr != startGrid)
      {
        path.push_back(gridToWorld(curr));
        curr = cameFrom[curr];
      }
      std::reverse(path.begin(), path.end());
      return path;
    }

    // 检查是否已经有更好的路径到达这个节点
    if (gScore.count(current.pos) && current.gCost > gScore[current.pos])
    {
      continue;
    }

    for (int i = 0; i < 4; ++i)
    {
      GridPos neighbor = {current.pos.x + dx[i], current.pos.y + dy[i]};

      if (!isWalkable(neighbor.y, neighbor.x))
        continue;

      float tentativeG = current.gCost + 1.f;

      if (!gScore.count(neighbor) || tentativeG < gScore[neighbor])
      {
        cameFrom[neighbor] = current.pos;
        gScore[neighbor] = tentativeG;
        float fCost = tentativeG + heuristic(neighbor, targetGrid);
        openSet.push({neighbor, tentativeG, fCost});
      }
    }
  }

  // 没有找到路径
  return {};
}
