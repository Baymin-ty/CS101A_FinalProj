#include "MazeGenerator.hpp"
#include <algorithm>
#include <ctime>
#include <set>

MazeGenerator::MazeGenerator(int width, int height)
    : m_width(width), m_height(height), m_seed(0), m_seedSet(false)
{
  // 确保宽高是奇数（迷宫生成算法需要）
  if (m_width % 2 == 0)
    m_width++;
  if (m_height % 2 == 0)
    m_height++;
}

void MazeGenerator::setSeed(unsigned int seed)
{
  m_seed = seed;
  m_seedSet = true;
}

std::vector<std::string> MazeGenerator::generate()
{
  // 在生成开始时设置随机数种子，确保相同种子产生相同结果
  if (m_seedSet) {
    m_rng.seed(m_seed);
  } else {
    m_rng.seed(static_cast<unsigned int>(std::time(nullptr)));
  }
  
  // 初始化网格，全部填充墙
  m_grid.clear();
  m_grid.resize(m_height, std::vector<char>(m_width, '#'));

  // 使用递归回溯法生成迷宫
  // 从 (1,1) 开始挖通道
  carvePassage(1, 1);

  // 随机放置起点和终点
  placeStartAndEnd();

  // 确保有路径
  ensurePath(m_startX, m_startY, m_endX, m_endY);

  // 放置敌人
  placeEnemies();

  // 放置可破坏墙
  placeDestructibleWalls();

  // 放置多人模式出生点（在终点附近对称的两个位置）
  placeMultiplayerSpawns();

  // 转换为字符串向量
  std::vector<std::string> result;
  result.reserve(m_height);
  for (int y = 0; y < m_height; ++y)
  {
    result.push_back(std::string(m_grid[y].begin(), m_grid[y].end()));
  }

  return result;
}

void MazeGenerator::carvePassage(int cx, int cy)
{
  // 四个方向：上、右、下、左
  int dx[] = {0, 2, 0, -2};
  int dy[] = {-2, 0, 2, 0};

  // 随机打乱方向
  std::vector<int> dirs = {0, 1, 2, 3};
  std::shuffle(dirs.begin(), dirs.end(), m_rng);

  m_grid[cy][cx] = '.';

  for (int dir : dirs)
  {
    int nx = cx + dx[dir];
    int ny = cy + dy[dir];

    // 检查边界
    if (nx > 0 && nx < m_width - 1 && ny > 0 && ny < m_height - 1)
    {
      if (m_grid[ny][nx] == '#')
      {
        // 打通中间的墙
        m_grid[cy + dy[dir] / 2][cx + dx[dir] / 2] = '.';
        carvePassage(nx, ny);
      }
    }
  }
}

std::vector<std::pair<int, int>> MazeGenerator::getEmptySpaces()
{
  std::vector<std::pair<int, int>> spaces;
  for (int y = 1; y < m_height - 1; ++y)
  {
    for (int x = 1; x < m_width - 1; ++x)
    {
      if (m_grid[y][x] == '.')
      {
        spaces.push_back({x, y});
      }
    }
  }
  return spaces;
}

