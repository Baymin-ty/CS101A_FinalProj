#!/bin/bash
# 创建 macOS .app 应用程序包

set -e

APP_NAME="TankMazeGame"
BUNDLE_NAME="CS101AFinalProj"

echo "=== 创建 $APP_NAME.app ==="

# 首先编译 Release 版本
echo ">>> 编译 Release 版本..."
mkdir -p Build_Release
cd Build_Release
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j
cd ..

# CMake 已经创建了 app bundle，我们需要修改它
SRC_APP="Build_Release/$BUNDLE_NAME.app"
DEST_APP="$APP_NAME.app"

if [ ! -d "$SRC_APP" ]; then
    echo "错误: 找不到 $SRC_APP"
    exit 1
fi

# 复制并重命名 app
echo ">>> 创建应用程序包..."
rm -rf "$DEST_APP"
cp -r "$SRC_APP" "$DEST_APP"

CONTENTS_DIR="$DEST_APP/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
RESOURCES_DIR="$CONTENTS_DIR/Resources"
FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"

# 确保目录存在
mkdir -p "$RESOURCES_DIR"
mkdir -p "$FRAMEWORKS_DIR"

# 复制资源文件
echo ">>> 复制资源文件..."
cp -r tank_assets "$RESOURCES_DIR/"

# 创建 Info.plist
echo ">>> 创建 Info.plist..."
cat > "$CONTENTS_DIR/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Tank Maze Game</string>
    <key>CFBundleDisplayName</key>
    <string>Tank Maze Game</string>
    <key>CFBundleIdentifier</key>
    <string>com.cs101a.tankmaze</string>
    <key>CFBundleVersion</key>
    <string>1.0.0</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>TankMazeGame</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

# 重命名可执行文件并创建启动脚本
echo ">>> 创建启动脚本..."
mv "$MACOS_DIR/$BUNDLE_NAME" "$MACOS_DIR/${APP_NAME}_bin"

cat > "$MACOS_DIR/$APP_NAME" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")/../Resources"
exec "$(dirname "$0")/TankMazeGame_bin" "$@"
EOF
chmod +x "$MACOS_DIR/$APP_NAME"

echo ""
echo "=== 打包完成! ==="
echo ""
echo "应用程序已创建: $DEST_APP"
echo ""
echo "你可以:"
echo "  1. 双击 $DEST_APP 直接运行游戏"
echo "  2. 将 $DEST_APP 拖到 Applications 文件夹安装"
echo "  3. 右键压缩后发送给其他 Mac 用户"
echo ""

# 打开 Finder 显示 app
open -R "$DEST_APP"
