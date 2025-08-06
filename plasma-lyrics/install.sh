#!/bin/bash

# GoMusic Plasma 歌词插件安装脚本

set -e

PLUGIN_NAME="org.kde.plasma.gomusic-lyrics"
PLUGIN_DIR="$HOME/.local/share/plasma/plasmoids/$PLUGIN_NAME"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "🎵 GoMusic Plasma 歌词插件安装程序"
echo "=================================="

# 检查是否为 KDE 环境
if [ -z "$KDE_SESSION_VERSION" ] && [ -z "$PLASMA_DESKTOP_SESSION" ]; then
    echo "⚠️  警告：未检测到 KDE Plasma 环境"
    echo "   此插件专为 KDE Plasma 设计"
    read -p "   是否继续安装？(y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ 安装已取消"
        exit 1
    fi
fi

# 检查 Plasma 版本
if command -v plasmashell >/dev/null 2>&1; then
    PLASMA_VERSION=$(plasmashell --version | grep -oP '\d+\.\d+' | head -1)
    echo "📋 检测到 Plasma 版本: $PLASMA_VERSION"
    
    # 检查版本兼容性
    if [ "$(echo "$PLASMA_VERSION < 6.0" | bc -l 2>/dev/null || echo 1)" -eq 1 ]; then
        echo "⚠️  警告：此插件需要 Plasma 6.0 或更高版本"
        echo "   当前版本: $PLASMA_VERSION"
        read -p "   是否继续安装？(y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "❌ 安装已取消"
            exit 1
        fi
    fi
else
    echo "⚠️  警告：无法检测 Plasma 版本"
fi

# 创建插件目录
echo "📁 创建插件目录..."
mkdir -p "$PLUGIN_DIR"

# 复制插件文件
echo "📋 复制插件文件..."
cp -r "$SCRIPT_DIR/contents" "$PLUGIN_DIR/"
cp "$SCRIPT_DIR/metadata.json" "$PLUGIN_DIR/"

# 设置权限
echo "🔐 设置文件权限..."
find "$PLUGIN_DIR" -type f -name "*.qml" -exec chmod 644 {} \;
find "$PLUGIN_DIR" -type f -name "*.json" -exec chmod 644 {} \;

# 验证安装
if [ -f "$PLUGIN_DIR/metadata.json" ] && [ -f "$PLUGIN_DIR/contents/ui/main.qml" ]; then
    echo "✅ 插件文件安装成功"
else
    echo "❌ 插件文件安装失败"
    exit 1
fi

# 重启 Plasma Shell
# echo "🔄 重启 Plasma Shell..."
# if command -v kquitapp5 >/dev/null 2>&1; then
#     kquitapp5 plasmashell && sleep 2 && plasmashell &
# elif command -v kquitapp6 >/dev/null 2>&1; then
#     kquitapp6 plasmashell && sleep 2 && plasmashell &
# else
#     echo "   手动重启 Plasma Shell..."
#     killall plasmashell 2>/dev/null || true
#     sleep 2
#     nohup plasmashell >/dev/null 2>&1 &
# fi

echo ""
echo "🎉 安装完成！"
echo ""
echo "📝 使用方法："
echo "   1. 右键点击桌面或面板"
echo "   2. 选择 '添加部件'"
echo "   3. 搜索 'GoMusic Lyrics'"
echo "   4. 将插件拖拽到桌面或面板上"
echo ""
echo "🔧 配置要求："
echo "   - GoMusic 播放器运行在 http://127.0.0.1:18911"
echo "   - SSE 服务端点：http://127.0.0.1:18911/api/osd-lyrics/sse"
echo ""
echo "📖 更多信息请查看 README.md"

# 检查 GoMusic 服务是否运行
echo ""
echo "🔍 检查 GoMusic 服务状态..."
if curl -s --connect-timeout 3 http://127.0.0.1:18911/api/osd-lyrics/sse >/dev/null 2>&1; then
    echo "✅ GoMusic 歌词服务正在运行"
else
    echo "⚠️  GoMusic 歌词服务未运行或不可访问"
    echo "   请确保 GoMusic 播放器正在运行"
fi

echo ""
echo "安装完成！享受音乐和歌词吧！🎵"
