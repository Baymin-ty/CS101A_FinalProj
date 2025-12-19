# ==============================================================================
# Tank Maze Game - CMake 构建配置
# ==============================================================================
cmake_minimum_required(VERSION 3.16)
project(CS101AFinalProj VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 基础配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置默认构建类型为 Release（如果未指定）
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------------------
# RPATH 配置（让可执行文件能找到动态库）
# ------------------------------------------------------------------------------
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# ------------------------------------------------------------------------------
# 第三方依赖：SFML 3.0.2
# ------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(sfml
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG        3.0.2
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(sfml)

# ------------------------------------------------------------------------------
# 源文件和头文件
# ------------------------------------------------------------------------------
set(SOURCES
  # Core
  src/core/main.cpp
  src/core/Game.cpp
  # Entities
  src/entities/Tank.cpp
  src/entities/Bullet.cpp
  src/entities/HealthBar.cpp
  src/entities/Enemy.cpp
  # World
  src/world/Maze.cpp
  src/world/MazeGenerator.cpp
  # Systems
  src/systems/CollisionSystem.cpp
  src/systems/AudioManager.cpp
  # Network
  src/network/NetworkManager.cpp
  src/network/MultiplayerHandler.cpp
)

set(HEADERS
  # Core
  src/include/core/Game.hpp
  # Entities
  src/include/entities/Tank.hpp
  src/include/entities/Bullet.hpp
  src/include/entities/HealthBar.hpp
  src/include/entities/Enemy.hpp
  # World
  src/include/world/Maze.hpp
  src/include/world/MazeGenerator.hpp
  # Systems
  src/include/systems/CollisionSystem.hpp
  src/include/systems/AudioManager.hpp
  # Network
  src/include/network/NetworkManager.hpp
  src/include/network/MultiplayerHandler.hpp
  # UI
  src/include/ui/UIHelper.hpp
  src/include/ui/RoundedRectangle.hpp
  # Utils
  src/include/utils/Utils.hpp
)

# ------------------------------------------------------------------------------
# 可执行文件配置
# ------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
  ${CMAKE_SOURCE_DIR}/src/include/core
  ${CMAKE_SOURCE_DIR}/src/include/entities
  ${CMAKE_SOURCE_DIR}/src/include/world
  ${CMAKE_SOURCE_DIR}/src/include/systems
  ${CMAKE_SOURCE_DIR}/src/include/network
  ${CMAKE_SOURCE_DIR}/src/include/ui
  ${CMAKE_SOURCE_DIR}/src/include/utils
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  SFML::Graphics
  SFML::Network
  SFML::Audio
)

# macOS: 链接 CoreFoundation 框架（用于获取 bundle 路径）
if(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")
endif()

# ------------------------------------------------------------------------------
# 平台特定配置
# ------------------------------------------------------------------------------
if(APPLE)
  # macOS: 创建 .app 包
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Tank Maze Game"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cs101a.tankmaze"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
  )

  # 复制资源文件到 app bundle
  set(RESOURCE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/tank_assets" "${RESOURCE_DIR}/tank_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/music_assets" "${RESOURCE_DIR}/music_assets"
    COMMENT "Copying resources to app bundle..."
  )

elseif(WIN32)
  # Windows: 复制资源文件
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/tank_assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/tank_assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/music_assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/music_assets"
    COMMENT "Copying resources to output directory..."
  )
endif()

# ------------------------------------------------------------------------------
# CPack 打包配置
# ------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "TankMazeGame")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "CS101A")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tank Maze Game")

include(CPack)
