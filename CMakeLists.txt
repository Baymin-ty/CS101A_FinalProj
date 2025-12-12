cmake_minimum_required(VERSION 3.16)
project(CS101AFinalProj CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 RPATH（让可执行文件能找到动态库）
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

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

# ============ 打包配置 ============
set(CPACK_PACKAGE_NAME "TankMazeGame")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "CS101A")

if(APPLE)
  # macOS: 创建 .app 包
  set_target_properties(CS101AFinalProj PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Tank Maze Game"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.cs101a.tankmaze"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
  )

  # 复制资源文件到 app bundle
  set(RESOURCE_DIR "$<TARGET_BUNDLE_CONTENT_DIR:CS101AFinalProj>/Resources")
  add_custom_command(TARGET CS101AFinalProj POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tank_assets" "${RESOURCE_DIR}/tank_assets"
  )

elseif(WIN32)
  # Windows: 复制资源和 DLL
  add_custom_command(TARGET CS101AFinalProj POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tank_assets" "$<TARGET_FILE_DIR:CS101AFinalProj>/tank_assets"
  )
endif()

include(CPack)
