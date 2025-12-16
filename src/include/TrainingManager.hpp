#pragma once

#include <string>
#include "json.hpp"
#include "AIPlayer.hpp"

using json = nlohmann::json;

/**
 * 训练管理器：处理与Python训练脚本的通信
 */
class TrainingManager
{
public:
  TrainingManager(const std::string& commDir = "ai_training_data");
  
  // 检查是否有训练命令
  bool checkForCommand(std::string& command);
  
  // 读取Python发送的动作
  bool readAction(AIAction& action);
  
  // 发送观察和奖励给Python
  void sendObservation(const AIObservation& obs, float reward, bool done, const std::string& info = "");
  
  // 计算奖励
  float calculateReward(const AIObservation& lastObs, const AIObservation& currentObs, bool gameOver, bool won);
  
  // 转换观察为JSON
  json observationToJson(const AIObservation& obs);
  
private:
  std::string m_commDir;
  std::string m_obsFile;
  std::string m_actionFile;
  std::string m_statusFile;
};
