# wmPlayer Music 歌词显示系统

本目录包含两种歌词显示实现，都通过 SSE (Server-Sent Events) 从 wmPlayer Music 播放器获取实时歌词。

## 🖥️ OSD 歌词 (osdlyric/)
桌面悬浮歌词显示，使用 GTK 实现的独立应用程序。

### 特性
- 🎭 桌面悬浮显示，支持所有桌面环境
- 🎨 透明度调节 (0.01-0.90)
- 🔤 字体大小调节 (12-48px)
- 🌈 文字颜色自定义
- 🔒 窗口锁定/解锁 (鼠标穿透)
- 🖱️ 拖拽移动和调整大小
- 📌 置顶显示
- 🎤 支持 KRC (卡拉OK) 和 LRC (标准) 格式歌词
- 📡 通过 SSE 实时获取歌词
- 🔄 自动重连机制
- 💾 配置自动保存

### 编译和运行
```bash
cd osdlyric
make
./osd_lyrics --sse-url http://127.0.0.1:18911/api/osd-lyrics/sse
```

### 依赖
- GTK 3.0+
- libcurl
- json-c

## 🎨 Plasma 歌词 (plasma-lyrics/)
KDE Plasma 桌面插件，完美集成到 Plasma 桌面环境。

### 特性
- 🖥️ 集成到 KDE Plasma 桌面/面板
- 🎨 自适应 Plasma 主题
- 📡 连接状态指示器
- 🎤 **卡拉OK效果** - KRC格式逐字高亮动画
- 📡 通过 SSE 实时获取歌词
- 🔄 自动重连机制 (3秒间隔)
- 💓 心跳检测 (30秒间隔)
- 📱 自适应布局
- 🛠️ 简单安装/卸载
- ✨ 平滑动画效果 (200ms过渡)
- 🎯 精确时间同步 (50ms更新)

### 安装
```bash
cd plasma-lyrics
./install.sh
```

### 卸载
```bash
cd plasma-lyrics
./uninstall.sh
```

### 测试
```bash
cd plasma-lyrics
./test.sh
```

### 要求
- KDE Plasma 6.0+
- Qt 5.15+ / Qt 6.0+

## 🔗 共同特性

### SSE 连接
- **端点**: `http://127.0.0.1:18911/api/osd-lyrics/sse`
- **格式**: Server-Sent Events
- **自动重连**: 连接断开时自动重试
- **心跳检测**: 定期检查连接状态

### 歌词格式支持
- **LRC 格式**: `[02:51.96]你走之后我又 再为谁等候`
- **KRC 格式**: `[171960,5040]<0,240,0>你<240,150,0>走<390,240,0>之<630,240,0>后`

### 事件类型
- `connected`: 连接建立
- `lyrics_update`: 歌词更新
- `heartbeat`: 心跳检测

## 🚀 快速开始

1. **启动 wmPlayer Music 播放器**
   确保播放器运行在 `http://127.0.0.1:18911`

2. **选择歌词显示方式**
   - **桌面悬浮**: 使用 OSD 歌词
   - **Plasma 集成**: 使用 Plasma 歌词插件

3. **安装和运行**
   按照对应目录的说明进行安装

## 🔧 故障排除

### 歌词不显示
1. 检查 wmPlayer Music 播放器是否运行
2. 测试 SSE 连接：
   ```bash
   curl http://127.0.0.1:18911/api/osd-lyrics/sse
   ```
3. 检查防火墙设置
4. 查看应用程序日志

### 连接问题
1. 确认端口 18911 未被占用
2. 重启 wmPlayer Music 播放器
3. 检查网络连接

## 📝 开发信息

- **协议**: SSE (Server-Sent Events)
- **数据格式**: JSON
- **许可证**: GPL-3.0
- **语言**: C (OSD), QML/JavaScript (Plasma)
