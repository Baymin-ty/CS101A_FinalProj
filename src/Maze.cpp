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
      wall.shape.setCornerRadius(WALL_CORNER_RADIUS);
      wall.shape.setPosition({x + 1.f, y + 1.f});

      switch (ch)
      {
      case '#': // 不可破坏墙
        wall.type = WallType::Solid;
        wall.shape.setFillColor(m_solidColor);
        wall.shape.setOutlineColor(sf::Color(60, 60, 60));
        wall.shape.setOutlineThickness(1.f);
        break;

      case '*': // 可破坏墙（普通）
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::None;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_destructibleColor);
        wall.shape.setOutlineColor(sf::Color(100, 60, 20));
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'G': // 金色墙 - 打掉获得2金币
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::Gold;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_goldWallColor);
        wall.shape.setOutlineColor(sf::Color(220, 170, 30)); // 金色边框
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'H': // 治疗墙 - 恢复25%血量
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::Heal;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_healWallColor);
        wall.shape.setOutlineColor(sf::Color(50, 140, 220)); // 蓝色边框
        wall.shape.setOutlineThickness(1.f);
        break;

      case 'B': // 爆炸墙 - 爆炸杀伤周围
        wall.type = WallType::Destructible;
        wall.attribute = WallAttribute::Explosive;
        wall.health = 100.f;
        wall.maxHealth = 100.f;
        wall.shape.setFillColor(m_explosiveWallColor);
        wall.shape.setOutlineColor(sf::Color(200, 50, 50)); // 红色边框
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

  // 计算每个墙体的圆角
  calculateRoundedCorners();
}

