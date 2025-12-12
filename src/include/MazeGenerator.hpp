#pragma once

#include <vector>
#include <string>
#include <random>

class MazeGenerator
{
public:
  MazeGenerator(int width, int height);

  // 生成随机迷宫
  std::vector<std::string> generate();

  // 设置随机种子
  void setSeed(unsigned int seed);

  // 设置敌人数量
  void setEnemyCount(int count) { m_enemyCount = count; }

  // 设置可破坏墙比例
  void setDestructibleRatio(float ratio) { m_destructibleRatio = ratio; }

  // 获取两个出生点（用于多人模式）
  std::pair<int, int> getSpawn1() const { return {m_spawn1X, m_spawn1Y}; }
  std::pair<int, int> getSpawn2() const { return {m_spawn2X, m_spawn2Y}; }

private:
  void carvePassage(int x, int y);
  void placeEnemies();
  void placeDestructibleWalls();
  void placeStartAndEnd();                                    // 随机放置起点和终点
  void placeMultiplayerSpawns();                              // 放置多人模式两个出生点
  void ensurePath(int startX, int startY, int endX, int endY); // 确保起点到终点有路径
  std::vector<std::pair<int, int>> getEmptySpaces();          // 获取所有空地

  int m_width;
  int m_height;
  std::vector<std::vector<char>> m_grid;
  std::mt19937 m_rng;
  unsigned int m_seed = 0;
  bool m_seedSet = false;

  int m_enemyCount = 5;
  float m_destructibleRatio = 0.15f;

  // 起点和终点坐标
  int m_startX = 1, m_startY = 1;
  int m_endX = 1, m_endY = 1;

  // 多人模式两个出生点
  int m_spawn1X = 1, m_spawn1Y = 1;
  int m_spawn2X = 1, m_spawn2Y = 1;
};
