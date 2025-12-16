"""
Tank Battle AI Training Script - 对抗性AI训练
使用PPO算法训练坦克对战AI

AI目标：
1. 击败玩家（主要目标）
2. 阻止玩家到达终点（次要目标）
3. 永远不触碰终点（硬约束）

依赖:
  pip install torch numpy matplotlib

使用方法:
  1. 模拟训练（无游戏）: python train_ai.py --mode simulate --episodes 10000
  2. 评估模型: python train_ai.py --mode eval --model models/best_model.pth
  3. 导出C++可用模型: python train_ai.py --mode export --model models/best_model.pth
"""

import torch
import torch.nn as nn
import torch.optim as optim
import torch.nn.functional as F
import numpy as np
from collections import deque
import argparse
import json
import os
import math
import random
from datetime import datetime
from typing import Tuple, List, Dict, Optional

# ============================================================================
# 观察空间和动作空间定义
# ============================================================================

# 观察向量维度说明：
# [0-1]: AI位置 (x, y) / 1000
# [2-5]: AI朝向和炮塔朝向 (cos, sin) 
# [6]: AI血量 / 100
# [7]: 敌人可见标志
# [8-11]: 敌人相对位置和状态
# [12-19]: 8方向墙壁距离
# [20-22]: 终点相对位置和距离
# [23-42]: 5个NPC信息 (每个4维)
# [43-54]: 3个子弹信息 (每个4维)
# [55-57]: 额外战术信息

OBS_DIM = 58
ACTION_DIM = 5  # moveX, moveY, turretAngle, shoot, activateNPC

# ============================================================================
# Neural Network Architecture - 改进的Actor-Critic网络
# ============================================================================

