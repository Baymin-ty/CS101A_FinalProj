#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <array>
#include "Tank.hpp"
#include "Enemy.hpp"
#include "Bullet.hpp"
#include "Maze.hpp"

// AI 观察状态（部分可观测）
struct AIObservation
{
  // 自身状态
  sf::Vector2f position;
  float rotation;
  float turretRotation;
  float health;
  int coins;
  int myTeam;  // AI自己的阵营
  
  // 视野内的对手信息（如果看不到则距离为-1）
  sf::Vector2f enemyPosition;
  float enemyHealth;
  float enemyDistance;
  bool enemyVisible;
  
  // 所有敌对目标（玩家+敌方NPC）
  std::vector<sf::Vector2f> allEnemyTargets;
  std::vector<float> allEnemyDistances;
  sf::Vector2f bestTarget;           // 最佳攻击目标
  float bestTargetDistance;          // 到最佳目标的距离
  bool hasBestTarget;                // 是否有可攻击目标
  
  // 视野内的最近NPC信息
  std::vector<sf::Vector2f> visibleNPCPositions;
  std::vector<int> visibleNPCTeams;  // 0=中立, 1=己方, 2=敌方
  std::vector<float> visibleNPCHealths;
  
  // 可激活的中立NPC（在范围内）
  bool hasNearbyNeutralNPC;
  sf::Vector2f nearestNeutralNPCPos;
  float nearestNeutralNPCDist;
  
  // 视野内的子弹信息
  std::vector<sf::Vector2f> visibleBulletPositions;
  std::vector<sf::Vector2f> visibleBulletVelocities;
  std::vector<bool> bulletIsEnemy;   // 子弹是否是敌方的
  
  // 周围墙壁信息（8方向射线检测距离）
  std::array<float, 8> wallDistances;
  
  // 目标点（出口）方向和距离
  sf::Vector2f exitPosition;
  float exitDistance;
  float exitAngle;
  
  // 路径规划信息（由AIPlayer计算）
  std::vector<sf::Vector2f> pathToEnemy;        // 到敌人的路径
  sf::Vector2f nextWaypoint;                     // 下一个路径点
  bool hasPathToEnemy;                           // 是否有到敌人的路径
  bool hasDestructibleWallOnPath;                // 路径上有可破坏墙
  sf::Vector2f destructibleWallTarget;           // 可破坏墙位置
  int bulletPathToEnemy;                         // 子弹路径检测：0=无阻挡，1=可破坏墙，2=不可破坏墙
  sf::Vector2f shootTarget;                      // 最佳射击目标位置
  bool canShootTarget;                           // 是否可以直接射击目标
  
  // 时间信息
  float deltaTime;
};

// AI 动作
struct AIAction
{
  float moveX;           // -1 to 1
  float moveY;           // -1 to 1
  float turretAngle;     // 炮塔目标角度（度）
  bool shoot;            // 是否射击
  bool activateNPC;      // 是否激活附近的NPC（按R键）
};

// 强化学习训练数据
struct RLTrainingData
{
  std::vector<float> observation;  // 观察向量
  std::vector<float> action;       // 动作向量
  float reward;                    // 即时奖励
  bool done;                       // 是否终止
  std::vector<float> nextObservation; // 下一个观察
};

// AI策略接口
class AIStrategy
{
public:
  virtual ~AIStrategy() = default;
  virtual AIAction decide(const AIObservation& obs) = 0;
  virtual void reset() {}
  virtual std::string getName() const = 0;
};

// 规则AI（基线）- 学习自NPC的进攻逻辑
class RuleBasedAI : public AIStrategy
{
public:
  RuleBasedAI() = default;
  AIAction decide(const AIObservation& obs) override;
  void reset() override;
  std::string getName() const override { return "RuleBased"; }
  
private:
  float m_shootCooldown = 0.f;
  sf::Vector2f m_lastPosition;
  float m_stuckTimer = 0.f;
  int m_dodgeDirection = 1;
  float m_lateralTimer = 0.f;     // 侧向移动定时器
  float m_dodgeTimer = 0.f;       // 躲避摆动定时器
  float m_dodgeInterval = 0.3f;   // 躲避方向切换间隔
  bool m_hasValidShootTarget = false;  // 是否有有效射击目标
  
