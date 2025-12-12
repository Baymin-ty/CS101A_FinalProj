#include "include/MultiplayerHandler.hpp"
#include "include/UIHelper.hpp"
#include "include/CollisionSystem.hpp"
#include "include/Utils.hpp"
#include <cmath>
#include <iostream>

void MultiplayerHandler::update(
  MultiplayerContext& ctx,
  MultiplayerState& state,
  float dt,
  std::function<void()> onVictory,
  std::function<void()> onDefeat)
{
  if (!ctx.player) return;

  // 获取鼠标在世界坐标中的位置
  sf::Vector2i mousePixelPos = sf::Mouse::getPosition(ctx.window);
  sf::Vector2f mouseWorldPos = ctx.window.mapPixelToCoords(mousePixelPos, ctx.gameView);

  // 保存旧位置
  sf::Vector2f oldPos = ctx.player->getPosition();
  sf::Vector2f movement = ctx.player->getMovement(dt);

  // 更新本地玩家
  ctx.player->update(dt, mouseWorldPos);

  // 碰撞检测
  sf::Vector2f newPos = ctx.player->getPosition();
  float radius = ctx.player->getCollisionRadius();

  if (ctx.maze.checkCollision(newPos, radius)) {
    sf::Vector2f posX = {oldPos.x + movement.x, oldPos.y};
    sf::Vector2f posY = {oldPos.x, oldPos.y + movement.y};

    bool canMoveX = !ctx.maze.checkCollision(posX, radius);
    bool canMoveY = !ctx.maze.checkCollision(posY, radius);

    if (canMoveX && canMoveY) {
      if (std::abs(movement.x) > std::abs(movement.y))
        ctx.player->setPosition(posX);
      else
        ctx.player->setPosition(posY);
    } else if (canMoveX) {
      ctx.player->setPosition(posX);
    } else if (canMoveY) {
      ctx.player->setPosition(posY);
    } else {
      ctx.player->setPosition(oldPos);
    }
  }

  // 检查玩家是否接近未激活的NPC
  checkNearbyNpc(ctx, state);

  // 处理NPC激活
  handleNpcActivation(ctx, state);

  // 更新NPC AI（仅房主）
  updateNpcAI(ctx, state, dt);

  // 发送位置到服务器
  auto& net = NetworkManager::getInstance();
  PlayerState pstate;
  pstate.x = ctx.player->getPosition().x;
  pstate.y = ctx.player->getPosition().y;
  pstate.rotation = ctx.player->getRotation();
  pstate.turretAngle = ctx.player->getTurretRotation();
  pstate.health = ctx.player->getHealth();
  pstate.reachedExit = state.localPlayerReachedExit;
  net.sendPosition(pstate);

  // 处理射击
  if (ctx.player->hasFiredBullet()) {
    sf::Vector2f bulletPos = ctx.player->getBulletSpawnPosition();
    float bulletAngle = ctx.player->getTurretRotation();
    ctx.bullets.push_back(std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, true));
    net.sendShoot(bulletPos.x, bulletPos.y, bulletAngle);
  }

  // 更新迷宫
  ctx.maze.update(dt);

  // 更新子弹
  for (auto& bullet : ctx.bullets) {
    bullet->update(dt);
  }

  // 子弹碰撞检测
  CollisionSystem::checkMultiplayerCollisions(
    ctx.player, ctx.otherPlayer, ctx.enemies, ctx.bullets, ctx.maze, state.isHost);

  // 删除超出范围的子弹
  ctx.bullets.erase(
    std::remove_if(ctx.bullets.begin(), ctx.bullets.end(),
      [](const std::unique_ptr<Bullet>& b) { return !b->isAlive(); }),
    ctx.bullets.end());

  // 检查玩家是否到达终点
  sf::Vector2f exitPos = ctx.maze.getExitPosition();
  float distToExit = std::hypot(ctx.player->getPosition().x - exitPos.x,
                                 ctx.player->getPosition().y - exitPos.y);

  if (distToExit < TILE_SIZE && !state.localPlayerReachedExit) {
    state.localPlayerReachedExit = true;
    state.multiplayerWin = true;
    net.sendGameResult(true);
    onVictory();
    return;
  }

  // 检查本地玩家是否死亡
  if (ctx.player->isDead()) {
    state.multiplayerWin = false;
    net.sendGameResult(false);
    onDefeat();
    return;
  }

  // 更新相机
  ctx.gameView.setCenter(ctx.player->getPosition());
}

