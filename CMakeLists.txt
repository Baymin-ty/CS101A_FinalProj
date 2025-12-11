cmake_minimum_required(VERSION 3.16)
project(CS101AFinalProj CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(sfml
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(sfml)

# 源文件
set(SOURCES
  src/main.cpp
  src/Game.cpp
  src/Tank.cpp
  src/Bullet.cpp
  src/HealthBar.cpp
  src/Enemy.cpp
  src/Maze.cpp
  src/MazeGenerator.cpp
  src/NetworkManager.cpp
)

# 头文件
set(HEADERS
  src/include/Utils.hpp
  src/include/Game.hpp
  src/include/Tank.hpp
  src/include/Bullet.hpp
  src/include/HealthBar.hpp
  src/include/Enemy.hpp
  src/include/Maze.hpp
  src/include/MazeGenerator.hpp
  src/include/NetworkManager.hpp
)

add_executable(CS101AFinalProj ${SOURCES} ${HEADERS})
target_compile_features(CS101AFinalProj PRIVATE cxx_std_20)
target_include_directories(CS101AFinalProj PRIVATE 
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
)
target_link_libraries(CS101AFinalProj PRIVATE SFML::Graphics SFML::Network)