  // 策略辅助函数
  bool shouldDodge(const AIObservation& obs);
  sf::Vector2f getDodgeDirection(const AIObservation& obs);
  sf::Vector2f getPathFollowingMovement(const AIObservation& obs);  // 基于路径的移动
  sf::Vector2f getFindShootPositionMovement(const AIObservation& obs);  // 寻找射击位置的移动
  sf::Vector2f getCombatMovement(const AIObservation& obs);         // 战斗中的移动
  bool shouldShoot(const AIObservation& obs);
  float getOptimalTurretAngle(const AIObservation& obs);
};

// 强化学习AI
class RLAgent : public AIStrategy
{
public:
  RLAgent();
  ~RLAgent();
  
  AIAction decide(const AIObservation& obs) override;
  void reset() override;
  std::string getName() const override { return "RLAgent"; }
  
  // 训练接口
  void setTrainingMode(bool training) { m_training = training; }
  bool isTraining() const { return m_training; }
  
  void recordTransition(const AIObservation& obs, const AIAction& action, 
                       float reward, bool done, const AIObservation& nextObs);
  
  std::vector<RLTrainingData>& getTrainingBuffer() { return m_trainingBuffer; }
  void clearTrainingBuffer() { m_trainingBuffer.clear(); }
  
  // 模型加载/保存
  bool loadModel(const std::string& path);
  bool saveModel(const std::string& path);
  
  // 设置探索率（epsilon-greedy）
  void setExplorationRate(float epsilon) { m_epsilon = epsilon; }
  float getExplorationRate() const { return m_epsilon; }
  
private:
  bool m_training = false;
  float m_epsilon = 0.1f;  // 探索率
  std::vector<RLTrainingData> m_trainingBuffer;
  RuleBasedAI m_fallbackAI;  // 当模型未加载时使用规则AI
  
  // 神经网络权重（简化版，实际训练在Python端）
  struct NeuralNetwork
  {
    std::vector<std::vector<float>> weights;
    std::vector<float> biases;
    bool loaded = false;
  } m_network;
  
  // 观察转向量
  std::vector<float> observationToVector(const AIObservation& obs);
  
  // 向量转动作
  AIAction vectorToAction(const std::vector<float>& actionVec);
  
  // 前向传播
  std::vector<float> forward(const std::vector<float>& input);
};

// AI玩家控制器
class AIPlayer
{
public:
  AIPlayer(std::shared_ptr<Tank> tank, AIStrategy* strategy);
  
  // 设置游戏环境引用
  void setEnvironment(Maze* maze, Tank* opponent, 
                     std::vector<std::unique_ptr<Enemy>>* npcs,
                     std::vector<std::unique_ptr<Bullet>>* bullets);
  
  // 更新AI（生成观察，执行决策，应用动作）
  void update(float dt);
  
  // 获取观察状态（用于调试和训练）
  AIObservation getObservation() const;
  
  // 获取/设置策略
  void setStrategy(AIStrategy* strategy) { m_strategy = strategy; }
  AIStrategy* getStrategy() { return m_strategy; }
  
  // 视野系统
  void setVisionRange(float range) { m_visionRange = range; }
  float getVisionRange() const { return m_visionRange; }
  
  // 获取控制的坦克
  std::shared_ptr<Tank> getTank() { return m_tank; }
  
  // 重置AI状态
  void reset();
  
  // 训练相关
  void setReward(float reward) { m_lastReward = reward; }
  float getLastReward() const { return m_lastReward; }
  
  // 执行动作（将AIAction转换为Tank输入）
  void applyAction(const AIAction& action, float dt);
  
private:
  std::shared_ptr<Tank> m_tank;
  AIStrategy* m_strategy;
  