void MultiplayerHandler::checkNearbyNpc(
  MultiplayerContext& ctx,
  MultiplayerState& state)
{
  state.nearbyNpcIndex = -1;
  sf::Vector2f playerPos = ctx.player->getPosition();

  for (size_t i = 0; i < ctx.enemies.size(); ++i) {
    auto& npc = ctx.enemies[i];
    if (!npc->isActivated()) {
      sf::Vector2f npcPos = npc->getPosition();
      float dx = playerPos.x - npcPos.x;
      float dy = playerPos.y - npcPos.y;
      float dist = std::sqrt(dx * dx + dy * dy);

      if (dist < 80.f) {
        state.nearbyNpcIndex = static_cast<int>(i);
        break;
      }
    }
  }
}

void MultiplayerHandler::handleNpcActivation(
  MultiplayerContext& ctx,
  MultiplayerState& state)
{
  if (state.nearbyNpcIndex >= 0 && state.rKeyJustPressed) {
    int localTeam = ctx.player->getTeam();
    std::cout << "[DEBUG] R key triggered! NPC index: " << state.nearbyNpcIndex << std::endl;
    if (ctx.player->getCoins() >= 3) {
      ctx.player->spendCoins(3);
      ctx.enemies[state.nearbyNpcIndex]->activate(localTeam);
      NetworkManager::getInstance().sendNpcActivate(state.nearbyNpcIndex, localTeam);
      std::cout << "[DEBUG] Activated NPC " << state.nearbyNpcIndex << ", coins left: " << ctx.player->getCoins() << std::endl;
      state.nearbyNpcIndex = -1;
    } else {
      std::cout << "[DEBUG] Not enough coins to activate NPC" << std::endl;
    }
  }
  state.rKeyJustPressed = false;
}

void MultiplayerHandler::updateNpcAI(
  MultiplayerContext& ctx,
  MultiplayerState& state,
  float dt)
{
  auto& net = NetworkManager::getInstance();

  for (size_t i = 0; i < ctx.enemies.size(); ++i) {
    auto& npc = ctx.enemies[i];
    if (npc->isDead())
      continue;

    if (npc->isActivated()) {
      int npcTeam = npc->getTeam();

      // 只有房主执行NPC AI逻辑
      if (state.isHost) {
        // 收集敌对目标
        std::vector<sf::Vector2f> targets;

        if (ctx.player && ctx.player->getTeam() != npcTeam && npcTeam != 0) {
          targets.push_back(ctx.player->getPosition());
        }

        if (ctx.otherPlayer && ctx.otherPlayer->getTeam() != npcTeam && npcTeam != 0) {
          targets.push_back(ctx.otherPlayer->getPosition());
        }

        for (const auto& otherNpc : ctx.enemies) {
          if (otherNpc.get() != npc.get() &&
              otherNpc->isActivated() &&
              !otherNpc->isDead() &&
              otherNpc->getTeam() != npcTeam &&
              otherNpc->getTeam() != 0) {
            targets.push_back(otherNpc->getPosition());
          }
        }

        if (!targets.empty()) {
          npc->setTargets(targets);
        }

        npc->update(dt, ctx.maze);

        // NPC射击
        if (npc->shouldShoot()) {
          sf::Vector2f bulletPos = npc->getGunPosition();
          float bulletAngle = npc->getTurretAngle();
          sf::Color bulletColor = (npcTeam == 1) ? sf::Color::Yellow : sf::Color::Magenta;
          auto bullet = std::make_unique<Bullet>(bulletPos.x, bulletPos.y, bulletAngle, false, bulletColor);
          bullet->setTeam(npcTeam);
          ctx.bullets.push_back(std::move(bullet));
          net.sendNpcShoot(static_cast<int>(i), bulletPos.x, bulletPos.y, bulletAngle);
        }

        // 定期同步NPC状态
        state.npcSyncCounter++;
        if ((state.npcSyncCounter + static_cast<int>(i)) % 5 == 0) {
          NpcState npcState;
          npcState.id = static_cast<int>(i);
          npcState.x = npc->getPosition().x;
          npcState.y = npc->getPosition().y;
          npcState.rotation = npc->getRotation();
          npcState.turretAngle = npc->getTurretAngle();
          npcState.health = npc->getHealth();
          npcState.team = npc->getTeam();
          npcState.activated = npc->isActivated();
          net.sendNpcUpdate(npcState);
        }
      }
    }
  }
}

