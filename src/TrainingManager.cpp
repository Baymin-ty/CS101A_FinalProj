#include "include/TrainingManager.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

TrainingManager::TrainingManager(const std::string& commDir)
  : m_commDir(commDir)
{
  // 创建通信目录
  std::filesystem::create_directories(commDir);
  
  m_obsFile = commDir + "/observation.json";
  m_actionFile = commDir + "/action.json";
  m_statusFile = commDir + "/status.json";
  
  std::cout << "[Training] Communication directory: " << commDir << std::endl;
}

bool TrainingManager::checkForCommand(std::string& command)
{
  if (!std::filesystem::exists(m_statusFile)) {
    return false;
  }
  
  try {
    std::ifstream file(m_statusFile);
    json data;
    file >> data;
    file.close();
    
    // 删除文件避免重复读取
    std::filesystem::remove(m_statusFile);
    
    if (data.contains("command")) {
      command = data["command"];
      return true;
    }
  } catch (const std::exception& e) {
    std::cerr << "[Training] Error reading status: " << e.what() << std::endl;
  }
  
  return false;
}

bool TrainingManager::readAction(AIAction& action)
{
  if (!std::filesystem::exists(m_actionFile)) {
    return false;
  }
  
  try {
    std::ifstream file(m_actionFile);
    json data;
    file >> data;
    file.close();
    
    // 删除文件避免重复读取
    std::filesystem::remove(m_actionFile);
    
    action.moveX = data.value("moveX", 0.0f);
    action.moveY = data.value("moveY", 0.0f);
    action.turretAngle = data.value("turretAngle", 0.0f);
    action.shoot = data.value("shoot", false);
    action.activateNPC = data.value("activateNPC", false);
    
    return true;
  } catch (const std::exception& e) {
    std::cerr << "[Training] Error reading action: " << e.what() << std::endl;
    return false;
  }
}

void TrainingManager::sendObservation(const AIObservation& obs, float reward, bool done, const std::string& info)
{
  try {
    json data;
    data["observation"] = observationToJson(obs);
    data["reward"] = reward;
    data["done"] = done;
    data["info"] = info;
    
    std::ofstream file(m_obsFile);
    file << data.dump(2);
    file.close();
  } catch (const std::exception& e) {
    std::cerr << "[Training] Error writing observation: " << e.what() << std::endl;
  }
}

float TrainingManager::calculateReward(const AIObservation& lastObs, const AIObservation& currentObs, 
                                       bool gameOver, bool won)
{
  float reward = 0.0f;
  
  // 游戏结束奖励
  if (gameOver) {
    if (won) {
      reward += 1000.0f;  // 击败对手
    } else {
      reward -= 1000.0f;  // 被击败
    }
    return reward;
  }
  
  // 生存奖励
  reward += 0.1f;
  
  // 伤害奖励
  float healthDiff = currentObs.health - lastObs.health;
  if (healthDiff < 0) {
    reward += healthDiff * 2.0f;  // 受伤惩罚
  }
  
  float enemyHealthDiff = lastObs.enemyHealth - currentObs.enemyHealth;
  if (enemyHealthDiff > 0) {
    reward += enemyHealthDiff * 5.0f;  // 对敌人造成伤害
  }
  
  // 距离奖励（接近敌人）
  if (currentObs.enemyVisible) {
    float lastDist = lastObs.enemyDistance;
    float currentDist = currentObs.enemyDistance;
    
    if (lastDist > 0 && currentDist < lastDist) {
      reward += (lastDist - currentDist) * 0.05f;  // 接近敌人
    }
    
    // 保持适当距离（200-400像素）
    if (currentDist >= 200.0f && currentDist <= 400.0f) {
      reward += 1.0f;
    }
  }
  
  // 金币奖励
  int coinDiff = currentObs.coins - lastObs.coins;
  if (coinDiff > 0) {
    reward += coinDiff * 2.0f;
  }
  
  // 避免接近出口
  if (currentObs.exitDistance < 100.0f) {
    reward -= 10.0f;  // 大惩罚
  }
  
  return reward;
}

json TrainingManager::observationToJson(const AIObservation& obs)
{
  json j;
  
  // 转换为向量形式（与Python端匹配）
  std::vector<float> vec;
  
  // 自身状态 (8维)
  vec.push_back(obs.position.x / 1000.0f);
  vec.push_back(obs.position.y / 1000.0f);
  vec.push_back(obs.rotation / 360.0f);
  vec.push_back(obs.turretRotation / 360.0f);
  vec.push_back(obs.health / 100.0f);
  vec.push_back(obs.coins / 10.0f);
  vec.push_back(0.0f);  // 保留
  vec.push_back(0.0f);  // 保留
  
  // 墙壁距离 (8维)
  for (int i = 0; i < 8; ++i) {
    vec.push_back(obs.wallDistances[i] / 500.0f);
  }
  
  // 敌人信息 (4维)
  if (obs.enemyVisible) {
    vec.push_back((obs.enemyPosition.x - obs.position.x) / 1000.0f);
    vec.push_back((obs.enemyPosition.y - obs.position.y) / 1000.0f);
    vec.push_back(obs.enemyHealth / 100.0f);
    vec.push_back(obs.enemyDistance / 1000.0f);
  } else {
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(-1.0f);
  }
  
  // 出口信息 (3维)
  vec.push_back((obs.exitPosition.x - obs.position.x) / 1000.0f);
  vec.push_back((obs.exitPosition.y - obs.position.y) / 1000.0f);
  vec.push_back(obs.exitDistance / 1000.0f);
  
  // NPC信息 (最多5个，每个4维 = 20维)
  int npcCount = std::min(5, static_cast<int>(obs.visibleNPCPositions.size()));
  for (int i = 0; i < npcCount; ++i) {
    vec.push_back((obs.visibleNPCPositions[i].x - obs.position.x) / 1000.0f);
    vec.push_back((obs.visibleNPCPositions[i].y - obs.position.y) / 1000.0f);
    vec.push_back(obs.visibleNPCTeams[i] / 2.0f);
    vec.push_back(obs.visibleNPCHealths[i] / 100.0f);
  }
  for (int i = npcCount; i < 5; ++i) {
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(0.0f);
  }
  
  // 子弹信息 (最多3个，每个4维 = 12维)
  int bulletCount = std::min(3, static_cast<int>(obs.visibleBulletPositions.size()));
  for (int i = 0; i < bulletCount; ++i) {
    vec.push_back((obs.visibleBulletPositions[i].x - obs.position.x) / 1000.0f);
    vec.push_back((obs.visibleBulletPositions[i].y - obs.position.y) / 1000.0f);
    vec.push_back(obs.visibleBulletVelocities[i].x / 500.0f);
    vec.push_back(obs.visibleBulletVelocities[i].y / 500.0f);
  }
  for (int i = bulletCount; i < 3; ++i) {
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(0.0f);
    vec.push_back(0.0f);
  }
  
  j["vector"] = vec;
  
  return j;
}
