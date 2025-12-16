#include "Game.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
  // 检查是否是训练模式
  bool trainingMode = false;
  if (argc > 1) {
    std::string arg = argv[1];
    if (arg == "--train" || arg == "-t") {
      trainingMode = true;
      std::cout << "==================================" << std::endl;
      std::cout << "  AI Training Mode Activated" << std::endl;
      std::cout << "==================================" << std::endl;
    }
  }
  
  Game game;

  if (!game.init(trainingMode))
  {
    return -1;
  }

  game.run();
  return 0;
}