void Maze::generateRandomMaze(int width, int height, unsigned int seed, int enemyCount, bool multiplayerMode)
{
  MazeGenerator generator(width, height);
  if (seed != 0)
  {
    generator.setSeed(seed);
  }
  // 设置NPC数量
  generator.setEnemyCount(enemyCount);
  // 设置联机模式（联机模式下生成特殊方块）
  generator.setMultiplayerMode(multiplayerMode);
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
        sf::Color color;

        // 根据墙体属性选择对应的颜色插值
        switch (wall.attribute)
        {
        case WallAttribute::Gold:
        {
          // 金色墙：从深金色到亮金色
          sf::Color dark(180, 140, 30);
          color.r = static_cast<std::uint8_t>(dark.r + (m_goldWallColor.r - dark.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(dark.g + (m_goldWallColor.g - dark.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(dark.b + (m_goldWallColor.b - dark.b) * healthRatio);
          break;
        }
        case WallAttribute::Heal:
        {
          // 蓝色墙：从深蓝色到亮蓝色
          sf::Color dark(40, 100, 180);
          color.r = static_cast<std::uint8_t>(dark.r + (m_healWallColor.r - dark.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(dark.g + (m_healWallColor.g - dark.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(dark.b + (m_healWallColor.b - dark.b) * healthRatio);
          break;
        }
        case WallAttribute::Explosive:
        {
          // 红色墙：从深红色到亮红色
          sf::Color dark(150, 40, 40);
          color.r = static_cast<std::uint8_t>(dark.r + (m_explosiveWallColor.r - dark.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(dark.g + (m_explosiveWallColor.g - dark.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(dark.b + (m_explosiveWallColor.b - dark.b) * healthRatio);
          break;
        }
        default: // WallAttribute::None - 普通可破坏墙（棕色）
          color.r = static_cast<std::uint8_t>(m_destructibleDamagedColor.r +
                                              (m_destructibleColor.r - m_destructibleDamagedColor.r) * healthRatio);
          color.g = static_cast<std::uint8_t>(m_destructibleDamagedColor.g +
                                              (m_destructibleColor.g - m_destructibleDamagedColor.g) * healthRatio);
          color.b = static_cast<std::uint8_t>(m_destructibleDamagedColor.b +
                                              (m_destructibleColor.b - m_destructibleDamagedColor.b) * healthRatio);
          break;
        }

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
        // 选择性圆角矩形与圆形碰撞检测
        float wallLeft = c * m_tileSize + 1.f; // 考虑1像素偏移
        float wallRight = wallLeft + m_tileSize - 2.f;
        float wallTop = r * m_tileSize + 1.f;
        float wallBottom = wallTop + m_tileSize - 2.f;

        float cornerRadius = WALL_CORNER_RADIUS;

        // 计算内部矩形边界（不包含圆角区域）
        float innerLeft = wallLeft + cornerRadius;
        float innerRight = wallRight - cornerRadius;
        float innerTop = wallTop + cornerRadius;
        float innerBottom = wallBottom - cornerRadius;

        // 判断位置在哪个角落区域
        bool inLeftZone = position.x < innerLeft;
        bool inRightZone = position.x > innerRight;
        bool inTopZone = position.y < innerTop;
        bool inBottomZone = position.y > innerBottom;

        // 确定是哪个角并检查该角是否有圆角
        int cornerIndex = -1; // 0=左上, 1=右上, 2=右下, 3=左下
        if (inLeftZone && inTopZone)
          cornerIndex = 0;
        else if (inRightZone && inTopZone)
          cornerIndex = 1;
        else if (inRightZone && inBottomZone)
          cornerIndex = 2;
        else if (inLeftZone && inBottomZone)
          cornerIndex = 3;

        if (cornerIndex >= 0 && wall.roundedCorners[cornerIndex])
        {
          // 这个角是圆角 - 使用圆形碰撞检测
          float cornerCenterX = inLeftZone ? innerLeft : innerRight;
          float cornerCenterY = inTopZone ? innerTop : innerBottom;

          float dx = position.x - cornerCenterX;
          float dy = position.y - cornerCenterY;
          float distSq = dx * dx + dy * dy;
          float combinedRadius = radius + cornerRadius;

          if (distSq < combinedRadius * combinedRadius)
          {
            return true;
          }
        }
        else
        {
          // 直角或边缘区域 - 普通矩形碰撞检测
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
      // 保存属性以便处理爆炸
      WallAttribute attr = wall.attribute;
      wall.type = WallType::None; // 墙被摧毁

      // 如果是爆炸墙，处理周围8格
      if (attr == WallAttribute::Explosive)
      {
        handleExplosion(c, r);
      }
    }
    return true;
  }

  return false;
}

WallDestroyResult Maze::bulletHitWithResult(sf::Vector2f bulletPos, float damage)
{
  WallDestroyResult result;

  int c = static_cast<int>(bulletPos.x / m_tileSize);
  int r = static_cast<int>(bulletPos.y / m_tileSize);

  if (r < 0 || r >= m_rows || c < 0 || c >= m_cols)
    return result;

  Wall &wall = m_walls[r][c];

  // 如果是不可破坏墙，视为命中但无摧毁效果
  if (wall.type == WallType::Solid)
  {
    result.destroyed = false;
    result.attribute = WallAttribute::None;
    result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
    result.gridX = c;
    result.gridY = r;
    return result;
  }

  if (wall.type == WallType::Destructible)
  {
    wall.health -= damage;

    if (wall.health <= 0)
    {
      // 记录摧毁信息
      result.destroyed = true;
      result.attribute = wall.attribute;
      result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
      result.gridX = c;
      result.gridY = r;

      // 清除当前墙格
      wall.type = WallType::None;

      // 如果是爆炸属性，处理周围8格
      if (result.attribute == WallAttribute::Explosive)
      {
        handleExplosion(result.gridX, result.gridY);
      }
    }
    else
    {
      // 受伤但未摧毁
      result.destroyed = false;
      result.attribute = WallAttribute::None;
      result.position = {c * m_tileSize + m_tileSize / 2.f, r * m_tileSize + m_tileSize / 2.f};
      result.gridX = c;
      result.gridY = r;
    }

    return result;
  }

  // 非墙体
  return result;
}

void Maze::handleExplosion(int gridX, int gridY)
{
  // 移除周围8格的可破坏墙（不影响不可破坏墙和出口）
  for (int dr = -1; dr <= 1; ++dr)
  {
    for (int dc = -1; dc <= 1; ++dc)
    {
      if (dr == 0 && dc == 0)
        continue;

      int nr = gridY + dr; // 行
      int nc = gridX + dc; // 列

      if (nr < 0 || nr >= m_rows || nc < 0 || nc >= m_cols)
        continue;

      Wall &neighbor = m_walls[nr][nc];
      if (neighbor.type == WallType::Destructible)
      {
        neighbor.type = WallType::None;
      }
    }
  }
}

std::vector<sf::Vector2f> Maze::getExplosionArea(int gridX, int gridY) const
{
  std::vector<sf::Vector2f> area;
  for (int dx = -1; dx <= 1; ++dx)
  {
    for (int dy = -1; dy <= 1; ++dy)
    {
      if (dx == 0 && dy == 0)
        continue;
      int gx = gridX + dx;
      int gy = gridY + dy;
      if (gy < 0 || gy >= m_rows || gx < 0 || gx >= m_cols)
        continue;
      area.push_back({gx * m_tileSize + m_tileSize / 2.f, gy * m_tileSize + m_tileSize / 2.f});
    }
  }
  return area;
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

int Maze::checkLineOfSight(sf::Vector2f start, sf::Vector2f end) const
{
  // 使用 Bresenham 线段算法检查视线
  // 返回值：0 = 无阻挡, 1 = 有可拆墙阻挡, 2 = 有不可拆墙阻挡

  GridPos startGrid = worldToGrid(start);
  GridPos endGrid = worldToGrid(end);

  int x0 = startGrid.x, y0 = startGrid.y;
  int x1 = endGrid.x, y1 = endGrid.y;

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;

  int result = 0; // 无阻挡

  while (true)
  {
    // 检查当前格子
    if (y0 >= 0 && y0 < m_rows && x0 >= 0 && x0 < m_cols)
    {
      WallType type = m_walls[y0][x0].type;
      if (type == WallType::Solid)
      {
        return 2; // 不可拆墙阻挡
      }
      else if (type == WallType::Destructible)
      {
        result = 1; // 可拆墙阻挡（继续检查是否有不可拆墙）
      }
    }

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }

  return result;
}

sf::Vector2f Maze::getFirstBlockedPosition(sf::Vector2f start, sf::Vector2f end) const
{
  // 找到视线上第一个被阻挡的位置
  GridPos startGrid = worldToGrid(start);
  GridPos endGrid = worldToGrid(end);

  int x0 = startGrid.x, y0 = startGrid.y;
  int x1 = endGrid.x, y1 = endGrid.y;

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;

  while (true)
  {
    // 检查当前格子
    if (y0 >= 0 && y0 < m_rows && x0 >= 0 && x0 < m_cols)
    {
      WallType type = m_walls[y0][x0].type;
      if (type == WallType::Solid || type == WallType::Destructible)
      {
        return gridToWorld({x0, y0});
      }
    }

    if (x0 == x1 && y0 == y1)
      break;

    int e2 = 2 * err;
    if (e2 > -dy)
    {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx)
    {
      err += dx;
      y0 += sy;
    }
  }

  return end; // 没有阻挡，返回目标位置
}

bool Maze::isWall(int row, int col) const
{
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols)
    return true; // 边界外视为墙

  WallType type = m_walls[row][col].type;
  return type == WallType::Solid || type == WallType::Destructible;
}

void Maze::calculateRoundedCorners()
{
  // 对于每个墙体，检查其四个角是否需要圆角
  // 规则：如果某个角的两个相邻方向都没有墙，则该角需要圆角
  // 例如：左上角需要圆角的条件是：左边没有墙 AND 上边没有墙

  for (int r = 0; r < m_rows; ++r)
  {
    for (int c = 0; c < m_cols; ++c)
    {
      Wall &wall = m_walls[r][c];

      // 只处理墙体
      if (wall.type != WallType::Solid && wall.type != WallType::Destructible && wall.type != WallType::Exit)
        continue;

      // 检查四个方向的邻居
      bool hasTop = isWall(r - 1, c);
      bool hasBottom = isWall(r + 1, c);
      bool hasLeft = isWall(r, c - 1);
      bool hasRight = isWall(r, c + 1);

      // 还需要检查对角线邻居（用于更精确的判断）
      bool hasTopLeft = isWall(r - 1, c - 1);
      bool hasTopRight = isWall(r - 1, c + 1);
      bool hasBottomLeft = isWall(r + 1, c - 1);
      bool hasBottomRight = isWall(r + 1, c + 1);

      // 计算每个角是否需要圆角
      // 左上角：如果左边和上边都没有墙，则需要圆角
      bool roundTopLeft = !hasTop && !hasLeft;
      // 右上角：如果右边和上边都没有墙，则需要圆角
      bool roundTopRight = !hasTop && !hasRight;
      // 右下角：如果右边和下边都没有墙，则需要圆角
      bool roundBottomRight = !hasBottom && !hasRight;
      // 左下角：如果左边和下边都没有墙，则需要圆角
      bool roundBottomLeft = !hasBottom && !hasLeft;

      // 设置圆角
      wall.roundedCorners = {roundTopLeft, roundTopRight, roundBottomRight, roundBottomLeft};
      wall.shape.setRoundedCorners(roundTopLeft, roundTopRight, roundBottomRight, roundBottomLeft);
    }
  }
}
