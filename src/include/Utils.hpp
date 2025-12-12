#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>

// 全局常量
constexpr float TILE_SIZE = 70.f;

namespace Utils
{
  constexpr float PI = 3.14159265f;

  // 计算从 from 到 to 的角度（度）
  inline float getAngle(sf::Vector2f from, sf::Vector2f to)
  {
    sf::Vector2f diff = to - from;
    return std::atan2(diff.y, diff.x) * 180.f / PI + 90.f;
  }

  // 计算向量方向的角度（度）
  inline float getDirectionAngle(sf::Vector2f dir)
  {
    return std::atan2(dir.y, dir.x) * 180.f / PI + 90.f;
  }

  // 平滑插值角度（处理 -180 到 180 的跨越）
  inline float lerpAngle(float current, float target, float t)
  {
    float diff = target - current;

    while (diff > 180.f)
      diff -= 360.f;
    while (diff < -180.f)
      diff += 360.f;

    return current + diff * t;
  }
}
