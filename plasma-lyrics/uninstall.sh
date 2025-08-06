#!/bin/bash

# GoMusic Plasma 歌词插件卸载脚本

set -e

PLUGIN_NAME="org.kde.plasma.gomusic-lyrics"
PLUGIN_DIR="$HOME/.local/share/plasma/plasmoids/$PLUGIN_NAME"

echo "🗑️  GoMusic Plasma 歌词插件卸载程序"
echo "===================================="

# 检查插件是否已安装
if [ ! -d "$PLUGIN_DIR" ]; then
    echo "❌ 插件未安装或已被删除"
    echo "   插件目录不存在: $PLUGIN_DIR"
    exit 1
fi

echo "📁 找到插件安装目录: $PLUGIN_DIR"

# 确认卸载
echo ""
echo "⚠️  即将删除 GoMusic Lyrics 插件"
echo "   这将移除所有插件文件"
read -p "   确定要卸载吗？(y/N): " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "❌ 卸载已取消"
    exit 0
fi

# 删除插件目录
echo "🗑️  删除插件文件..."
rm -rf "$PLUGIN_DIR"

# 验证删除
if [ -d "$PLUGIN_DIR" ]; then
    echo "❌ 插件删除失败"
    echo "   请手动删除目录: $PLUGIN_DIR"
    exit 1
else
    echo "✅ 插件文件删除成功"
fi

# 重启 Plasma Shell
echo "🔄 重启 Plasma Shell..."
if command -v kquitapp5 >/dev/null 2>&1; then
    kquitapp5 plasmashell && sleep 2 && plasmashell &
elif command -v kquitapp6 >/dev/null 2>&1; then
    kquitapp6 plasmashell && sleep 2 && plasmashell &
else
    echo "   手动重启 Plasma Shell..."
    killall plasmashell 2>/dev/null || true
    sleep 2
    nohup plasmashell >/dev/null 2>&1 &
fi

echo ""
echo "🎉 卸载完成！"
echo ""
echo "📝 注意事项："
echo "   - 如果桌面或面板上还有插件实例，请手动移除"
echo "   - 右键点击插件 → 移除"
echo ""
echo "感谢使用 GoMusic Lyrics 插件！"
