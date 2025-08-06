#ifndef OSD_LYRICS_H
#define OSD_LYRICS_H

#include <gtk/gtk.h>

// 公共API函数声明

/**
 * 初始化OSD歌词系统
 * @return 成功返回TRUE，失败返回FALSE
 */
gboolean osd_lyrics_init(void);

/**
 * 使用SSE URL初始化OSD歌词系统
 * @param sse_url SSE连接URL，可以为NULL
 * @return 成功返回TRUE，失败返回FALSE
 */
gboolean osd_lyrics_init_with_sse(const gchar *sse_url);

/**
 * 清理OSD歌词系统资源
 */
void osd_lyrics_cleanup(void);

/**
 * 设置歌词文本
 * @param lyrics 歌词文本
 */
void osd_lyrics_set_text(const gchar *lyrics);

/**
 * 获取当前歌词文本
 * @return 当前歌词文本
 */
const gchar* osd_lyrics_get_text(void);

/**
 * 显示/隐藏窗口
 * @param visible 是否显示
 */
void osd_lyrics_set_visible(gboolean visible);

/**
 * 设置透明度
 * @param opacity 透明度值 (0.01 - 0.90)
 */
void osd_lyrics_set_opacity(gdouble opacity);

/**
 * 设置字体大小
 * @param size 字体大小 (12 - 48)
 */
void osd_lyrics_set_font_size(gint size);

/**
 * 设置鼠标穿透
 * @param enabled 是否启用鼠标穿透
 */
void osd_lyrics_set_mouse_through(gboolean enabled);

/**
 * 设置置顶
 * @param enabled 是否置顶
 */
void osd_lyrics_set_always_on_top(gboolean enabled);

/**
 * 设置文字颜色
 * @param color RGBA颜色值
 */
void osd_lyrics_set_text_color(const GdkRGBA *color);

/**
 * 获取当前文字颜色
 * @param color 输出的RGBA颜色值
 */
void osd_lyrics_get_text_color(GdkRGBA *color);

#endif // OSD_LYRICS_H
