# OSD Lyrics - 桌面歌词显示程序

一个使用C语言和GTK3开发的桌面歌词显示程序，支持透明度调节、字体大小调节、歌词置顶、鼠标穿透等功能。

## 功能特性

- ✅ **透明度调节**: 支持0.01-0.90范围内的透明度调节
- ✅ **字体大小调节**: 支持12-48像素的字体大小调节
- ✅ **文字颜色选择**: 可以自定义歌词文字颜色，默认红色
- ✅ **歌词置顶**: 可以设置窗口始终置顶显示
- ✅ **鼠标穿透**: 启用后鼠标点击会穿透歌词窗口
- ✅ **简洁关闭按钮**: 无背景的✕按钮，悬停时变红
- ✅ **长按拖动**: 长按窗口可以拖动到任意位置
- ✅ **双击设置**: 双击窗口显示/隐藏设置面板
- ✅ **美观界面**: 半透明白色背景，圆角设计，文字始终清晰

## 系统要求

- Linux系统
- GTK+ 3.0 开发库
- GCC编译器
- pkg-config

## 安装依赖

### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install libgtk-3-dev build-essential pkg-config
```

### CentOS/RHEL/Fedora:
```bash
# CentOS/RHEL
sudo yum install gtk3-devel gcc make pkg-config

# Fedora
sudo dnf install gtk3-devel gcc make pkg-config
```

### Arch Linux:
```bash
sudo pacman -S gtk3 gcc make pkg-config
```

## 编译和运行

1. **检查依赖**:
   ```bash
   make check-deps
   ```

2. **编译程序**:
   ```bash
   make
   ```

3. **运行程序**:
   ```bash
   make run
   # 或者直接运行
   ./osd_lyrics
   ```

4. **安装到系统**:
   ```bash
   make install
   ```

5. **卸载**:
   ```bash
   make uninstall
   ```

## 使用方法

### 基本操作
- **双击窗口**: 显示/隐藏设置面板
- **长按拖动**: 按住鼠标左键拖动窗口位置
- **调节透明度**: 使用设置面板中的透明度滑块
- **调节字体**: 使用设置面板中的字体大小滑块
- **鼠标穿透**: 勾选"鼠标穿透"复选框
- **置顶显示**: 勾选"置顶"复选框
- **关闭程序**: 点击"关闭"按钮

### API接口

程序提供了以下API接口供其他程序调用：

```c
// 设置歌词文本
void osd_lyrics_set_text(const gchar *lyrics);

// 获取当前歌词文本
const gchar* osd_lyrics_get_text(void);

// 显示/隐藏窗口
void osd_lyrics_set_visible(gboolean visible);

// 设置透明度 (0.01 - 0.90)
void osd_lyrics_set_opacity(gdouble opacity);

// 设置字体大小 (12 - 48)
void osd_lyrics_set_font_size(gint size);

// 设置鼠标穿透
void osd_lyrics_set_mouse_through(gboolean enabled);

// 设置置顶
void osd_lyrics_set_always_on_top(gboolean enabled);
```

## 开发

### 调试版本
```bash
make debug
```

### 清理编译文件
```bash
make clean
```

### 代码结构
- `osd_lyrics.c` - 主程序文件
- `osd_lyrics.h` - 头文件，包含API声明
- `Makefile` - 编译配置文件
- `README.md` - 说明文档

## 故障排除

1. **编译错误**: 确保已安装GTK3开发库
2. **运行时错误**: 检查是否有图形界面环境
3. **权限问题**: 安装时可能需要sudo权限

## 许可证

本项目采用MIT许可证。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。