  // 环境引用
  Maze* m_maze = nullptr;
  Tank* m_opponent = nullptr;
  std::vector<std::unique_ptr<Enemy>>* m_npcs = nullptr;
  std::vector<std::unique_ptr<Bullet>>* m_bullets = nullptr;
  
  // 视野参数
  float m_visionRange = 400.f;  // 视野半径
  float m_visionAngle = 360.f;   // 视野角度（暂时360度全视野）
  
  // 路径规划缓存（像NPC一样）- mutable允许在const函数中修改
  mutable std::vector<sf::Vector2f> m_cachedPath;
  mutable size_t m_currentPathIndex = 0;
  mutable sf::Clock m_pathUpdateClock;
  float m_pathUpdateInterval = 0.5f;  // 路径更新间隔
  mutable bool m_hasDestructibleWallOnPath = false;
  mutable sf::Vector2f m_destructibleWallTarget;
  
  // 打墙锁定状态（防止瞄准来回跳动）
  mutable bool m_isShootingWall = false;
  mutable sf::Vector2f m_wallShootTarget;
  mutable sf::Clock m_wallShootClock;
  
  // 状态记录
  AIObservation m_lastObservation;
  AIAction m_lastAction;
  float m_lastReward = 0.f;
  
  // 射击冷却
  float m_shootCooldown = 0.f;
  const float SHOOT_COOLDOWN_TIME = 0.5f;
  
  // 辅助函数
  bool isInVisionRange(const sf::Vector2f& pos) const;
  bool hasLineOfSight(const sf::Vector2f& from, const sf::Vector2f& to) const;
  std::array<float, 8> calculateWallDistances() const;
};

// 奖励函数计算器 - AI目标：击败玩家，阻止玩家到达终点，自己永不触碰终点
class RewardCalculator
{
public:
  static float calculateReward(
    const AIObservation& obs,
    const AIObservation& nextObs,
    const AIAction& action,
    bool aiWon,           // AI击杀玩家
    bool aiLost,          // AI被玩家击杀
    bool playerReachedExit, // 玩家到达终点（AI失败）
    bool aiTouchedExit     // AI触碰终点（严重惩罚）
  );
  
  // 分项奖励权重 - 重新设计为对抗性AI
  // === 战斗相关 ===
  static constexpr float DAMAGE_DEALT_REWARD = 5.0f;     // 对玩家造成伤害
  static constexpr float DAMAGE_TAKEN_PENALTY = -2.0f;   // 被玩家伤害
  static constexpr float KILL_PLAYER_REWARD = 300.0f;    // 击杀玩家（主要目标）
  static constexpr float AI_DEATH_PENALTY = -150.0f;     // AI死亡
  
  // === 终点控制相关 ===
  static constexpr float PLAYER_EXIT_PENALTY = -400.0f;  // 玩家到达终点（AI最大失败）
  static constexpr float AI_TOUCH_EXIT_PENALTY = -500.0f; // AI触碰终点（严重惩罚！）
  static constexpr float BLOCK_PLAYER_EXIT_REWARD = 2.0f; // 阻挡玩家靠近终点
  static constexpr float EXIT_PROXIMITY_PENALTY = -3.0f;  // AI靠近终点的惩罚
  static constexpr float SAFE_EXIT_DISTANCE = 80.0f;      // AI与终点的安全距离
  
  // === 战术奖励 ===
  static constexpr float INTERCEPT_POSITION_REWARD = 1.5f; // 处于玩家和终点之间
  static constexpr float AGGRESSIVE_PURSUIT_REWARD = 0.5f; // 追击玩家
  static constexpr float NPC_ACTIVATED_REWARD = 15.0f;    // 激活NPC加入己方
  static constexpr float HIT_WALL_PENALTY = -0.3f;        // 撞墙
  static constexpr float SURVIVAL_REWARD = 0.02f;         // 生存时间
  
  // 辅助计算函数
  static bool isBlockingPlayerToExit(const AIObservation& obs);
  static float getExitBlockingScore(const AIObservation& obs);
};