void MazeGenerator::placeStartAndEnd()
{
  auto emptySpaces = getEmptySpaces();
  if (emptySpaces.size() < 2)
  {
    // 回退到默认位置
    m_startX = 1;
    m_startY = 1;
    m_endX = m_width - 2;
    m_endY = m_height - 2;
    m_grid[m_startY][m_startX] = 'S';
    m_grid[m_endY][m_endX] = 'E';
    return;
  }

  std::shuffle(emptySpaces.begin(), emptySpaces.end(), m_rng);

  // 随机选一个起点
  auto [sx, sy] = emptySpaces[0];

  // 计算所有点与起点的距离，并排序
  std::vector<std::pair<int, std::pair<int, int>>> distancePoints;
  for (size_t i = 1; i < emptySpaces.size(); ++i)
  {
    auto [ex, ey] = emptySpaces[i];
    int dist = std::abs(ex - sx) + std::abs(ey - sy);
    distancePoints.push_back({dist, {ex, ey}});
  }

  // 按距离排序（从远到近）
  std::sort(distancePoints.begin(), distancePoints.end(),
            [](const auto &a, const auto &b)
            { return a.first > b.first; });

  // 从距离最远的前 10% 中随机选一个作为终点
  // 确保起点离终点足够远
  int topCount = std::max(1, static_cast<int>(distancePoints.size() * 0.1));
  int selectedIdx = m_rng() % topCount;

  m_startX = sx;
  m_startY = sy;
  m_endX = distancePoints[selectedIdx].second.first;
  m_endY = distancePoints[selectedIdx].second.second;

  m_grid[m_startY][m_startX] = 'S';
  m_grid[m_endY][m_endX] = 'E';
}

void MazeGenerator::ensurePath(int startX, int startY, int endX, int endY)
{
  // 使用 BFS 确保起点到终点有路径
  // 如果没有路径，打通一些墙

  std::vector<std::vector<bool>> visited(m_height, std::vector<bool>(m_width, false));
  std::vector<std::pair<int, int>> queue;
  queue.push_back({startX, startY});
  visited[startY][startX] = true;

  int dx[] = {0, 1, 0, -1};
  int dy[] = {-1, 0, 1, 0};

  while (!queue.empty())
  {
    auto [x, y] = queue.front();
    queue.erase(queue.begin());

    if (x == endX && y == endY)
    {
      return; // 找到路径
    }

    for (int i = 0; i < 4; ++i)
    {
      int nx = x + dx[i];
      int ny = y + dy[i];

      if (nx > 0 && nx < m_width - 1 && ny > 0 && ny < m_height - 1 && !visited[ny][nx])
      {
        if (m_grid[ny][nx] != '#')
        {
          visited[ny][nx] = true;
          queue.push_back({nx, ny});
        }
      }
    }
  }

  // 如果没有找到路径，创建一条随机弯曲的路径
  int x = startX, y = startY;
  while (x != endX || y != endY)
  {
    // 随机决定先走X还是Y
    bool moveX = (m_rng() % 2 == 0);

    if (moveX && x != endX)
    {
      x += (endX > x) ? 1 : -1;
    }
    else if (y != endY)
    {
      y += (endY > y) ? 1 : -1;
    }
    else if (x != endX)
    {
      x += (endX > x) ? 1 : -1;
    }

    if (m_grid[y][x] == '#')
      m_grid[y][x] = '.';
  }
}

void MazeGenerator::placeEnemies()
{
  std::vector<std::pair<int, int>> emptySpaces;

  // 收集所有空地（排除起点和终点附近）
  int minDistFromStart = 5; // 距离起点至少5格
  for (int y = 1; y < m_height - 1; ++y)
  {
    for (int x = 1; x < m_width - 1; ++x)
    {
      if (m_grid[y][x] == '.')
      {
        // 计算与起点的距离
        int distFromStart = std::abs(x - m_startX) + std::abs(y - m_startY);
        int distFromEnd = std::abs(x - m_endX) + std::abs(y - m_endY);

        // 不要太靠近起点或终点
        if (distFromStart > minDistFromStart && distFromEnd > 3)
        {
          emptySpaces.push_back({x, y});
        }
      }
    }
  }

  // 随机放置敌人
  std::shuffle(emptySpaces.begin(), emptySpaces.end(), m_rng);
  int enemiesPlaced = 0;
  for (const auto &pos : emptySpaces)
  {
    if (enemiesPlaced >= m_enemyCount)
      break;
    m_grid[pos.second][pos.first] = 'X';
    enemiesPlaced++;
  }
}