void MultiplayerHandler::renderConnecting(
  sf::RenderWindow& window,
  sf::View& uiView,
  sf::Font& font,
  unsigned int screenWidth,
  unsigned int screenHeight,
  const std::string& connectionStatus,
  const std::string& inputText,
  bool isServerIPMode)
{
  window.setView(uiView);
  window.clear(sf::Color(30, 30, 50));

  UIHelper::drawCenteredText(window, font, "Multiplayer", 48, sf::Color::White, 80.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, connectionStatus, 24, sf::Color::Yellow, 180.f, static_cast<float>(screenWidth));

  std::string labelText = isServerIPMode ? "Server IP:" : "Room Code (or press C to create):";
  UIHelper::drawCenteredText(window, font, labelText, 24, sf::Color::White, 260.f, static_cast<float>(screenWidth));

  UIHelper::drawInputBox(window, font, inputText, 
    (screenWidth - 400.f) / 2.f, 300.f, 400.f, 50.f);

  UIHelper::drawCenteredText(window, font, "Press ENTER to confirm, ESC to cancel", 
    20, sf::Color(150, 150, 150), 400.f, static_cast<float>(screenWidth));

  window.display();
}

void MultiplayerHandler::renderWaitingForPlayer(
  sf::RenderWindow& window,
  sf::View& uiView,
  sf::Font& font,
  unsigned int screenWidth,
  unsigned int screenHeight,
  const std::string& roomCode)
{
  window.setView(uiView);
  window.clear(sf::Color(30, 30, 50));

  UIHelper::drawCenteredText(window, font, "Waiting for Player", 48, sf::Color::White, 80.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Room Code: " + roomCode, 36, sf::Color::Green, 200.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Share this code with your friend!", 24, sf::Color::Yellow, 280.f, static_cast<float>(screenWidth));

  // 动画点
  static float dotTime = 0;
  dotTime += 0.016f;
  int dots = static_cast<int>(dotTime * 2) % 4;
  std::string waiting = "Waiting";
  for (int i = 0; i < dots; i++) waiting += ".";

  UIHelper::drawCenteredText(window, font, waiting, 28, sf::Color::White, 360.f, static_cast<float>(screenWidth));
  UIHelper::drawCenteredText(window, font, "Press ESC to cancel", 20, sf::Color(150, 150, 150), 450.f, static_cast<float>(screenWidth));

  window.display();
}

void MultiplayerHandler::renderMultiplayer(
  MultiplayerContext& ctx,
  MultiplayerState& state)
{
  ctx.window.clear(sf::Color(30, 30, 30));
  ctx.window.setView(ctx.gameView);

  // 渲染迷宫
  ctx.maze.render(ctx.window);

  // 渲染终点
  sf::Vector2f exitPos = ctx.maze.getExitPosition();
  sf::RectangleShape exitMarker({TILE_SIZE * 0.8f, TILE_SIZE * 0.8f});
  exitMarker.setFillColor(sf::Color(0, 255, 0, 100));
  exitMarker.setOutlineColor(sf::Color::Green);
  exitMarker.setOutlineThickness(3.f);
  exitMarker.setPosition({exitPos.x - TILE_SIZE * 0.4f, exitPos.y - TILE_SIZE * 0.4f});
  ctx.window.draw(exitMarker);

  // 渲染NPC
  renderNpcs(ctx, state);

  // 渲染另一个玩家
  if (ctx.otherPlayer) {
    ctx.otherPlayer->render(ctx.window);
    if (state.otherPlayerReachedExit) {
      UIHelper::drawTeamMarker(ctx.window,
        {ctx.otherPlayer->getPosition().x, ctx.otherPlayer->getPosition().y - 25.f},
        15.f, sf::Color(0, 255, 0, 150));
    }
  }

  // 渲染本地玩家
  if (ctx.player) {
    ctx.player->render(ctx.window);
    if (state.localPlayerReachedExit) {
      UIHelper::drawTeamMarker(ctx.window,
        {ctx.player->getPosition().x, ctx.player->getPosition().y - 25.f},
        15.f, sf::Color(0, 255, 0, 150));
    }
  }

  // 渲染子弹
  for (const auto& bullet : ctx.bullets) {
    bullet->render(ctx.window);
  }

  // NPC激活提示
  if (state.nearbyNpcIndex >= 0 && state.nearbyNpcIndex < static_cast<int>(ctx.enemies.size())) {
    auto& npc = ctx.enemies[state.nearbyNpcIndex];
    sf::Vector2f npcPos = npc->getPosition();

    sf::Text activateHint(ctx.font);
    if (ctx.player->getCoins() >= 3) {
      activateHint.setString("Press R (3 coins)");
      activateHint.setFillColor(sf::Color::Yellow);
    } else {
      activateHint.setString("Need 3 coins!");
      activateHint.setFillColor(sf::Color::Red);
    }
    activateHint.setCharacterSize(14);
    sf::FloatRect hintBounds = activateHint.getLocalBounds();
    activateHint.setPosition({npcPos.x - hintBounds.size.x / 2.f, npcPos.y - 55.f});
    ctx.window.draw(activateHint);
  }

  // 渲染UI
  renderUI(ctx, state);

  ctx.window.display();
}