class TacticalActorCritic(nn.Module):
    """
    战术感知的Actor-Critic网络
    专门设计用于对抗性AI：击败玩家、保护终点、避免触碰终点
    """
    def __init__(self, obs_dim=OBS_DIM, action_dim=ACTION_DIM, hidden_dim=256):
        super(TacticalActorCritic, self).__init__()
        
        # 特征提取 - 使用简单稳定的网络
        self.feature_net = nn.Sequential(
            nn.Linear(obs_dim, hidden_dim),
            nn.Tanh(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.Tanh(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.Tanh(),
        )
        
        # 战术评估分支
        self.tactical_net = nn.Sequential(
            nn.Linear(hidden_dim, 128),
            nn.Tanh(),
            nn.Linear(128, 64),
            nn.Tanh(),
        )
        
        # 安全评估分支
        self.safety_net = nn.Sequential(
            nn.Linear(hidden_dim, 64),
            nn.Tanh(),
            nn.Linear(64, 32),
            nn.Tanh(),
        )
        
        # Actor头
        combined_dim = hidden_dim + 64 + 32
        self.actor_mean = nn.Sequential(
            nn.Linear(combined_dim, 128),
            nn.Tanh(),
            nn.Linear(128, action_dim)
        )
        self.actor_logstd = nn.Parameter(torch.zeros(action_dim) - 0.5)  # 初始较小的方差
        
        # Critic头
        self.critic = nn.Sequential(
            nn.Linear(combined_dim, 128),
            nn.Tanh(),
            nn.Linear(128, 1)
        )
        
        # 初始化权重
        self._init_weights()
        
    def _init_weights(self):
        for m in self.modules():
            if isinstance(m, nn.Linear):
                nn.init.orthogonal_(m.weight, gain=0.5)
                nn.init.constant_(m.bias, 0)
    
    def forward(self, obs):
        # 检查输入中是否有NaN
        if torch.isnan(obs).any():
            obs = torch.nan_to_num(obs, nan=0.0)
        
        # 基础特征
        features = self.feature_net(obs)
        
        # 战术和安全分支
        tactical = self.tactical_net(features)
        safety = self.safety_net(features)
        
        # 合并所有特征
        combined = torch.cat([features, tactical, safety], dim=-1)
        
        # Actor
        action_mean = self.actor_mean(combined)
        
        # 确保action_mean不会太极端
        action_mean = torch.clamp(action_mean, -5, 5)
        
        action_std = torch.exp(torch.clamp(self.actor_logstd, -3, 0))
        
        # Critic
        value = self.critic(combined)
        
        return action_mean, action_std, value
    
    def get_action(self, obs, deterministic=False):
        """根据观察选择动作"""
        with torch.no_grad():
            if isinstance(obs, np.ndarray):
                obs_tensor = torch.FloatTensor(obs).unsqueeze(0)
            else:
                obs_tensor = obs.unsqueeze(0) if obs.dim() == 1 else obs
            
            # 检查输入
            if torch.isnan(obs_tensor).any():
                obs_tensor = torch.nan_to_num(obs_tensor, nan=0.0)
                
            action_mean, action_std, value = self.forward(obs_tensor)
            
            # 检查输出
            if torch.isnan(action_mean).any():
                action_mean = torch.zeros_like(action_mean)
            if torch.isnan(action_std).any() or (action_std <= 0).any():
                action_std = torch.ones_like(action_std) * 0.5
            
            if deterministic:
                action = action_mean
            else:
                dist = torch.distributions.Normal(action_mean, action_std)
                action = dist.sample()
            
            # 后处理动作
            processed_action = self._process_action(action)
            
            return processed_action.squeeze(0).numpy(), value.item()
    
    def _process_action(self, action):
        """处理动作输出"""
        processed = action.clone()
        # moveX, moveY: tanh到[-1, 1]
        processed[..., 0] = torch.tanh(action[..., 0])
        processed[..., 1] = torch.tanh(action[..., 1])
        # turretAngle: sigmoid到[0, 1]
        processed[..., 2] = torch.sigmoid(action[..., 2])
        # shoot, activateNPC: sigmoid到[0, 1]作为概率
        processed[..., 3] = torch.sigmoid(action[..., 3])
        processed[..., 4] = torch.sigmoid(action[..., 4])
        return processed
    
    def evaluate_actions(self, obs, actions):
        """评估给定观察和动作的log概率和价值"""
        action_mean, action_std, value = self.forward(obs)
        
        # 确保没有NaN
        if torch.isnan(action_mean).any() or torch.isnan(action_std).any():
            action_mean = torch.nan_to_num(action_mean, nan=0.0)
            action_std = torch.nan_to_num(action_std, nan=1.0)
        
        # 确保std是正数
        action_std = torch.clamp(action_std, min=1e-6)
        
        dist = torch.distributions.Normal(action_mean, action_std)
        action_log_probs = dist.log_prob(actions).sum(dim=-1, keepdim=True)
        entropy = dist.entropy().sum(dim=-1, keepdim=True)
        
        return action_log_probs, value, entropy


# ============================================================================
# 模拟环境 - 用于无游戏训练
# ============================================================================

class TankBattleSimulator:
    """
    坦克对战模拟器 - 用于强化学习训练
    模拟AI vs 简单玩家的对战场景
    """
    def __init__(self, map_size=1000, exit_zone_radius=50):
        self.map_size = map_size
        self.exit_zone_radius = exit_zone_radius
        self.exit_danger_zone = 100  # AI必须避开的区域
        
        self.reset()
    
    def reset(self) -> np.ndarray:
        """重置环境"""
        # 随机生成终点位置（在地图边缘区域）
        edge = random.choice(['top', 'bottom', 'left', 'right'])
        margin = 100
        if edge == 'top':
            self.exit_pos = np.array([random.uniform(margin, self.map_size - margin), margin])
        elif edge == 'bottom':
            self.exit_pos = np.array([random.uniform(margin, self.map_size - margin), self.map_size - margin])
        elif edge == 'left':
            self.exit_pos = np.array([margin, random.uniform(margin, self.map_size - margin)])
        else:
            self.exit_pos = np.array([self.map_size - margin, random.uniform(margin, self.map_size - margin)])
        
        # AI位置：远离终点的随机位置
        while True:
            self.ai_pos = np.array([random.uniform(200, self.map_size - 200),
                                   random.uniform(200, self.map_size - 200)])
            if np.linalg.norm(self.ai_pos - self.exit_pos) > 300:
                break
        
        # 玩家位置：远离AI和终点
        while True:
            self.player_pos = np.array([random.uniform(150, self.map_size - 150),
                                       random.uniform(150, self.map_size - 150)])
            if (np.linalg.norm(self.player_pos - self.ai_pos) > 200 and 
                np.linalg.norm(self.player_pos - self.exit_pos) > 200):
                break
        
        # 状态
        self.ai_health = 100.0
        self.player_health = 100.0
        self.ai_rotation = random.uniform(0, 360)
        self.ai_turret_rotation = random.uniform(0, 360)
        self.player_rotation = random.uniform(0, 360)
        
        self.step_count = 0
        self.max_steps = 1000
        
        # 简单的墙壁（矩形障碍物）
        self.walls = []
        for _ in range(random.randint(3, 8)):
            wx = random.uniform(150, self.map_size - 150)
            wy = random.uniform(150, self.map_size - 150)
            ww = random.uniform(30, 100)
            wh = random.uniform(30, 100)
            # 确保墙不会挡住终点
            if np.linalg.norm(np.array([wx, wy]) - self.exit_pos) > 150:
                self.walls.append((wx - ww/2, wy - wh/2, ww, wh))
        
        # 子弹
        self.bullets = []  # (pos, vel, owner) owner: 'ai' or 'player'
        
        self.ai_shoot_cooldown = 0.0
        self.player_shoot_cooldown = 0.0
        
        return self._get_observation()
    
    def step(self, action: np.ndarray) -> Tuple[np.ndarray, float, bool, Dict]:
        """执行一步"""
        self.step_count += 1
        dt = 0.016  # 假设60fps
        
        move_x, move_y = action[0], action[1]
        turret_angle = action[2] * 360  # [0,1] -> [0,360]
        shoot = action[3] > 0.5
        
        # 记录状态用于奖励计算
        old_ai_pos = self.ai_pos.copy()
        old_player_pos = self.player_pos.copy()
        old_ai_health = self.ai_health
        old_player_health = self.player_health
        old_ai_exit_dist = np.linalg.norm(self.ai_pos - self.exit_pos)
        old_player_exit_dist = np.linalg.norm(self.player_pos - self.exit_pos)
        
        # ==========================================================
        # 1. AI移动
        # ==========================================================
        ai_speed = 150.0  # 像素/秒
        move_vec = np.array([move_x, move_y])
        move_len = np.linalg.norm(move_vec)
        if move_len > 1:
            move_vec /= move_len
        
        new_ai_pos = self.ai_pos + move_vec * ai_speed * dt
        
        # 边界和墙壁碰撞检测
        if self._is_valid_position(new_ai_pos, 20):
            self.ai_pos = new_ai_pos
            if move_len > 0.1:
                self.ai_rotation = math.atan2(move_vec[1], move_vec[0]) * 180 / math.pi
        
        # 更新炮塔
        self.ai_turret_rotation = turret_angle
        
        # ==========================================================
        # 2. AI射击
        # ==========================================================
        self.ai_shoot_cooldown = max(0, self.ai_shoot_cooldown - dt)
        if shoot and self.ai_shoot_cooldown <= 0:
            bullet_speed = 400.0
            angle_rad = turret_angle * math.pi / 180
            bullet_vel = np.array([math.cos(angle_rad), math.sin(angle_rad)]) * bullet_speed
            self.bullets.append({
                'pos': self.ai_pos.copy(),
                'vel': bullet_vel,
                'owner': 'ai',
                'lifetime': 2.0
            })
            self.ai_shoot_cooldown = 0.5
        
        # ==========================================================
        # 3. 玩家AI（简单策略：朝终点移动，偶尔攻击AI）
        # ==========================================================
        player_speed = 130.0
        
        # 玩家策略：混合朝终点移动和躲避
        to_exit = self.exit_pos - self.player_pos
        to_exit_dist = np.linalg.norm(to_exit)
        to_exit_normalized = to_exit / (to_exit_dist + 0.001)
        
        # 如果AI靠近，有概率躲避
        to_ai = self.ai_pos - self.player_pos
        ai_dist = np.linalg.norm(to_ai)
        
        if ai_dist < 200 and random.random() < 0.3:
            # 躲避：垂直于AI方向移动
            perp = np.array([-to_ai[1], to_ai[0]]) / (ai_dist + 0.001)
            player_move = perp * (1 if random.random() > 0.5 else -1)
        else:
            # 朝终点移动
            player_move = to_exit_normalized + np.random.randn(2) * 0.1
        
        player_move_len = np.linalg.norm(player_move)
        if player_move_len > 1:
            player_move /= player_move_len
        
        new_player_pos = self.player_pos + player_move * player_speed * dt
        if self._is_valid_position(new_player_pos, 20):
            self.player_pos = new_player_pos
            self.player_rotation = math.atan2(player_move[1], player_move[0]) * 180 / math.pi
        
        # 玩家射击
        self.player_shoot_cooldown = max(0, self.player_shoot_cooldown - dt)
        if ai_dist < 300 and random.random() < 0.1 and self.player_shoot_cooldown <= 0:
            bullet_speed = 400.0
            to_ai_normalized = to_ai / (ai_dist + 0.001)
            # 添加瞄准误差
            angle_error = random.gauss(0, 10) * math.pi / 180
            cos_e, sin_e = math.cos(angle_error), math.sin(angle_error)
            rotated = np.array([
                to_ai_normalized[0] * cos_e - to_ai_normalized[1] * sin_e,
                to_ai_normalized[0] * sin_e + to_ai_normalized[1] * cos_e
            ])
            self.bullets.append({
                'pos': self.player_pos.copy(),
                'vel': rotated * bullet_speed,
                'owner': 'player',
                'lifetime': 2.0
            })
            self.player_shoot_cooldown = 0.7
        
        # ==========================================================
        # 4. 更新子弹
        # ==========================================================
        new_bullets = []
        for bullet in self.bullets:
            bullet['pos'] += bullet['vel'] * dt
            bullet['lifetime'] -= dt
            
            # 碰撞检测
            if bullet['owner'] == 'ai':
                if np.linalg.norm(bullet['pos'] - self.player_pos) < 25:
                    self.player_health -= 25
                    continue
            else:
                if np.linalg.norm(bullet['pos'] - self.ai_pos) < 25:
                    self.ai_health -= 25
                    continue
            
            # 墙壁碰撞
            hit_wall = False
            for wall in self.walls:
                if (wall[0] <= bullet['pos'][0] <= wall[0] + wall[2] and
                    wall[1] <= bullet['pos'][1] <= wall[1] + wall[3]):
                    hit_wall = True
                    break
            
            if not hit_wall and bullet['lifetime'] > 0 and self._in_bounds(bullet['pos']):
                new_bullets.append(bullet)
        
        self.bullets = new_bullets
        
        # ==========================================================
        # 5. 计算奖励和终止条件
        # ==========================================================
        reward = 0.0
        done = False
        info = {}
        
        new_ai_exit_dist = np.linalg.norm(self.ai_pos - self.exit_pos)
        new_player_exit_dist = np.linalg.norm(self.player_pos - self.exit_pos)
        
        # 检查AI是否触碰终点（严重惩罚！）
        ai_touched_exit = new_ai_exit_dist < self.exit_zone_radius
        if ai_touched_exit:
            reward = -500.0
            done = True
            info['result'] = 'ai_touched_exit'
            return self._get_observation(), reward, done, info
        
        # AI靠近终点的惩罚
        if new_ai_exit_dist < self.exit_danger_zone:
            proximity_penalty = -3.0 * (1.0 - new_ai_exit_dist / self.exit_danger_zone)
            reward += proximity_penalty
        
        # 检查玩家是否到达终点（AI失败）
        player_reached_exit = new_player_exit_dist < self.exit_zone_radius
        if player_reached_exit:
            reward = -400.0
            done = True
            info['result'] = 'player_escaped'
            return self._get_observation(), reward, done, info
        
        # AI击杀玩家（主要目标）
        if self.player_health <= 0:
            reward = 300.0
            done = True
            info['result'] = 'ai_won'
            return self._get_observation(), reward, done, info
        
        # AI死亡
        if self.ai_health <= 0:
            reward = -150.0
            done = True
            info['result'] = 'ai_died'
            return self._get_observation(), reward, done, info
        
        # 造成伤害奖励
        damage_dealt = old_player_health - self.player_health
        if damage_dealt > 0:
            reward += damage_dealt * 5.0 / 25.0
        
        # 受到伤害惩罚
        damage_taken = old_ai_health - self.ai_health
        if damage_taken > 0:
            reward += -damage_taken * 2.0 / 25.0
        
        # 阻止玩家接近终点奖励
        player_exit_progress = old_player_exit_dist - new_player_exit_dist
        if player_exit_progress > 0:
            # 玩家在接近终点，如果AI在拦截位置，奖励
            if self._is_blocking_player():
                reward += 2.0
        elif player_exit_progress < 0:
            # 玩家被迫远离终点
            reward += 1.0
        
        # 拦截位置奖励
        blocking_score = self._get_blocking_score()
        reward += blocking_score * 1.5
        
        # 追击玩家（在安全距离外）
        if new_ai_exit_dist > self.exit_danger_zone:
            old_dist_to_player = np.linalg.norm(old_ai_pos - old_player_pos)
            new_dist_to_player = np.linalg.norm(self.ai_pos - self.player_pos)
            if new_dist_to_player < old_dist_to_player:
                reward += 0.5 * (old_dist_to_player - new_dist_to_player) / 100.0
        
        # 生存奖励
        reward += 0.02
        
        # 撞墙惩罚
        min_wall_dist = self._get_min_wall_distance(self.ai_pos)
        if min_wall_dist < 30:
            reward -= 0.3
        
        # 超时
        if self.step_count >= self.max_steps:
            done = True
            info['result'] = 'timeout'
            # 根据当前状态给予奖励/惩罚
            if self.player_health < self.ai_health:
                reward += 50.0  # AI健康优势
            if new_player_exit_dist > 300:
                reward += 30.0  # 玩家远离终点
        
        return self._get_observation(), reward, done, info
    
    def _is_blocking_player(self) -> bool:
        """检查AI是否在拦截玩家"""
        player_to_exit = self.exit_pos - self.player_pos
        player_to_ai = self.ai_pos - self.player_pos
        
        player_exit_dist = np.linalg.norm(player_to_exit)
        player_ai_dist = np.linalg.norm(player_to_ai)
        
        if player_ai_dist < player_exit_dist * 0.8:
            dot = np.dot(player_to_ai, player_to_exit) / (player_exit_dist ** 2)
            if 0.2 < dot < 0.9:
                return True
        return False
    
    def _get_blocking_score(self) -> float:
        """获取拦截评分"""
        player_to_exit = self.exit_pos - self.player_pos
        player_exit_dist = np.linalg.norm(player_to_exit)
        
        if player_exit_dist < 10:
            return 0.0
        
        # 理想拦截位置
        t = min(0.7, max(0.3, (player_exit_dist - 50) / player_exit_dist))
        ideal_pos = self.player_pos + player_to_exit * t
        
        # 确保理想位置不太靠近终点
        ideal_exit_dist = np.linalg.norm(ideal_pos - self.exit_pos)
        if ideal_exit_dist < self.exit_danger_zone:
            direction = (ideal_pos - self.exit_pos) / (ideal_exit_dist + 0.001)
            ideal_pos = self.exit_pos + direction * self.exit_danger_zone
        
        dist_to_ideal = np.linalg.norm(self.ai_pos - ideal_pos)
        score = max(0, 1.0 - dist_to_ideal / 300.0)
        
        # 玩家接近终点时增加紧迫性
        if player_exit_dist < 200:
            score *= 2.0
        
        return min(score, 1.0)
    
    def _is_valid_position(self, pos: np.ndarray, radius: float) -> bool:
        """检查位置是否有效"""
        # 边界检查
        if pos[0] < radius or pos[0] > self.map_size - radius:
            return False
        if pos[1] < radius or pos[1] > self.map_size - radius:
            return False
        
        # 墙壁碰撞
        for wall in self.walls:
            if (wall[0] - radius <= pos[0] <= wall[0] + wall[2] + radius and
                wall[1] - radius <= pos[1] <= wall[1] + wall[3] + radius):
                return False
        
        return True
    
    def _in_bounds(self, pos: np.ndarray) -> bool:
        return 0 <= pos[0] <= self.map_size and 0 <= pos[1] <= self.map_size
    
    def _get_min_wall_distance(self, pos: np.ndarray) -> float:
        """获取到最近墙壁的距离"""
        min_dist = float('inf')
        
        # 边界
        min_dist = min(min_dist, pos[0], pos[1], 
                      self.map_size - pos[0], self.map_size - pos[1])
        
        # 墙壁（简化计算）
        for wall in self.walls:
            cx = wall[0] + wall[2] / 2
            cy = wall[1] + wall[3] / 2
            dist = np.linalg.norm(pos - np.array([cx, cy])) - max(wall[2], wall[3]) / 2
            min_dist = min(min_dist, dist)
        
        return max(0, min_dist)
    
    def _get_observation(self) -> np.ndarray:
        """生成观察向量"""
        obs = np.zeros(OBS_DIM, dtype=np.float32)
        
        # AI位置
        obs[0] = self.ai_pos[0] / self.map_size
        obs[1] = self.ai_pos[1] / self.map_size
        
        # AI朝向
        ai_rot_rad = self.ai_rotation * math.pi / 180
        obs[2] = math.cos(ai_rot_rad)
        obs[3] = math.sin(ai_rot_rad)
        
        # AI炮塔朝向
        turret_rad = self.ai_turret_rotation * math.pi / 180
        obs[4] = math.cos(turret_rad)
        obs[5] = math.sin(turret_rad)
        
        # AI血量
        obs[6] = self.ai_health / 100.0
        
        # 敌人（玩家）信息 - AI是全知的
        obs[7] = 1.0  # 敌人可见
        rel_player = (self.player_pos - self.ai_pos) / self.map_size
        obs[8] = rel_player[0]
        obs[9] = rel_player[1]
        obs[10] = self.player_health / 100.0
        obs[11] = np.linalg.norm(self.player_pos - self.ai_pos) / self.map_size
        
        # 8方向墙壁距离
        for i in range(8):
            angle = i * 45 * math.pi / 180
            direction = np.array([math.cos(angle), math.sin(angle)])
            dist = self._raycast(self.ai_pos, direction, 200)
            obs[12 + i] = dist / 200.0
        
        # 终点信息 - AI完全知道终点位置
        rel_exit = (self.exit_pos - self.ai_pos) / self.map_size
        obs[20] = rel_exit[0]
        obs[21] = rel_exit[1]
        obs[22] = np.linalg.norm(self.exit_pos - self.ai_pos) / self.map_size
        
        # NPC信息（这个模拟器没有NPC，填0）
        # obs[23:43] = 0 (已经初始化为0)
        
        # 子弹信息
        bullet_idx = 43
        for i, bullet in enumerate(self.bullets[:3]):
            if bullet_idx + 3 < OBS_DIM:
                rel_bullet = (bullet['pos'] - self.ai_pos) / self.map_size
                obs[bullet_idx] = rel_bullet[0]
                obs[bullet_idx + 1] = rel_bullet[1]
                obs[bullet_idx + 2] = bullet['vel'][0] / 500.0
                obs[bullet_idx + 3] = bullet['vel'][1] / 500.0
                bullet_idx += 4
        
        # 额外战术信息
        if 55 < OBS_DIM:
            # 玩家到终点的距离
            obs[55] = np.linalg.norm(self.player_pos - self.exit_pos) / self.map_size
            # 拦截评分
            obs[56] = self._get_blocking_score()
            # AI是否在危险区域
            obs[57] = 1.0 if np.linalg.norm(self.ai_pos - self.exit_pos) < self.exit_danger_zone else 0.0
        
        return obs
    
    def _raycast(self, origin: np.ndarray, direction: np.ndarray, max_dist: float) -> float:
        """射线检测"""
        for dist in np.arange(0, max_dist, 5):
            pos = origin + direction * dist
            if not self._in_bounds(pos):
                return dist
            for wall in self.walls:
                if (wall[0] <= pos[0] <= wall[0] + wall[2] and
                    wall[1] <= pos[1] <= wall[1] + wall[3]):
                    return dist
        return max_dist


# ============================================================================
# PPO Training Algorithm
# ============================================================================

class PPOTrainer:
    """PPO训练器 - 针对对抗性AI优化"""
    def __init__(
        self,
        obs_dim=OBS_DIM,
        action_dim=ACTION_DIM,
        hidden_dim=256,
        lr=3e-4,
        gamma=0.99,
        lambda_gae=0.95,
        clip_epsilon=0.2,
        value_coef=0.5,
        entropy_coef=0.01,
        max_grad_norm=0.5,
        ppo_epochs=10,
        mini_batch_size=64
    ):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        print(f"Using device: {self.device}")
        
        self.policy = TacticalActorCritic(obs_dim, action_dim, hidden_dim).to(self.device)
        self.optimizer = optim.Adam(self.policy.parameters(), lr=lr, eps=1e-5)
        
        # 学习率调度器
        self.scheduler = optim.lr_scheduler.StepLR(self.optimizer, step_size=1000, gamma=0.95)
        
        self.gamma = gamma
        self.lambda_gae = lambda_gae
        self.clip_epsilon = clip_epsilon
        self.value_coef = value_coef
        self.entropy_coef = entropy_coef
        self.max_grad_norm = max_grad_norm
        self.ppo_epochs = ppo_epochs
        self.mini_batch_size = mini_batch_size
        
        self.total_steps = 0
        self.episode_rewards = []
        self.episode_lengths = []
        
    def compute_gae(self, rewards, values, dones, next_value):
        """计算GAE"""
        advantages = []
        gae = 0
        
        values = values + [next_value]
        
        for step in reversed(range(len(rewards))):
            delta = rewards[step] + self.gamma * values[step + 1] * (1 - dones[step]) - values[step]
            gae = delta + self.gamma * self.lambda_gae * (1 - dones[step]) * gae
            advantages.insert(0, gae)
        
        returns = [adv + val for adv, val in zip(advantages, values[:-1])]
        
        return advantages, returns
    
    def update(self, rollout_buffer):
        """更新策略"""
        obs = torch.FloatTensor(np.array(rollout_buffer['observations'])).to(self.device)
        actions = torch.FloatTensor(np.array(rollout_buffer['actions'])).to(self.device)
        old_log_probs = torch.FloatTensor(np.array(rollout_buffer['log_probs'])).to(self.device).unsqueeze(-1)
        advantages = torch.FloatTensor(np.array(rollout_buffer['advantages'])).to(self.device).unsqueeze(-1)
        returns = torch.FloatTensor(np.array(rollout_buffer['returns'])).to(self.device).unsqueeze(-1)
        
        # 清理NaN
        obs = torch.nan_to_num(obs, nan=0.0)
        actions = torch.nan_to_num(actions, nan=0.0)
        old_log_probs = torch.nan_to_num(old_log_probs, nan=-10.0)
        advantages = torch.nan_to_num(advantages, nan=0.0)
        returns = torch.nan_to_num(returns, nan=0.0)
        
        # 标准化advantages
        adv_mean = advantages.mean()
        adv_std = advantages.std()
        if adv_std > 1e-8:
            advantages = (advantages - adv_mean) / (adv_std + 1e-8)
        else:
            advantages = advantages - adv_mean
        
        total_loss = 0
        policy_loss_sum = 0
        value_loss_sum = 0
        entropy_sum = 0
        num_updates = 0
        
        for _ in range(self.ppo_epochs):
            indices = np.arange(len(obs))
            np.random.shuffle(indices)
            
            for start in range(0, len(obs), self.mini_batch_size):
                end = min(start + self.mini_batch_size, len(obs))
                batch_indices = indices[start:end]
                
                new_log_probs, values, entropy = self.policy.evaluate_actions(
                    obs[batch_indices],
                    actions[batch_indices]
                )
                
                # 清理NaN
                new_log_probs = torch.nan_to_num(new_log_probs, nan=-10.0)
                values = torch.nan_to_num(values, nan=0.0)
                entropy = torch.nan_to_num(entropy, nan=0.0)
                
                # PPO目标
                ratio = torch.exp(torch.clamp(new_log_probs - old_log_probs[batch_indices], -10, 10))
                surr1 = ratio * advantages[batch_indices]
                surr2 = torch.clamp(ratio, 1 - self.clip_epsilon, 1 + self.clip_epsilon) * advantages[batch_indices]
                policy_loss = -torch.min(surr1, surr2).mean()
                
                # 价值损失
                value_loss = nn.functional.mse_loss(values, returns[batch_indices])
                value_loss = torch.clamp(value_loss, 0, 1000)  # 防止爆炸
                
                # 熵奖励
                entropy_loss = -entropy.mean()
                
                # 总损失
                loss = policy_loss + self.value_coef * value_loss + self.entropy_coef * entropy_loss
                
                # 检查损失是否有效
                if torch.isnan(loss) or torch.isinf(loss):
                    continue
                
                self.optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(self.policy.parameters(), self.max_grad_norm)
                self.optimizer.step()
                
                total_loss += loss.item()
                policy_loss_sum += policy_loss.item()
                value_loss_sum += value_loss.item()
                entropy_sum += entropy.mean().item()
                num_updates += 1
        
        self.scheduler.step()
        
        return {
            'total_loss': total_loss / max(num_updates, 1),
            'policy_loss': policy_loss_sum / max(num_updates, 1),
            'value_loss': value_loss_sum / max(num_updates, 1),
            'entropy': entropy_sum / max(num_updates, 1)
        }
    
    def save(self, path):
        """保存模型"""
        os.makedirs(os.path.dirname(path) if os.path.dirname(path) else '.', exist_ok=True)
        torch.save({
            'policy_state_dict': self.policy.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'total_steps': self.total_steps,
            'episode_rewards': self.episode_rewards,
            'episode_lengths': self.episode_lengths
        }, path)
        print(f"Model saved to {path}")
    
    def load(self, path):
        """加载模型"""
        if os.path.exists(path):
            checkpoint = torch.load(path, map_location=self.device)
            self.policy.load_state_dict(checkpoint['policy_state_dict'])
            self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
            self.total_steps = checkpoint.get('total_steps', 0)
            self.episode_rewards = checkpoint.get('episode_rewards', [])
            self.episode_lengths = checkpoint.get('episode_lengths', [])
            print(f"Model loaded from {path}")
        else:
            print(f"No model found at {path}, starting fresh")


# ============================================================================
# Training Functions
# ============================================================================

def train_simulate(args):
    """使用模拟器训练（无需游戏）"""
    print("=" * 60)
    print("Starting Simulated Training - 对抗性AI")
    print("目标：击败玩家，阻止玩家逃脱，永不触碰终点")
    print("=" * 60)
    
    trainer = PPOTrainer(
        obs_dim=OBS_DIM,
        action_dim=ACTION_DIM,
        hidden_dim=args.hidden_dim,
        lr=args.lr,
        gamma=args.gamma,
        clip_epsilon=args.clip_epsilon,
        ppo_epochs=args.ppo_epochs,
        mini_batch_size=args.batch_size
    )
    
    if args.load_model:
        trainer.load(args.load_model)
    
    env = TankBattleSimulator()
    
    # 统计
    wins = 0
    losses = 0
    escapes = 0
    exit_touches = 0
    timeouts = 0
    
    best_reward = -float('inf')
    
    for episode in range(args.episodes):
        obs = env.reset()
        
        rollout_buffer = {
            'observations': [],
            'actions': [],
            'rewards': [],
            'values': [],
            'log_probs': [],
            'dones': []
        }
        
        episode_reward = 0
        episode_length = 0
        done = False
        
        while not done and episode_length < args.max_steps:
            action, value = trainer.policy.get_action(obs, deterministic=False)
            
            next_obs, reward, done, info = env.step(action)
            
            rollout_buffer['observations'].append(obs)
            rollout_buffer['actions'].append(action)
            rollout_buffer['rewards'].append(reward)
            rollout_buffer['values'].append(value)
            rollout_buffer['dones'].append(done)
            
            # 计算log概率
            with torch.no_grad():
                obs_tensor = torch.FloatTensor(obs).unsqueeze(0).to(trainer.device)
                action_tensor = torch.FloatTensor(action).unsqueeze(0).to(trainer.device)
                log_prob, _, _ = trainer.policy.evaluate_actions(obs_tensor, action_tensor)
                rollout_buffer['log_probs'].append(log_prob.item())
            
            obs = next_obs
            episode_reward += reward
            episode_length += 1
            trainer.total_steps += 1
        
        # 统计结果
        result = info.get('result', 'unknown')
        if result == 'ai_won':
            wins += 1
        elif result == 'ai_died':
            losses += 1
        elif result == 'player_escaped':
            escapes += 1
        elif result == 'ai_touched_exit':
            exit_touches += 1
        elif result == 'timeout':
            timeouts += 1
        
        # 计算GAE并更新
        if len(rollout_buffer['observations']) > 0:
            with torch.no_grad():
                next_obs_tensor = torch.FloatTensor(obs).unsqueeze(0).to(trainer.device)
                _, _, next_value = trainer.policy.forward(next_obs_tensor)
                next_value = next_value.item()
            
            advantages, returns = trainer.compute_gae(
                rollout_buffer['rewards'],
                rollout_buffer['values'],
                rollout_buffer['dones'],
                next_value
            )
            
            rollout_buffer['advantages'] = advantages
            rollout_buffer['returns'] = returns
            
            update_info = trainer.update(rollout_buffer)
        else:
            update_info = {'total_loss': 0, 'policy_loss': 0, 'value_loss': 0}
        
        trainer.episode_rewards.append(episode_reward)
        trainer.episode_lengths.append(episode_length)
        
        # 每100回合打印统计
        if (episode + 1) % 100 == 0:
            avg_reward = np.mean(trainer.episode_rewards[-100:])
            print(f"\n{'='*60}")
            print(f"Episode {episode + 1}/{args.episodes}")
            print(f"Avg Reward (100): {avg_reward:.2f}")
            print(f"Win: {wins}, Loss: {losses}, Escape: {escapes}, Exit Touch: {exit_touches}, Timeout: {timeouts}")
            print(f"Win Rate: {wins/(episode+1)*100:.1f}%, Exit Touch Rate: {exit_touches/(episode+1)*100:.1f}%")
            print(f"Loss: {update_info['total_loss']:.4f}")
            
            # 重置统计
            if (episode + 1) % 1000 == 0:
                wins = losses = escapes = exit_touches = timeouts = 0
            
            # 保存最佳模型
            if avg_reward > best_reward:
                best_reward = avg_reward
                trainer.save(os.path.join(args.save_dir, 'best_model.pth'))
        
        # 定期保存
        if (episode + 1) % args.save_interval == 0:
            trainer.save(os.path.join(args.save_dir, f'model_ep{episode+1}.pth'))
    
    # 保存最终模型
    trainer.save(os.path.join(args.save_dir, 'final_model.pth'))
    
    print("\n" + "=" * 60)
    print("Training Complete!")
    print(f"Best Average Reward: {best_reward:.2f}")
    print("=" * 60)


def export_model(args):
    """导出模型为C++可用格式"""
    print("Exporting model for C++...")
    
    trainer = PPOTrainer()
    trainer.load(args.model)
    
    # 导出为TorchScript
    trainer.policy.eval()
    example_input = torch.randn(1, OBS_DIM)
    
    # 尝试trace
    try:
        traced_script = torch.jit.trace(trainer.policy, example_input)
        export_path = args.model.replace('.pth', '_traced.pt')
        traced_script.save(export_path)
        print(f"TorchScript model exported to {export_path}")
    except Exception as e:
        print(f"TorchScript export failed: {e}")
    
    # 导出权重为JSON格式（用于简单的C++实现）
    weights_dict = {}
    for name, param in trainer.policy.named_parameters():
        weights_dict[name] = param.detach().cpu().numpy().tolist()
    
    json_path = args.model.replace('.pth', '_weights.json')
    with open(json_path, 'w') as f:
        json.dump(weights_dict, f, indent=2)
    print(f"Weights exported to {json_path}")


def evaluate(args):
    """评估模型"""
    print("Evaluating model...")
    
    trainer = PPOTrainer()
    trainer.load(args.model)
    
    env = TankBattleSimulator()
    
    results = {'wins': 0, 'losses': 0, 'escapes': 0, 'exit_touches': 0, 'timeouts': 0}
    total_rewards = []
    
    for episode in range(args.eval_episodes):
        obs = env.reset()
        episode_reward = 0
        done = False
        
        while not done:
            action, _ = trainer.policy.get_action(obs, deterministic=True)
            obs, reward, done, info = env.step(action)
            episode_reward += reward
        
        total_rewards.append(episode_reward)
        result = info.get('result', 'unknown')
        
        if result == 'ai_won':
            results['wins'] += 1
        elif result == 'ai_died':
            results['losses'] += 1
        elif result == 'player_escaped':
            results['escapes'] += 1
        elif result == 'ai_touched_exit':
            results['exit_touches'] += 1
        elif result == 'timeout':
            results['timeouts'] += 1
        
        print(f"Episode {episode + 1}: Reward = {episode_reward:.2f}, Result = {result}")
    
    print(f"\n{'='*60}")
    print(f"Evaluation Results ({args.eval_episodes} episodes):")
    print(f"Average Reward: {np.mean(total_rewards):.2f} ± {np.std(total_rewards):.2f}")
    print(f"Results: {results}")
    print(f"Win Rate: {results['wins']/args.eval_episodes*100:.1f}%")
    print(f"Exit Touch Rate: {results['exit_touches']/args.eval_episodes*100:.1f}%")


def main():
    parser = argparse.ArgumentParser(description='Train Adversarial Tank AI')
    
    parser.add_argument('--mode', type=str, default='simulate', 
                       choices=['simulate', 'train', 'eval', 'export'],
                       help='Training mode')
    
    parser.add_argument('--episodes', type=int, default=10000)
    parser.add_argument('--max_steps', type=int, default=1000)
    parser.add_argument('--hidden_dim', type=int, default=256)
    parser.add_argument('--lr', type=float, default=3e-4)
    parser.add_argument('--gamma', type=float, default=0.99)
    parser.add_argument('--clip_epsilon', type=float, default=0.2)
    parser.add_argument('--ppo_epochs', type=int, default=10)
    parser.add_argument('--batch_size', type=int, default=64)
    
    parser.add_argument('--save_dir', type=str, default='models')
    parser.add_argument('--save_interval', type=int, default=500)
    parser.add_argument('--load_model', type=str, default=None)
    parser.add_argument('--model', type=str, default='models/best_model.pth')
    
    parser.add_argument('--eval_episodes', type=int, default=100)
    
    args = parser.parse_args()
    
    if args.mode == 'simulate':
        train_simulate(args)
    elif args.mode == 'train':
        train_simulate(args)  # 同样使用模拟训练
    elif args.mode == 'eval':
        evaluate(args)
    elif args.mode == 'export':
        export_model(args)

if __name__ == '__main__':
    main()