void MazeGenerator::placeDestructibleWalls()
{
  // 首先收集所有可以变成可破坏墙的位置
  std::vector<std::pair<int, int>> destructibleCandidates;
  
  for (int y = 1; y < m_height - 1; ++y)
  {
    for (int x = 1; x < m_width - 1; ++x)
    {
      if (m_grid[y][x] == '#')
      {
        // 检查是否有相邻的通道
        bool hasAdjacentPath = false;
        int dx[] = {0, 1, 0, -1};
        int dy[] = {-1, 0, 1, 0};

        for (int i = 0; i < 4; ++i)
        {
          int nx = x + dx[i];
          int ny = y + dy[i];
          if (nx >= 0 && nx < m_width && ny >= 0 && ny < m_height)
          {
            if (m_grid[ny][nx] == '.' || m_grid[ny][nx] == 'S' || m_grid[ny][nx] == 'E')
            {
              hasAdjacentPath = true;
              break;
            }
          }
        }

        // 只有相邻有通道的墙才可能变成可破坏墙
        if (hasAdjacentPath)
        {
          float roll = static_cast<float>(m_rng() % 1000) / 1000.f;
          if (roll < m_destructibleRatio)
          {
            destructibleCandidates.push_back({x, y});
          }
        }
      }
    }
  }
  
  // 单机模式：所有可破坏墙都是普通的（无特殊属性）
  if (!m_multiplayerMode)
  {
    for (const auto& [x, y] : destructibleCandidates)
    {
      m_grid[y][x] = '*'; // 普通可破坏墙
    }
    return;
  }
  
  // 联机模式：生成特殊方块
  // 先放置爆炸墙（10%，避开边缘）
  std::vector<std::pair<int, int>> explosiveWalls;
  for (auto it = destructibleCandidates.begin(); it != destructibleCandidates.end();)
  {
    int x = it->first;
    int y = it->second;
    
    // 爆炸墙不能在边缘一圈（距离边界至少2格）
    bool nearEdge = (x <= 2 || x >= m_width - 3 || y <= 2 || y >= m_height - 3);
    
    if (!nearEdge)
    {
      float roll = static_cast<float>(m_rng() % 1000) / 1000.f;
      if (roll < 0.10f) // 10%概率变成爆炸墙
      {
        m_grid[y][x] = 'B'; // B = Bomb/Explosive
        explosiveWalls.push_back({x, y});
        it = destructibleCandidates.erase(it);
        continue;
      }
    }
    ++it;
  }
  
  // 标记爆炸墙周围8格不能有增益墙
  std::set<std::pair<int, int>> noBuffZone;
  for (const auto& [ex, ey] : explosiveWalls)
  {
    for (int dy = -1; dy <= 1; ++dy)
    {
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (dx == 0 && dy == 0) continue;
        noBuffZone.insert({ex + dx, ey + dy});
      }
    }
  }
  
  // 剩余墙体分配属性
  for (const auto& [x, y] : destructibleCandidates)
  {
    // 检查是否在禁止增益区域
    bool inNoBuffZone = noBuffZone.count({x, y}) > 0;
    
    if (inNoBuffZone)
    {
      m_grid[y][x] = '*'; // 普通可破坏墙
    }
    else
    {
      // 分配属性：50%金色，25%治疗，25%普通
      float roll = static_cast<float>(m_rng() % 1000) / 1000.f;
      if (roll < 0.50f)
      {
        m_grid[y][x] = 'G'; // Gold
      }
      else if (roll < 0.75f)
      {
        m_grid[y][x] = 'H'; // Heal
      }
      else
      {
        m_grid[y][x] = '*'; // 普通
      }
    }
  }
}