void MultiplayerHandler::renderNpcs(
  MultiplayerContext& ctx,
  MultiplayerState& state)
{
  for (const auto& npc : ctx.enemies) {
    if (npc->isDead()) continue;

    npc->draw(ctx.window);

    sf::Vector2f npcPos = npc->getPosition();
    if (npc->isActivated()) {
      sf::Color markerColor = (npc->getTeam() == ctx.player->getTeam()) 
        ? sf::Color(0, 255, 0, 200)   // 己方：绿色
        : sf::Color(255, 0, 0, 200);  // 敌方：红色
      UIHelper::drawTeamMarker(ctx.window, {npcPos.x, npcPos.y - 27.f}, 8.f, markerColor);
    } else {
      UIHelper::drawTeamMarker(ctx.window, {npcPos.x, npcPos.y - 27.f}, 8.f, sf::Color(150, 150, 150, 200));
    }
  }
}

void MultiplayerHandler::renderUI(
  MultiplayerContext& ctx,
  MultiplayerState& state)
{
  ctx.window.setView(ctx.uiView);

  float barWidth = 150.f;
  float barHeight = 20.f;
  float barX = 20.f;
  float barY = 20.f;

  // Self 标签和血条
  sf::Text selfLabel(ctx.font);
  selfLabel.setString("Self");
  selfLabel.setCharacterSize(18);
  selfLabel.setFillColor(sf::Color::White);
  selfLabel.setPosition({barX, barY - 2.f});
  ctx.window.draw(selfLabel);

  float selfHealthPercent = ctx.player ? (ctx.player->getHealth() / 100.f) : 0.f;
  UIHelper::drawHealthBar(ctx.window, barX + 50.f, barY, barWidth, barHeight,
                          selfHealthPercent, sf::Color::Green);

  // Other 标签和血条
  sf::Text otherLabel(ctx.font);
  otherLabel.setString("Other");
  otherLabel.setCharacterSize(18);
  otherLabel.setFillColor(sf::Color::White);
  otherLabel.setPosition({barX, barY + 30.f - 2.f});
  ctx.window.draw(otherLabel);

  float otherHealthPercent = ctx.otherPlayer ? (ctx.otherPlayer->getHealth() / 100.f) : 0.f;
  UIHelper::drawHealthBar(ctx.window, barX + 50.f, barY + 30.f, barWidth, barHeight,
                          otherHealthPercent, sf::Color::Cyan);

  // 金币显示
  sf::Text coinsText(ctx.font);
  coinsText.setString("Coins: " + std::to_string(ctx.player ? ctx.player->getCoins() : 0));
  coinsText.setCharacterSize(20);
  coinsText.setFillColor(sf::Color::Yellow);
  coinsText.setPosition({barX, barY + 60.f});
  ctx.window.draw(coinsText);

  // 显示操作提示
  sf::Text controlHint(ctx.font);
  controlHint.setString("WASD: Move | Mouse: Aim | Click: Shoot | R: Activate NPC");
  controlHint.setCharacterSize(14);
  controlHint.setFillColor(sf::Color(150, 150, 150));
  controlHint.setPosition({barX, static_cast<float>(ctx.screenHeight) - 30.f});
  ctx.window.draw(controlHint);
}