void MazeGenerator::placeMultiplayerSpawns()
{
  // 为多人模式找两个出生点和一个合适的终点：
  // 1. 两个出生点在地图中部区域，有一定距离
  // 2. 终点在地图边缘/四周区域
  // 3. 两个出生点到终点的距离要相近（公平）
  
  auto emptySpaces = getEmptySpaces();
  if (emptySpaces.size() < 3) {
    // 回退到起点附近
    m_spawn1X = m_startX;
    m_spawn1Y = m_startY;
    m_spawn2X = m_startX;
    m_spawn2Y = m_startY + 2;
    return;
  }

  // 定义地图中部区域（中间50%的区域）
  int centerMarginX = m_width / 4;
  int centerMarginY = m_height / 4;
  int centerMinX = centerMarginX;
  int centerMaxX = m_width - centerMarginX;
  int centerMinY = centerMarginY;
  int centerMaxY = m_height - centerMarginY;

  // 定义边缘区域（距边界25%以内）
  auto isEdgeArea = [&](int x, int y) {
    return x < centerMarginX || x >= m_width - centerMarginX ||
           y < centerMarginY || y >= m_height - centerMarginY;
  };

  // 筛选中部区域的空地作为出生点候选
  std::vector<std::pair<int, int>> spawnCandidates;
  for (const auto& [x, y] : emptySpaces) {
    if (m_grid[y][x] == '.' || m_grid[y][x] == 'S') {
      // 在中部区域
      if (x >= centerMinX && x < centerMaxX && y >= centerMinY && y < centerMaxY) {
        spawnCandidates.push_back({x, y});
      }
    }
  }
  
  // 如果中部候选太少，扩大范围
  if (spawnCandidates.size() < 10) {
    spawnCandidates.clear();
    int smallMarginX = m_width / 6;
    int smallMarginY = m_height / 6;
    for (const auto& [x, y] : emptySpaces) {
      if (m_grid[y][x] == '.' || m_grid[y][x] == 'S') {
        if (x >= smallMarginX && x < m_width - smallMarginX && 
            y >= smallMarginY && y < m_height - smallMarginY) {
          spawnCandidates.push_back({x, y});
        }
      }
    }
  }

  if (spawnCandidates.size() < 2) {
    spawnCandidates = emptySpaces; // 回退
  }

  // 随机打乱候选点
  std::shuffle(spawnCandidates.begin(), spawnCandidates.end(), m_rng);

  // 找两个有一定距离的出生点
  int minSpawnDist = std::max(6, std::min(m_width, m_height) / 4); // 最小距离（增大）
  int maxSpawnDist = std::max(15, std::min(m_width, m_height) / 2); // 最大距离
  
  // 收集符合条件的出生点对
  std::vector<std::pair<int, int>> validSpawnPairs; // 存储索引对
  for (size_t i = 0; i < spawnCandidates.size() && i < 30; ++i) {
    for (size_t j = i + 1; j < spawnCandidates.size() && j < 30; ++j) {
      auto [x1, y1] = spawnCandidates[i];
      auto [x2, y2] = spawnCandidates[j];
      int dist = std::abs(x1 - x2) + std::abs(y1 - y2);
      if (dist >= minSpawnDist && dist <= maxSpawnDist) {
        validSpawnPairs.push_back({static_cast<int>(i), static_cast<int>(j)});
      }
    }
  }

  // 从有效的出生点对中随机选择一对
  if (!validSpawnPairs.empty()) {
    int pairIdx = m_rng() % validSpawnPairs.size();
    auto [idx1, idx2] = validSpawnPairs[pairIdx];
    m_spawn1X = spawnCandidates[idx1].first;
    m_spawn1Y = spawnCandidates[idx1].second;
    m_spawn2X = spawnCandidates[idx2].first;
    m_spawn2Y = spawnCandidates[idx2].second;
  } else if (spawnCandidates.size() >= 2) {
    // 回退：直接选前两个
    m_spawn1X = spawnCandidates[0].first;
    m_spawn1Y = spawnCandidates[0].second;
    m_spawn2X = spawnCandidates[1].first;
    m_spawn2Y = spawnCandidates[1].second;
  } else {
    // 最终回退
    m_spawn1X = m_width / 2 - 2;
    m_spawn1Y = m_height / 2;
    m_spawn2X = m_width / 2 + 2;
    m_spawn2Y = m_height / 2;
  }

  // 筛选边缘区域的空地作为终点候选，并计算到两个出生点的距离
  std::vector<std::tuple<int, int, int, int>> endCandidates; // {x, y, minDist, distDiff}
  
  for (const auto& [x, y] : emptySpaces) {
    if (m_grid[y][x] == '.' || m_grid[y][x] == 'S') {
      // 优先选择边缘区域的点
      if (isEdgeArea(x, y)) {
        int distToSpawn1 = std::abs(x - m_spawn1X) + std::abs(y - m_spawn1Y);
        int distToSpawn2 = std::abs(x - m_spawn2X) + std::abs(y - m_spawn2Y);
        int minDist = std::min(distToSpawn1, distToSpawn2);
        int distDiff = std::abs(distToSpawn1 - distToSpawn2);
        
        // 确保两个出生点到终点的距离差不要太大（公平性）
        if (distDiff <= std::max(3, minDist / 3)) {
          endCandidates.push_back({x, y, minDist, distDiff});
        }
      }
    }
  }

  // 如果边缘区域候选太少，也加入一些非边缘但距离较远的点
  if (endCandidates.size() < 5) {
    for (const auto& [x, y] : emptySpaces) {
      if (m_grid[y][x] == '.' || m_grid[y][x] == 'S') {
        if (!isEdgeArea(x, y)) {
          int distToSpawn1 = std::abs(x - m_spawn1X) + std::abs(y - m_spawn1Y);
          int distToSpawn2 = std::abs(x - m_spawn2X) + std::abs(y - m_spawn2Y);
          int minDist = std::min(distToSpawn1, distToSpawn2);
          int distDiff = std::abs(distToSpawn1 - distToSpawn2);
          
          // 要求距离较远
          if (minDist > std::min(m_width, m_height) / 3 && distDiff <= std::max(3, minDist / 3)) {
            endCandidates.push_back({x, y, minDist, distDiff});
          }
        }
      }
    }
  }

  // 按距离排序（从远到近），优先选择距离远的
  std::sort(endCandidates.begin(), endCandidates.end(),
            [](const auto &a, const auto &b) { return std::get<2>(a) > std::get<2>(b); });

  // 从距离最远的前 30% 中随机选一个作为终点
  int bestEndX = m_endX, bestEndY = m_endY;
  if (!endCandidates.empty()) {
    int topCount = std::max(1, static_cast<int>(endCandidates.size() * 0.3));
    int selectedIdx = m_rng() % topCount;
    bestEndX = std::get<0>(endCandidates[selectedIdx]);
    bestEndY = std::get<1>(endCandidates[selectedIdx]);
  }

  // 更新终点位置
  if (bestEndX != m_endX || bestEndY != m_endY) {
    // 清除旧终点
    if (m_grid[m_endY][m_endX] == 'E') {
      m_grid[m_endY][m_endX] = '.';
    }
    m_endX = bestEndX;
    m_endY = bestEndY;
    m_grid[m_endY][m_endX] = 'E';
  }

  // 在地图上标记出生点，用 '1' 和 '2' 表示
  // 确保位置是空地才标记
  if (m_spawn1Y >= 0 && m_spawn1Y < m_height && m_spawn1X >= 0 && m_spawn1X < m_width) {
    if (m_grid[m_spawn1Y][m_spawn1X] == '.' || m_grid[m_spawn1Y][m_spawn1X] == 'S') {
      m_grid[m_spawn1Y][m_spawn1X] = '1';
    }
  }
  if (m_spawn2Y >= 0 && m_spawn2Y < m_height && m_spawn2X >= 0 && m_spawn2X < m_width) {
    if (m_grid[m_spawn2Y][m_spawn2X] == '.' || m_grid[m_spawn2Y][m_spawn2X] == 'S') {
      m_grid[m_spawn2Y][m_spawn2X] = '2';
    }
  }
}
