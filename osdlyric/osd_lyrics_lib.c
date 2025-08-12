#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *close_button;
    GtkWidget *settings_box;
    GtkWidget *opacity_increase_btn;
    GtkWidget *opacity_decrease_btn;
    GtkWidget *lock_button;
    GtkWidget *always_on_top_toggle;
    GtkWidget *color_button;

    gboolean dragging;
    gint drag_start_x;
    gint drag_start_y;
    gint window_start_x;
    gint window_start_y;
    
    // 窗口调整大小相关
    gboolean resizing;
    gint resize_start_x;
    gint resize_start_y;
    gint resize_start_width;
    gint resize_start_height;
    gint window_width;
    gint window_height;
    
    gboolean settings_visible;
    gboolean is_locked;
    guint hide_timer_id;  // 自动隐藏定时器ID
    gboolean mouse_in_window;  // 鼠标是否在窗口内
    guint unlock_timer_id;  // 解锁显示定时器ID
    gboolean showing_unlock_icon;  // 是否正在显示解锁图标

    gchar *current_lyrics;
    gdouble opacity;
    gint font_size;
    GdkRGBA text_color;  // 文字颜色
    gchar *sse_url;      // SSE连接URL
    gboolean initialized;
} OSDLyrics;

static OSDLyrics *osd = NULL;

// KRC渐进式播放状态
static struct {
    gchar *current_krc_line;
    gint64 line_start_time;
    guint timer_id;
    gboolean is_active;
} krc_progress_state = {NULL, 0, 0, FALSE};

// 函数声明
static void update_opacity(OSDLyrics *osd);  // 移到前面，因为setup_css需要调用它
static void on_window_realize(GtkWidget *widget);
static void setup_window_properties(OSDLyrics *osd);
static void create_ui(OSDLyrics *osd);
static void setup_css(OSDLyrics *osd);
static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data);
static gboolean on_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data);
static gboolean on_leave_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data);
static gboolean auto_hide_settings(gpointer data);
static gboolean on_window_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static gboolean save_config_delayed(gpointer data);
static void on_opacity_increase_clicked(GtkButton *button, gpointer data);
static void on_opacity_decrease_clicked(GtkButton *button, gpointer data);
static void on_font_increase_clicked(GtkButton *button, gpointer data);
static void on_font_decrease_clicked(GtkButton *button, gpointer data);
static void on_lock_clicked(GtkButton *button, gpointer data);
static void on_always_on_top_toggled(GtkToggleButton *button, gpointer data);
static void on_close_clicked(GtkButton *button, gpointer data);
static void on_color_button_clicked(GtkButton *button, gpointer data);
static void toggle_settings(OSDLyrics *osd);
static void update_font_size(OSDLyrics *osd);
static void update_text_color(OSDLyrics *osd);
static void update_color_button_appearance(OSDLyrics *osd);
static void update_mouse_through(OSDLyrics *osd);
static void start_sse_connection(OSDLyrics *osd);
static size_t sse_write_callback(void *contents, size_t size, size_t nmemb, void *userp);
static gboolean update_krc_lyrics_from_sse(gpointer data);
static void osd_lyrics_start_krc_progressive_display(const char *krc_line);
static gboolean osd_lyrics_update_krc_progress(gpointer data);
static void osd_lyrics_process_krc_line(const char *krc_line);
static void osd_lyrics_process_lrc_line(const char *lrc_line);
static void clear_krc_state(void);
static gpointer sse_connection_thread(gpointer data);
static void save_config(OSDLyrics *osd);
static void load_config(OSDLyrics *osd);
static gchar* get_config_dir(void);
static gchar* get_config_file_path(void);

// 窗口实现时调用，启用覆盖重定向以移除窗口管理器装饰（如阴影）
static void on_window_realize(GtkWidget *widget) {
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    if (gdk_window) {
        gdk_window_set_override_redirect(gdk_window, TRUE);
        printf("🔧 [Window] Override redirect enabled to remove WM decorations.\n");
    }
}

static void setup_window_properties(OSDLyrics *osd) {
    // 设置窗口属性
    gtk_window_set_decorated(GTK_WINDOW(osd->window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_DOCK);
    
    // GNOME兼容性：在GNOME下，accept_focus需要特殊处理
    const gchar *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    if (desktop && (g_str_has_prefix(desktop, "GNOME") || g_str_has_prefix(desktop, "gnome"))) {
        // GNOME环境下，先设置为可接受焦点，然后在显示后再禁用
        gtk_window_set_accept_focus(GTK_WINDOW(osd->window), TRUE);
        printf("🔧 [GNOME] 检测到GNOME环境，使用兼容性设置\n");
    } else {
        gtk_window_set_accept_focus(GTK_WINDOW(osd->window), FALSE);
    }

    // 设置窗口位置和大小
    gtk_window_set_default_size(GTK_WINDOW(osd->window), 800, 100);
    gtk_window_set_position(GTK_WINDOW(osd->window), GTK_WIN_POS_CENTER);
    
    // 强制设置最大高度限制，防止GNOME环境下高度异常
    GdkGeometry geometry;
    geometry.min_width = 300;
    geometry.max_width = 1000;
    geometry.min_height = 80;
    geometry.max_height = 100;
    gtk_window_set_geometry_hints(GTK_WINDOW(osd->window), NULL, &geometry,
                                 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);
    
    // 允许窗口调整大小，但设置大小请求策略
    gtk_window_set_resizable(GTK_WINDOW(osd->window), TRUE);
    
    // 防止窗口自动调整大小 - 移除可能导致崩溃的设置
    // g_object_set(osd->window, "resize-mode", GTK_RESIZE_IMMEDIATE, NULL);

    // 启用RGBA视觉效果，支持透明背景
    GdkScreen *screen = gtk_widget_get_screen(osd->window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual != NULL && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(osd->window, visual);
    }

    // 启用事件
    gtk_widget_add_events(osd->window,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_LEAVE_NOTIFY_MASK |
                         GDK_STRUCTURE_MASK);

    // 连接事件处理器
    g_signal_connect(osd->window, "button-press-event", G_CALLBACK(on_button_press), osd);
    g_signal_connect(osd->window, "button-release-event", G_CALLBACK(on_button_release), osd);
    g_signal_connect(osd->window, "motion-notify-event", G_CALLBACK(on_motion_notify), osd);
    g_signal_connect(osd->window, "enter-notify-event", G_CALLBACK(on_enter_notify), osd);
    g_signal_connect(osd->window, "leave-notify-event", G_CALLBACK(on_leave_notify), osd);
    g_signal_connect(osd->window, "configure-event", G_CALLBACK(on_window_configure), osd);
    g_signal_connect(osd->window, "realize", G_CALLBACK(on_window_realize), NULL);

    // GNOME兼容性：移除窗口特效
    g_object_set(osd->window, "hide-titlebar-when-maximized", TRUE, NULL);
}

static void create_ui(OSDLyrics *osd) {
    // 使用叠加容器，确保歌词位置固定
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(osd->window), overlay);

    // 创建歌词标签容器 - 在去掉控制按钮高度后居中
    GtkWidget *lyrics_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(lyrics_container, GTK_ALIGN_FILL);
    gtk_widget_set_valign(lyrics_container, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(lyrics_container, -1, 50);
    gtk_widget_set_margin_bottom(lyrics_container, 25);  // 向上偏移，排除控制按钮高度
    
    // 创建歌词标签 - 使用窗口全宽度居中显示
    osd->label = gtk_label_new("OSD Lyrics - 鼠标悬停显示控制");
    gtk_label_set_justify(GTK_LABEL(osd->label), GTK_JUSTIFY_CENTER);
    gtk_label_set_xalign(GTK_LABEL(osd->label), 0.5);  // 明确设置水平居中
    gtk_label_set_yalign(GTK_LABEL(osd->label), 0.5);  // 明确设置垂直居中
    gtk_widget_set_halign(osd->label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(osd->label, GTK_ALIGN_CENTER);
    
    // 设置标签为单行显示，超出部分省略
    gtk_label_set_ellipsize(GTK_LABEL(osd->label), PANGO_ELLIPSIZE_START);  // 开头省略
    gtk_label_set_single_line_mode(GTK_LABEL(osd->label), TRUE);  // 单行模式
    
    // 将歌词标签添加到歌词容器
    gtk_box_pack_start(GTK_BOX(lyrics_container), osd->label, TRUE, TRUE, 0);
    
    // 将歌词容器作为主要内容添加到叠加容器
    gtk_container_add(GTK_CONTAINER(overlay), lyrics_container);
    
    // 创建设置面板
    osd->settings_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_left(osd->settings_box, 10);
    gtk_widget_set_margin_right(osd->settings_box, 10);
    gtk_widget_set_margin_bottom(osd->settings_box, 5);
    
    // 透明度控制 - 使用两个小灯泡按钮
    GtkWidget *opacity_label = gtk_label_new("透明度:");
    
    // 减少透明度按钮（更透明）
    osd->opacity_decrease_btn = gtk_button_new_with_label("💡");
    gtk_widget_set_size_request(osd->opacity_decrease_btn, 20, 20);
    gtk_widget_set_tooltip_text(osd->opacity_decrease_btn, "增加透明度（更透明）");
    gtk_widget_set_name(osd->opacity_decrease_btn, "mini-btn");
    g_signal_connect(osd->opacity_decrease_btn, "clicked", G_CALLBACK(on_opacity_decrease_clicked), osd);
    
    // 增加透明度按钮（更不透明）
    osd->opacity_increase_btn = gtk_button_new_with_label("🔆");
    gtk_widget_set_size_request(osd->opacity_increase_btn, 20, 20);
    gtk_widget_set_tooltip_text(osd->opacity_increase_btn, "减少透明度（更不透明）");
    gtk_widget_set_name(osd->opacity_increase_btn, "mini-btn");
    g_signal_connect(osd->opacity_increase_btn, "clicked", G_CALLBACK(on_opacity_increase_clicked), osd);
    
    // 字体大小控制按钮
    GtkWidget *font_decrease_btn = gtk_button_new_with_label("🔤");
    gtk_widget_set_size_request(font_decrease_btn, 20, 20);
    gtk_widget_set_tooltip_text(font_decrease_btn, "减小字体");
    gtk_widget_set_name(font_decrease_btn, "mini-btn");
    g_signal_connect(font_decrease_btn, "clicked", G_CALLBACK(on_font_decrease_clicked), osd);
    
    GtkWidget *font_increase_btn = gtk_button_new_with_label("🔠");
    gtk_widget_set_size_request(font_increase_btn, 20, 20);
    gtk_widget_set_tooltip_text(font_increase_btn, "增大字体");
    gtk_widget_set_name(font_increase_btn, "mini-btn");
    g_signal_connect(font_increase_btn, "clicked", G_CALLBACK(on_font_increase_clicked), osd);
    
    // 锁定功能区域
    GtkWidget *lock_label = gtk_label_new("锁定:");
    osd->lock_button = gtk_button_new_with_label("🔓");
    gtk_widget_set_size_request(osd->lock_button, 20, 20);
    gtk_widget_set_tooltip_text(osd->lock_button, "锁定窗口（启用鼠标穿透）");
    gtk_widget_set_name(osd->lock_button, "mini-btn");
    g_signal_connect(osd->lock_button, "clicked", G_CALLBACK(on_lock_clicked), osd);
    
    // 置顶开关
    osd->always_on_top_toggle = gtk_check_button_new_with_label("置顶");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle), TRUE);
    g_signal_connect(osd->always_on_top_toggle, "toggled", G_CALLBACK(on_always_on_top_toggled), osd);
    
    // 文字颜色选择按钮 - 使用自定义按钮和颜色对话框
    GdkRGBA default_color = {1.0, 0.0, 0.0, 1.0}; // 红色

    // 创建一个普通按钮作为颜色选择器
    osd->color_button = gtk_button_new();
    gtk_widget_set_tooltip_text(osd->color_button, "选择歌词文字颜色");
    gtk_widget_set_size_request(osd->color_button, 16, 16);  // 紧凑尺寸，无内边距
    gtk_widget_set_name(osd->color_button, "color-button");  // 设置CSS名称
    gtk_widget_set_halign(osd->color_button, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(osd->color_button, GTK_ALIGN_CENTER);

    // 设置按钮的背景颜色为红色
    GtkCssProvider *color_provider = gtk_css_provider_new();
    gchar *color_css = g_strdup_printf(
        "button#color-button {"
        "  background-color: rgb(255, 0, 0);"
        "  border: 1px solid rgba(150, 150, 150, 0.6);"
        "  border-radius: 2px;"
        "  min-width: 16px;"
        "  min-height: 16px;"
        "  max-width: 16px;"
        "  max-height: 16px;"
        "  padding: 0px;"
        "  margin: 2px;"
        "  box-shadow: none;"
        "}");
    gtk_css_provider_load_from_data(color_provider, color_css, -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(osd->color_button),
        GTK_STYLE_PROVIDER(color_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(color_provider);
    g_free(color_css);

    g_signal_connect(osd->color_button, "clicked", G_CALLBACK(on_color_button_clicked), osd);

    // 关闭按钮 - 使用Unicode符号，无背景
    osd->close_button = gtk_button_new_with_label("✕");
    gtk_widget_set_size_request(osd->close_button, 30, 30);
    gtk_widget_set_tooltip_text(osd->close_button, "关闭");
    gtk_widget_set_name(osd->close_button, "close-button");  // 设置CSS名称
    g_signal_connect(osd->close_button, "clicked", G_CALLBACK(on_close_clicked), osd);
    
    // 创建颜色和字体控制容器
    GtkWidget *color_font_label = gtk_label_new("样式:");
    GtkWidget *color_font_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_halign(color_font_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(color_font_container, GTK_ALIGN_CENTER);
    
    // 添加颜色按钮
    gtk_box_pack_start(GTK_BOX(color_font_container), osd->color_button, FALSE, FALSE, 0);
    // 添加字体大小按钮
    gtk_box_pack_start(GTK_BOX(color_font_container), font_decrease_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(color_font_container), font_increase_btn, FALSE, FALSE, 0);

    // 创建透明度按钮容器
    GtkWidget *opacity_btn_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start(GTK_BOX(opacity_btn_container), osd->opacity_decrease_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(opacity_btn_container), osd->opacity_increase_btn, FALSE, FALSE, 0);
    
    // 添加控件到设置面板
    gtk_box_pack_start(GTK_BOX(osd->settings_box), opacity_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), opacity_btn_container, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), color_font_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), color_font_container, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), lock_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), osd->lock_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), osd->always_on_top_toggle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(osd->settings_box), osd->close_button, FALSE, FALSE, 0);

    // 将控制面板作为叠加层添加到底部
    gtk_widget_set_size_request(osd->settings_box, -1, 40);  // 高度固定，宽度自适应
    gtk_widget_set_halign(osd->settings_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(osd->settings_box, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(osd->settings_box, 5);
    
    // 将设置面板作为叠加层添加，不影响主要内容布局
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), osd->settings_box);

    // 初始隐藏设置面板
    gtk_widget_hide(osd->settings_box);
    osd->settings_visible = FALSE;
}

static void setup_css(OSDLyrics *osd) {
    // 初始CSS设置，透明度将通过update_opacity动态更新
    update_opacity(osd);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    // 如果处于锁定状态，不处理拖拽等操作事件，但允许悬停检测
    if (osd->is_locked && event->type != GDK_2BUTTON_PRESS) {
        return FALSE;
    }

    if (event->button == 1) { // 左键
        if (event->type == GDK_2BUTTON_PRESS) {
            // 双击切换设置面板（保留作为备用）
            toggle_settings(osd);
            return TRUE;
        } else {
            // 获取当前窗口大小
            gtk_window_get_size(GTK_WINDOW(osd->window), &osd->window_width, &osd->window_height);
            
            // 检查鼠标位置，确定调整模式
            gboolean at_right_edge = (event->x >= osd->window_width - 10);
            gboolean at_bottom_edge = (event->y >= osd->window_height - 10);
            gboolean at_corner = at_right_edge && at_bottom_edge;
            
            if (at_corner) {
                // 右下角：同时调整宽度和高度
                osd->resizing = TRUE;
                osd->resize_start_x = event->x_root;
                osd->resize_start_width = osd->window_width;
                osd->resize_start_y = event->y_root;
                osd->resize_start_height = osd->window_height;
                printf("🔧 [窗口调整] 开始调整大小，当前: %dx%d\n", osd->window_width, osd->window_height);
                return TRUE;
            } else if (at_right_edge) {
                // 右边缘：只调整宽度
                osd->resizing = TRUE;
                osd->resize_start_x = event->x_root;
                osd->resize_start_width = osd->window_width;
                printf("🔧 [窗口调整] 开始调整宽度，当前宽度: %d\n", osd->window_width);
                return TRUE;
            } else if (at_bottom_edge) {
                // 底边缘：只调整高度
                osd->resizing = TRUE;
                osd->resize_start_y = event->y_root;
                osd->resize_start_height = osd->window_height;
                printf("🔧 [窗口调整] 开始调整高度，当前高度: %d\n", osd->window_height);
                return TRUE;
            } else {
                // 检测桌面环境，选择合适的拖拽方式
                const gchar *desktop = g_getenv("XDG_CURRENT_DESKTOP");
                if (desktop && (g_str_has_prefix(desktop, "GNOME") || g_str_has_prefix(desktop, "gnome"))) {
                    // GNOME环境：使用GTK原生拖拽API
                    printf("🖱️ [窗口拖拽] GNOME环境，使用GTK原生拖拽\n");
                    gtk_window_begin_move_drag(GTK_WINDOW(osd->window),
                                             event->button,
                                             event->x_root,
                                             event->y_root,
                                             event->time);
                } else {
                    // KDE等其他环境：使用自定义拖拽
                    printf("🖱️ [窗口拖拽] KDE环境，使用自定义拖拽\n");
                    osd->dragging = TRUE;
                    osd->drag_start_x = event->x_root;
                    osd->drag_start_y = event->y_root;
                    gtk_window_get_position(GTK_WINDOW(osd->window),
                                          &osd->window_start_x, &osd->window_start_y);
                }
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    if (event->button == 1) {
        if (osd->resizing) {
            osd->resizing = FALSE;
            // 重置调整起始点
            osd->resize_start_x = 0;
            osd->resize_start_y = 0;
            printf("🔧 [窗口调整] 完成大小调整，最终大小: %dx%d\n", osd->window_width, osd->window_height);
            // 保存配置
            save_config(osd);
        }
        osd->dragging = FALSE;
        return TRUE;
    }
    return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    // 如果处于锁定状态，不处理拖拽，但允许光标更新
    if (osd->is_locked && !osd->showing_unlock_icon) {
        return FALSE;
    }

    // 更新鼠标光标样式（锁定状态下也允许）
    if (!osd->dragging && !osd->resizing && !osd->is_locked) {
        gtk_window_get_size(GTK_WINDOW(osd->window), &osd->window_width, &osd->window_height);
        gboolean at_right_edge = (event->x >= osd->window_width - 10);
        gboolean at_bottom_edge = (event->y >= osd->window_height - 10);
        gboolean at_corner = at_right_edge && at_bottom_edge;
        
        GdkWindow *gdk_window = gtk_widget_get_window(widget);
        if (gdk_window) {
            GdkDisplay *display = gdk_window_get_display(gdk_window);
            GdkCursor *cursor = NULL;
            
            if (at_corner) {
                // 右下角：双向调整光标
                cursor = gdk_cursor_new_for_display(display, GDK_BOTTOM_RIGHT_CORNER);
            } else if (at_right_edge) {
                // 右边缘：水平调整光标
                cursor = gdk_cursor_new_for_display(display, GDK_SB_H_DOUBLE_ARROW);
            } else if (at_bottom_edge) {
                // 底边缘：垂直调整光标
                cursor = gdk_cursor_new_for_display(display, GDK_SB_V_DOUBLE_ARROW);
            }
            
            gdk_window_set_cursor(gdk_window, cursor);
            if (cursor) {
                g_object_unref(cursor);
            }
        }
    }

    if (osd->resizing) {
        gboolean width_changed = FALSE;
        gboolean height_changed = FALSE;
        
        // 检查是否需要调整宽度
        if (osd->resize_start_x > 0) {
            gint width_delta = event->x_root - osd->resize_start_x;
            gint new_width = osd->resize_start_width + width_delta;
            new_width = CLAMP(new_width, 300, 1000);
            
            if (new_width != osd->window_width) {
                osd->window_width = new_width;
                width_changed = TRUE;
            }
        }
        
        // 检查是否需要调整高度
        if (osd->resize_start_y > 0) {
            gint height_delta = event->y_root - osd->resize_start_y;
            gint new_height = osd->resize_start_height + height_delta;
            new_height = CLAMP(new_height, 80, 100);
            
            if (new_height != osd->window_height) {
                osd->window_height = new_height;
                height_changed = TRUE;
            }
        }
        
        // 如果有变化，更新窗口大小
        if (width_changed || height_changed) {
            gtk_window_resize(GTK_WINDOW(osd->window), osd->window_width, osd->window_height);
            printf("🔧 [窗口调整] 调整大小到: %dx%d\n", osd->window_width, osd->window_height);
        }
        return TRUE;
    } else if (osd->dragging) {
        // 自定义拖拽处理（主要用于KDE环境）
        gint new_x = osd->window_start_x + (event->x_root - osd->drag_start_x);
        gint new_y = osd->window_start_y + (event->y_root - osd->drag_start_y);
        gtk_window_move(GTK_WINDOW(osd->window), new_x, new_y);
        return TRUE;
    }
    return FALSE;
}

// 显示解锁图标
static gboolean show_unlock_icon(gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    
    if (osd->is_locked && osd->mouse_in_window) {
        printf("🔓 [锁定状态] 显示解锁图标\n");
        gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔓");
        gtk_widget_set_tooltip_text(osd->lock_button, "点击解锁");
        osd->showing_unlock_icon = TRUE;
        
        // 显示设置面板
        if (!osd->settings_visible) {
            gtk_widget_show(osd->settings_box);
            osd->settings_visible = TRUE;
        }
    }
    
    osd->unlock_timer_id = 0;
    return G_SOURCE_REMOVE;
}

// 鼠标进入窗口事件
static gboolean on_enter_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    printf("🖱️ [OSD歌词] 鼠标进入窗口\n");
    osd->mouse_in_window = TRUE;

    // 取消自动隐藏定时器
    if (osd->hide_timer_id > 0) {
        g_source_remove(osd->hide_timer_id);
        osd->hide_timer_id = 0;
    }

    // 如果处于锁定状态，启动定时器显示解锁图标
    if (osd->is_locked) {
        printf("🔒 [锁定状态] 检测到悬停，1秒后显示解锁图标\n");
        if (osd->unlock_timer_id > 0) {
            g_source_remove(osd->unlock_timer_id);
        }
        osd->unlock_timer_id = g_timeout_add(1000, show_unlock_icon, osd);
    } else if (!osd->settings_visible) {
        // 如果没有锁定，显示设置面板
        printf("🖱️ [OSD歌词] 显示控制面板\n");
        gtk_widget_show(osd->settings_box);
        osd->settings_visible = TRUE;
    }

    return FALSE;
}

// 鼠标离开窗口事件
static gboolean on_leave_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    printf("🖱️ [OSD歌词] 鼠标离开窗口\n");
    osd->mouse_in_window = FALSE;

    // 取消解锁图标显示定时器
    if (osd->unlock_timer_id > 0) {
        g_source_remove(osd->unlock_timer_id);
        osd->unlock_timer_id = 0;
        printf("🔒 [锁定状态] 鼠标离开，取消解锁图标显示\n");
    }
    
    // 如果正在显示解锁图标，恢复锁定图标
    if (osd->showing_unlock_icon && osd->is_locked) {
        gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔒");
        gtk_widget_set_tooltip_text(osd->lock_button, "已锁定（鼠标穿透）");
        osd->showing_unlock_icon = FALSE;
        
        // 隐藏设置面板
        if (osd->settings_visible) {
            gtk_widget_hide(osd->settings_box);
            osd->settings_visible = FALSE;
        }
    }

    // 如果设置面板可见且没有锁定，启动自动隐藏定时器
    if (osd->settings_visible && !osd->is_locked) {
        printf("🖱️ [OSD歌词] 启动自动隐藏定时器 (3秒)\n");
        osd->hide_timer_id = g_timeout_add(3000, auto_hide_settings, osd);
    }

    return FALSE;
}

// 自动隐藏设置面板
static gboolean auto_hide_settings(gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    // 如果设置面板可见且没有锁定且没有在窗口内，隐藏设置面板
    if (!osd->mouse_in_window && !osd->is_locked && osd->settings_visible) {
        printf("🖱️ [OSD歌词] 自动隐藏控制面板\n");
        gtk_widget_hide(osd->settings_box);
        osd->settings_visible = FALSE;
    }

    osd->hide_timer_id = 0;
    return G_SOURCE_REMOVE; // 移除定时器
}

// 窗口位置变化事件
static gboolean on_window_configure(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    static gint last_x = -1, last_y = -1;
    OSDLyrics *osd = (OSDLyrics *)data;

    // 只有当位置真正变化时才保存配置
    if (event->x != last_x || event->y != last_y) {
        last_x = event->x;
        last_y = event->y;

        // 使用延迟保存，避免拖动过程中频繁保存
        static guint save_timer_id = 0;
        if (save_timer_id > 0) {
            g_source_remove(save_timer_id);
        }

        save_timer_id = g_timeout_add(500, (GSourceFunc)save_config_delayed, osd);
    }

    return FALSE; // 继续传递事件
}

// 延迟保存配置（避免拖动时频繁保存）
static gboolean save_config_delayed(gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    save_config(osd);
    return G_SOURCE_REMOVE;
}

// 增加不透明度（减少透明度）
static void on_opacity_increase_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    osd->opacity = CLAMP(osd->opacity + 0.05, 0.01, 0.90);
    update_opacity(osd);
    save_config(osd);
    printf("🔆 [透明度] 减少透明度到: %.2f\n", osd->opacity);
}

// 减少不透明度（增加透明度）
static void on_opacity_decrease_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    osd->opacity = CLAMP(osd->opacity - 0.05, 0.01, 0.90);
    update_opacity(osd);
    save_config(osd);
    printf("💡 [透明度] 增加透明度到: %.2f\n", osd->opacity);
}

// 增大字体
static void on_font_increase_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    osd->font_size = CLAMP(osd->font_size + 2, 12, 48);
    update_font_size(osd);
    save_config(osd);
    printf("🔠 [字体] 增大字体到: %d\n", osd->font_size);
}

// 减小字体
static void on_font_decrease_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    osd->font_size = CLAMP(osd->font_size - 2, 12, 48);
    update_font_size(osd);
    save_config(osd);
    printf("🔤 [字体] 减小字体到: %d\n", osd->font_size);
}

static void on_lock_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    
    if (osd->is_locked) {
        // 解锁
        osd->is_locked = FALSE;
        gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔓");
        gtk_widget_set_tooltip_text(osd->lock_button, "锁定窗口（启用鼠标穿透）");
        osd->showing_unlock_icon = FALSE;
        printf("🔓 [锁定状态] 窗口已解锁，禁用鼠标穿透\n");
    } else {
        // 锁定
        osd->is_locked = TRUE;
        gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔒");
        gtk_widget_set_tooltip_text(osd->lock_button, "已锁定（鼠标穿透）");
        printf("🔒 [锁定状态] 窗口已锁定，启用鼠标穿透\n");
        
        // 锁定时隐藏设置面板
        if (osd->settings_visible) {
            gtk_widget_hide(osd->settings_box);
            osd->settings_visible = FALSE;
        }
        // 取消自动隐藏定时器
        if (osd->hide_timer_id > 0) {
            g_source_remove(osd->hide_timer_id);
            osd->hide_timer_id = 0;
        }
    }

    update_mouse_through(osd);
    save_config(osd);
}

static void on_always_on_top_toggled(GtkToggleButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    gboolean keep_above = gtk_toggle_button_get_active(button);
    
    // GNOME兼容性：使用多种方法确保置顶生效
    gtk_window_set_keep_above(GTK_WINDOW(osd->window), keep_above);
    
    if (keep_above) {
        // 在GNOME下，可能需要设置窗口类型提示
        gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_DOCK);
        // 强制重新应用置顶
        gtk_window_present(GTK_WINDOW(osd->window));
        printf("📌 [置顶] 启用置顶显示 (GNOME兼容模式)\n");
    } else {
        // 恢复普通窗口类型
        gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_NORMAL);
        printf("📌 [置顶] 禁用置顶显示\n");
    }
    
    save_config(osd);  // 自动保存配置
}

static void on_close_clicked(GtkButton *button, gpointer data) {
    gtk_main_quit();
}

// 颜色按钮点击回调 - 打开颜色选择对话框
static void on_color_button_clicked(GtkButton *button, gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    // 创建颜色选择对话框
    GtkWidget *dialog = gtk_color_chooser_dialog_new("选择文字颜色", GTK_WINDOW(osd->window));
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), &osd->text_color);

    // 显示对话框
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        // 获取选择的颜色
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &osd->text_color);

        // 更新按钮颜色
        update_color_button_appearance(osd);

        // 更新文字颜色
        update_text_color(osd);

        // 自动保存配置
        save_config(osd);
    }

    gtk_widget_destroy(dialog);
}

static void toggle_settings(OSDLyrics *osd) {
    if (osd->settings_visible) {
        gtk_widget_hide(osd->settings_box);
        osd->settings_visible = FALSE;
        // 不再改变窗口高度，保持用户设置的大小
    } else {
        gtk_widget_show_all(osd->settings_box);
        osd->settings_visible = TRUE;
        // 不再改变窗口高度，保持用户设置的大小
    }
}

// 更新透明度 - 只对背景设置透明度，文字保持不透明
static void update_opacity(OSDLyrics *osd) {
    // 不使用gtk_widget_set_opacity，而是通过CSS只对背景设置透明度
    GtkCssProvider *provider = gtk_css_provider_new();

    // 确保颜色值有效
    gdouble red = osd->text_color.red * 255;
    gdouble green = osd->text_color.green * 255;
    gdouble blue = osd->text_color.blue * 255;

    // 如果颜色值无效，使用默认红色
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) {
        red = 255;
        green = 0;
        blue = 0;
    }

    gchar *css_data = g_strdup_printf(
        "window {"
        "  background-color: rgba(255, 255, 255, %.2f);"  /* 只有背景有透明度 */
        "  border-radius: 1px;"
        "  border: none;"  /* 移除边框 */
        "}"
        "label {"
        "  color: rgba(%.0f, %.0f, %.0f, 1.0);"  /* 使用动态文字颜色 */
        "  font-weight: bold;"
        "  text-shadow: 0px 1px 2px rgba(255, 255, 255, 0.8);"
        "}"
        "scale {"
        "  color: rgb(51, 51, 51);"
        "}"
        "scale trough {"
        "  background-color: rgba(200, 200, 200, 0.8);"
        "}"
        "scale slider {"
        "  background-color: rgb(100, 100, 100);"
        "}"
        "checkbutton {"
        "  color: rgb(51, 51, 51);"
        "}"
        "checkbutton check {"
        "  background-color: rgb(240, 240, 240);"
        "  border: 1px solid rgb(200, 200, 200);"
        "}"
        "button {"
        "  background-color: rgba(240, 240, 240, 0.9);"
        "  color: rgb(51, 51, 51);"
        "  border: 1px solid rgba(200, 200, 200, 0.8);"
        "  border-radius: 50%%;"
        "  padding: 2px;"
        "  font-weight: bold;"
        "}"
        "button:hover {"
        "  background-color: rgba(220, 220, 220, 0.95);"
        "  color: rgb(255, 85, 85);"
        "}"
        "button#close-button {"
        "  background: none !important;"
        "  background-color: transparent !important;"
        "  background-image: none !important;"
        "  border: none !important;"
        "  border-radius: 0 !important;"
        "  box-shadow: none !important;"
        "  color: rgba(100, 100, 100, 0.8);"
        "  font-weight: bold;"
        "  font-size: 16px;"
        "  padding: 2px;"
        "}"
        "button#close-button:hover {"
        "  color: rgb(255, 85, 85);"
        "  background: none !important;"
        "  background-color: transparent !important;"
        "  background-image: none !important;"
        "  border-radius: 0 !important;"
        "  box-shadow: none !important;"
        "}"
        "button#mini-btn {"
        "  background: none;"
        "  border: none;"
        "  color: rgba(100, 100, 100, 0.8);"
        "  font-size: 12px;"
        "  min-width: 20px;"
        "  min-height: 20px;"
        "  max-width: 20px;"
        "  max-height: 20px;"
        "  padding: 0px;"
        "  margin: 1px;"
        "  box-shadow: none;"
        "}"
        "button#mini-btn:hover {"
        "  color: rgba(50, 50, 50, 1.0);"
        "}"
        "tooltip {"
        "  background-color: rgba(255, 255, 255, 0.95);"
        "  color: rgba(50, 50, 50, 1.0);"
        "  border: 1px solid rgba(200, 200, 200, 0.8);"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  font-size: 11px;"
        "  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);"
        "}", osd->opacity, red, green, blue);

    gtk_css_provider_load_from_data(provider, css_data, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                            GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    g_free(css_data);
}

// 更新文字颜色
static void update_text_color(OSDLyrics *osd) {
    // 重新应用CSS以更新文字颜色
    update_opacity(osd);
}

// 更新颜色按钮外观
static void update_color_button_appearance(OSDLyrics *osd) {
    GtkCssProvider *color_provider = gtk_css_provider_new();
    gchar *color_css = g_strdup_printf(
        "button#color-button {"
        "  background-color: rgb(%.0f, %.0f, %.0f);"
        "  border: 1px solid rgba(150, 150, 150, 0.6);"
        "  border-radius: 2px;"
        "  min-width: 16px;"
        "  min-height: 16px;"
        "  max-width: 16px;"
        "  max-height: 16px;"
        "  padding: 0px;"
        "  margin: 2px;"
        "  box-shadow: none;"
        "}"
        "button#color-button:hover {"
        "  border: 1px solid rgba(100, 100, 100, 0.8);"
        "}",
        osd->text_color.red * 255,
        osd->text_color.green * 255,
        osd->text_color.blue * 255);

    gtk_css_provider_load_from_data(color_provider, color_css, -1, NULL);
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(osd->color_button),
        GTK_STYLE_PROVIDER(color_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(color_provider);
    g_free(color_css);
}

static void update_font_size(OSDLyrics *osd) {
    gchar *font_desc = g_strdup_printf("Sans Bold %d", osd->font_size);
    PangoFontDescription *font = pango_font_description_from_string(font_desc);
    gtk_widget_override_font(osd->label, font);
    pango_font_description_free(font);
    g_free(font_desc);
}

static void update_mouse_through(OSDLyrics *osd) {
    GdkWindow *gdk_window = gtk_widget_get_window(osd->window);
    if (gdk_window) {
        if (osd->is_locked) {
            // 锁定状态：创建一个小的可交互区域用于检测悬停
            // 这样可以保持鼠标穿透的同时允许悬停检测
            cairo_region_t *region = cairo_region_create();
            cairo_rectangle_int_t rect = {0, 0, 1, 1}; // 左上角1x1像素的小区域
            cairo_region_union_rectangle(region, &rect);
            gtk_widget_input_shape_combine_region(osd->window, region);
            cairo_region_destroy(region);
        } else {
            // 解锁状态：禁用鼠标穿透
            gtk_widget_input_shape_combine_region(osd->window, NULL);
        }
    }
}

// 使用SSE URL初始化OSD歌词系统
gboolean osd_lyrics_init_with_sse(const gchar *sse_url);

// 初始化OSD歌词系统
gboolean osd_lyrics_init(void) {
    return osd_lyrics_init_with_sse(NULL);
}

// 使用SSE URL初始化OSD歌词系统
gboolean osd_lyrics_init_with_sse(const gchar *sse_url) {
    if (osd && osd->initialized) {
        return TRUE; // 已经初始化
    }

    osd = g_malloc0(sizeof(OSDLyrics));

    // 创建窗口
    osd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(osd->window), "OSD Lyrics");

    // 初始化默认值
    osd->opacity = 0.7;  // 默认透明度调整为0.7
    osd->font_size = 24;
    osd->is_locked = FALSE;
    osd->dragging = FALSE;
    osd->resizing = FALSE;
    osd->window_width = 800;
    osd->window_height = 100;
    osd->settings_visible = FALSE;
    osd->hide_timer_id = 0;
    osd->mouse_in_window = FALSE;
    osd->unlock_timer_id = 0;
    osd->showing_unlock_icon = FALSE;
    osd->current_lyrics = g_strdup("OSD Lyrics - 鼠标悬停显示控制");

    // 设置默认红色文字
    osd->text_color.red = 1.0;    // 红色
    osd->text_color.green = 0.0;  // 绿色
    osd->text_color.blue = 0.0;   // 蓝色
    osd->text_color.alpha = 1.0;  // 不透明

    // 设置SSE URL
    if (sse_url) {
        osd->sse_url = g_strdup(sse_url);
    }

    // 更新颜色按钮外观
    update_color_button_appearance(osd);

    osd->initialized = TRUE;

    // 如果没有SSE URL，设置默认URL
    if (!osd->sse_url) {
        osd->sse_url = g_strdup("http://127.0.0.1:18911/api/osd-lyrics/sse");
    }

    // 启动SSE连接
    start_sse_connection(osd);

    // 设置窗口属性
    setup_window_properties(osd);

    // 创建UI
    create_ui(osd);

    // 设置CSS样式
    setup_css(osd);

    // 应用初始设置
    update_font_size(osd);
    update_opacity(osd);

    // 加载保存的配置
    load_config(osd);

    return TRUE;
}

// 设置歌词文本
void osd_lyrics_set_text(const gchar *lyrics) {
    if (!osd || !osd->label || !osd->initialized || !lyrics) {
        return;
    }

    // 检查是否在主线程中
    if (!g_main_context_is_owner(g_main_context_default())) {
        g_warning("osd_lyrics_set_text 必须在主线程中调用");
        return;
    }

    // 额外检查确保 label 是有效的 GTK Label 对象
    if (!GTK_IS_LABEL(osd->label)) {
        g_warning("osd->label 不是有效的 GTK Label 对象");
        return;
    }

    // 检查 label 是否已经被销毁
    if (!gtk_widget_get_realized(osd->label) && !gtk_widget_get_visible(osd->label)) {
        g_warning("GTK Label 对象可能已被销毁");
        return;
    }

    // 安全地更新文本
    if (osd->current_lyrics) {
        g_free(osd->current_lyrics);
    }
    osd->current_lyrics = g_strdup(lyrics);

    // 使用 try-catch 机制保护 GTK 调用
    gtk_label_set_text(GTK_LABEL(osd->label), lyrics);
}

// 设置带Pango标记的歌词文本（用于渐进式颜色效果）
void osd_lyrics_set_markup_text(const gchar *markup) {
    if (!osd || !osd->label || !osd->initialized || !markup) {
        return;
    }

    // 检查是否在主线程中
    if (!g_main_context_is_owner(g_main_context_default())) {
        g_warning("osd_lyrics_set_markup_text 必须在主线程中调用");
        return;
    }

    // 额外检查确保 label 是有效的 GTK Label 对象
    if (!GTK_IS_LABEL(osd->label)) {
        g_warning("osd->label 不是有效的 GTK Label 对象");
        // 降级为普通文本显示，但需要去除标记
        gchar *plain_text = g_markup_escape_text(markup, -1);
        osd_lyrics_set_text(plain_text);
        g_free(plain_text);
        return;
    }

    // 检查 label 是否已经被销毁
    if (!gtk_widget_get_realized(osd->label) && !gtk_widget_get_visible(osd->label)) {
        g_warning("GTK Label 对象可能已被销毁");
        return;
    }

    // 验证 markup 格式是否有效
    GError *error = NULL;
    if (!pango_parse_markup(markup, -1, 0, NULL, NULL, NULL, &error)) {
        g_warning("无效的 Pango 标记: %s", error ? error->message : "未知错误");
        if (error) g_error_free(error);
        // 降级为普通文本显示
        gchar *plain_text = g_markup_escape_text(markup, -1);
        osd_lyrics_set_text(plain_text);
        g_free(plain_text);
        return;
    }

    // 安全地更新标记文本
    if (osd->current_lyrics) {
        g_free(osd->current_lyrics);
    }
    osd->current_lyrics = g_strdup(markup);

    // 使用 try-catch 机制保护 GTK 调用
    gtk_label_set_markup(GTK_LABEL(osd->label), markup);
}

// 线程安全的文本更新结构
typedef struct {
    gchar *text;
    gboolean is_markup;
} ThreadSafeTextUpdate;

// 主线程中执行文本更新的回调函数
static gboolean update_text_in_main_thread(gpointer data) {
    ThreadSafeTextUpdate *update = (ThreadSafeTextUpdate *)data;

    if (update) {
        if (update->is_markup) {
            osd_lyrics_set_markup_text(update->text);
        } else {
            osd_lyrics_set_text(update->text);
        }

        // 清理资源
        if (update->text) {
            g_free(update->text);
        }
        g_free(update);
    }

    return G_SOURCE_REMOVE; // 只执行一次
}

// 线程安全的文本设置函数
void osd_lyrics_set_text_safe(const gchar *lyrics) {
    if (!lyrics) return;

    if (g_main_context_is_owner(g_main_context_default())) {
        // 已经在主线程中，直接调用
        osd_lyrics_set_text(lyrics);
    } else {
        // 在其他线程中，调度到主线程执行
        ThreadSafeTextUpdate *update = g_malloc0(sizeof(ThreadSafeTextUpdate));
        update->text = g_strdup(lyrics);
        update->is_markup = FALSE;

        gdk_threads_add_idle(update_text_in_main_thread, update);
    }
}

// 线程安全的标记文本设置函数
void osd_lyrics_set_markup_text_safe(const gchar *markup) {
    if (!markup) return;

    if (g_main_context_is_owner(g_main_context_default())) {
        // 已经在主线程中，直接调用
        osd_lyrics_set_markup_text(markup);
    } else {
        // 在其他线程中，调度到主线程执行
        ThreadSafeTextUpdate *update = g_malloc0(sizeof(ThreadSafeTextUpdate));
        update->text = g_strdup(markup);
        update->is_markup = TRUE;

        gdk_threads_add_idle(update_text_in_main_thread, update);
    }
}

// 获取当前歌词文本
const gchar* osd_lyrics_get_text(void) {
    return (osd && osd->initialized) ? osd->current_lyrics : NULL;
}

// 强制检查并修正窗口高度（防止GNOME环境下高度异常）
static gboolean force_check_window_height(gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;
    if (osd && osd->window && osd->initialized) {
        gint current_width, current_height;
        gtk_window_get_size(GTK_WINDOW(osd->window), &current_width, &current_height);
        
        if (current_height > 100) {
            printf("🔧 [GNOME修正] 检测到异常高度 %d，强制修正为100\n", current_height);
            gtk_window_resize(GTK_WINDOW(osd->window), current_width, 100);
            osd->window_height = 100;
        }
        
        // GNOME环境下，延迟禁用焦点接受
        const gchar *desktop = g_getenv("XDG_CURRENT_DESKTOP");
        if (desktop && (g_str_has_prefix(desktop, "GNOME") || g_str_has_prefix(desktop, "gnome"))) {
            gtk_window_set_accept_focus(GTK_WINDOW(osd->window), FALSE);
            printf("🔧 [GNOME修正] 禁用窗口焦点接受\n");
        }
    }
    return G_SOURCE_REMOVE; // 只执行一次
}

// 显示/隐藏窗口
void osd_lyrics_set_visible(gboolean visible) {
    if (osd && osd->window && osd->initialized) {
        if (visible) {
            gtk_widget_show_all(osd->window);
            if (!osd->settings_visible) {
                gtk_widget_hide(osd->settings_box);
            }
            // 延迟检查窗口高度，确保窗口管理器完成布局后再修正
            g_timeout_add(100, force_check_window_height, osd);
        } else {
            gtk_widget_hide(osd->window);
        }
    }
}

// 设置透明度
void osd_lyrics_set_opacity(gdouble opacity) {
    if (osd && osd->initialized) {
        osd->opacity = CLAMP(opacity, 0.01, 0.90);
        update_opacity(osd);
    }
}

// 设置字体大小
void osd_lyrics_set_font_size(gint size) {
    if (osd && osd->initialized) {
        osd->font_size = CLAMP(size, 12, 48);
        update_font_size(osd);
    }
}

// 设置锁定状态
void osd_lyrics_set_mouse_through(gboolean enabled) {
    if (osd && osd->initialized) {
        osd->is_locked = enabled;
        if (enabled) {
            gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔒");
            gtk_widget_set_tooltip_text(osd->lock_button, "已锁定（鼠标穿透）");
        } else {
            gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔓");
            gtk_widget_set_tooltip_text(osd->lock_button, "锁定窗口（启用鼠标穿透）");
        }
        update_mouse_through(osd);
    }
}

// 设置置顶
void osd_lyrics_set_always_on_top(gboolean enabled) {
    if (osd && osd->initialized) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle), enabled);
        gtk_window_set_keep_above(GTK_WINDOW(osd->window), enabled);
        
        // GNOME兼容性设置
        if (enabled) {
            gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_DOCK);
            gtk_window_present(GTK_WINDOW(osd->window));
        } else {
            gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_NORMAL);
        }
    }
}

// 设置文字颜色
void osd_lyrics_set_text_color(const GdkRGBA *color) {
    if (osd && osd->initialized && color) {
        // 复制颜色值
        osd->text_color = *color;

        // 更新颜色按钮外观
        update_color_button_appearance(osd);

        // 更新文字颜色
        update_text_color(osd);
    }
}

// 获取当前文字颜色
void osd_lyrics_get_text_color(GdkRGBA *color) {
    if (osd && osd->initialized && color) {
        *color = osd->text_color;
    }
}

// SSE数据结构
typedef struct {
    OSDLyrics *osd;
    gchar *buffer;
    size_t buffer_size;
} SSEData;

// SSE写入回调函数
static size_t sse_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    SSEData *sse_data = (SSEData *)userp;

    // 检查输入参数有效性
    if (!sse_data || !contents || realsize == 0) {
        return 0;
    }

    // 检查OSD对象是否仍然有效
    if (!sse_data->osd || !sse_data->osd->initialized) {
        printf("⚠️ [SSE回调] OSD对象已失效，停止处理数据\n");
        return 0;
    }

    // 重新分配缓冲区，添加错误检查
    gchar *new_buffer = g_realloc(sse_data->buffer, sse_data->buffer_size + realsize + 1);
    if (new_buffer == NULL) {
        g_warning("SSE缓冲区内存分配失败");
        return 0;
    }
    sse_data->buffer = new_buffer;

    // 复制数据到缓冲区
    memcpy(&(sse_data->buffer[sse_data->buffer_size]), contents, realsize);
    sse_data->buffer_size += realsize;
    sse_data->buffer[sse_data->buffer_size] = 0;

    // 处理SSE数据
    gchar **lines = g_strsplit(sse_data->buffer, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        if (g_str_has_prefix(lines[i], "data: ")) {
            gchar *json_data = lines[i] + 6; // 跳过 "data: "

            // 解析JSON数据
            json_object *root = json_tokener_parse(json_data);
            if (root) {
                json_object *type_obj, *text_obj, *song_obj, *artist_obj;

                if (json_object_object_get_ex(root, "type", &type_obj)) {
                    const char *type = json_object_get_string(type_obj);

                    if (strcmp(type, "lyrics_update") == 0) {
                        const char *text = "";
                        const char *song = "";
                        const char *artist = "";
                        const char *format = "lrc"; // 默认格式

                        if (json_object_object_get_ex(root, "text", &text_obj)) {
                            text = json_object_get_string(text_obj);
                        }
                        if (json_object_object_get_ex(root, "songName", &song_obj)) {
                            song = json_object_get_string(song_obj);
                        }
                        if (json_object_object_get_ex(root, "artist", &artist_obj)) {
                            artist = json_object_get_string(artist_obj);
                        }

                        // 检查歌词格式
                        json_object *format_obj;
                        if (json_object_object_get_ex(root, "format", &format_obj)) {
                            format = json_object_get_string(format_obj);
                        }

                        printf("🎵 [OSD歌词] 收到歌词 (%s): %s - %s\n", format, song, artist);

                        // 根据格式字段正确处理歌词
                        if (strcmp(format, "krc") == 0) {
                            printf("🎤 [OSD歌词] 处理KRC格式歌词\n");
                            gchar *lyrics_data = g_strdup(text);
                            gdk_threads_add_idle(update_krc_lyrics_from_sse, lyrics_data);
                        } else {
                            printf("📝 [OSD歌词] 处理LRC格式歌词\n");
                            // 清理KRC状态
                            clear_krc_state();
                            // 直接处理LRC歌词
                            osd_lyrics_process_lrc_line(text);
                        }
                    } else if (strcmp(type, "connected") == 0) {
                        printf("✅ [OSD歌词] SSE连接成功\n");
                    } else if (strcmp(type, "heartbeat") == 0) {
                        printf("💓 [OSD歌词] 收到心跳\n");
                    }
                }

                json_object_put(root);
            }
        }
    }
    g_strfreev(lines);

    // 清空缓冲区
    sse_data->buffer_size = 0;

    return realsize;
}

// 旧的更新函数已移除，现在统一使用 update_krc_lyrics_from_sse

// 在主线程中更新歌词（处理原始KRC/LRC格式）
static gboolean update_krc_lyrics_from_sse(gpointer data) {
    gchar *lyrics_text = (gchar *)data;
    if (lyrics_text && osd && osd->initialized) {
        printf("📝 [OSD歌词] 处理原始歌词: %s\n", lyrics_text);

        if (strstr(lyrics_text, "[") && strstr(lyrics_text, ",") && strstr(lyrics_text, "]<")) {
            // KRC格式：[171960,5040]<0,240,0>你<240,150,0>走...
            printf("🎤 [OSD歌词] 检测到KRC格式，启动渐进式播放模式\n");
            osd_lyrics_start_krc_progressive_display(lyrics_text);
        } else if (strstr(lyrics_text, "[") && strstr(lyrics_text, ":") && strstr(lyrics_text, "]")) {
            // LRC格式：[02:51.96]你走之后我又 再为谁等候
            printf("📝 [OSD歌词] 检测到LRC格式，提取文本显示\n");
            osd_lyrics_process_lrc_line(lyrics_text);
        } else {
            // 纯文本
            printf("📝 [OSD歌词] 纯文本模式: %s\n", lyrics_text);
            osd_lyrics_set_text_safe(lyrics_text);
        }

        g_free(lyrics_text);
    }
    return G_SOURCE_REMOVE;
}

// 启动SSE连接
static void start_sse_connection(OSDLyrics *osd) {
    if (!osd->sse_url) {
        return;
    }

    // 在新线程中运行SSE连接
    g_thread_new("sse-connection", (GThreadFunc)sse_connection_thread, osd);
}

// SSE连接线程
static gpointer sse_connection_thread(gpointer data) {
    OSDLyrics *osd = (OSDLyrics *)data;

    if (!osd) {
        printf("❌ [SSE线程] OSD对象为空，线程退出\n");
        return NULL;
    }

    printf("🔗 [OSD歌词] 开始SSE连接线程\n");

    while (osd && osd->initialized) {
        CURL *curl;
        CURLcode res;
        SSEData sse_data = {0};

        // 再次检查OSD对象有效性
        if (!osd || !osd->initialized) {
            printf("⚠️ [SSE线程] OSD对象已失效，退出连接循环\n");
            break;
        }

        sse_data.osd = osd;
        sse_data.buffer = NULL;
        sse_data.buffer_size = 0;

        printf("🔗 [OSD歌词] 尝试连接到: %s\n", osd->sse_url);

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, osd->sse_url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sse_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // 无超时
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 连接超时10秒
            // 添加信号处理，允许中断长时间连接
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            // 设置SSE头部
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Accept: text/event-stream");
            headers = curl_slist_append(headers, "Cache-Control: no-cache");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                printf("❌ [OSD歌词] SSE连接失败: %s\n", curl_easy_strerror(res));
            } else {
                printf("🔌 [OSD歌词] SSE连接断开\n");
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        // 安全清理缓冲区
        if (sse_data.buffer) {
            g_free(sse_data.buffer);
            sse_data.buffer = NULL;
        }

        // 检查程序是否还在运行，如果是则等待后重连
        if (osd && osd->initialized) {
            printf("⏰ [OSD歌词] 3秒后重连...\n");
            // 使用更短的睡眠间隔，以便更快响应程序退出
            for (int i = 0; i < 30 && osd && osd->initialized; i++) {
                g_usleep(100 * 1000); // 100ms * 30 = 3秒
            }
        }
    }

    printf("🔴 [OSD歌词] SSE连接线程退出\n");
    return NULL;
}

// 获取配置目录路径
static gchar* get_config_dir(void) {
    const gchar *home_dir = g_get_home_dir();
    if (!home_dir) {
        g_warning("无法获取用户主目录");
        return NULL;
    }

    gchar *config_dir = g_build_filename(home_dir, ".config", "gomusic", NULL);

    // 创建目录（如果不存在）
    if (g_mkdir_with_parents(config_dir, 0755) != 0) {
        g_warning("无法创建配置目录: %s", config_dir);
        g_free(config_dir);
        return NULL;
    }

    return config_dir;
}

// 获取配置文件路径
static gchar* get_config_file_path(void) {
    gchar *config_dir = get_config_dir();
    if (!config_dir) {
        return NULL;
    }

    gchar *config_file = g_build_filename(config_dir, "osd_lyrics.conf", NULL);
    g_free(config_dir);

    return config_file;
}

// 保存配置到文件
static void save_config(OSDLyrics *osd) {
    if (!osd || !osd->initialized) {
        return;
    }

    gchar *config_file = get_config_file_path();
    if (!config_file) {
        return;
    }

    printf("💾 [OSD歌词] 保存配置到: %s\n", config_file);

    // 创建JSON对象
    json_object *config = json_object_new_object();

    // 保存窗口位置和大小
    gint x, y, width, height;
    gtk_window_get_position(GTK_WINDOW(osd->window), &x, &y);
    gtk_window_get_size(GTK_WINDOW(osd->window), &width, &height);
    json_object_object_add(config, "window_x", json_object_new_int(x));
    json_object_object_add(config, "window_y", json_object_new_int(y));
    json_object_object_add(config, "window_width", json_object_new_int(width));
    json_object_object_add(config, "window_height", json_object_new_int(height));

    // 保存透明度
    json_object_object_add(config, "opacity", json_object_new_double(osd->opacity));

    // 保存字体大小
    json_object_object_add(config, "font_size", json_object_new_int(osd->font_size));

    // 保存文字颜色
    json_object_object_add(config, "text_color_red", json_object_new_double(osd->text_color.red));
    json_object_object_add(config, "text_color_green", json_object_new_double(osd->text_color.green));
    json_object_object_add(config, "text_color_blue", json_object_new_double(osd->text_color.blue));

    // 不保存鼠标穿透状态 - 每次启动都重置为默认值

    // 保存置顶状态
    gboolean always_on_top = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle));
    json_object_object_add(config, "always_on_top", json_object_new_boolean(always_on_top));

    // 写入文件
    const char *json_string = json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY);
    if (json_string) {
        FILE *file = fopen(config_file, "w");
        if (file) {
            fprintf(file, "%s", json_string);
            fclose(file);
            printf("✅ [OSD歌词] 配置保存成功\n");
        } else {
            g_warning("无法写入配置文件: %s", config_file);
        }
    }

    json_object_put(config);
    g_free(config_file);
}

// 从文件加载配置
static void load_config(OSDLyrics *osd) {
    if (!osd || !osd->initialized) {
        return;
    }

    gchar *config_file = get_config_file_path();
    if (!config_file) {
        return;
    }

    // 检查文件是否存在
    if (!g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        printf("📄 [OSD歌词] 配置文件不存在，使用默认设置: %s\n", config_file);
        g_free(config_file);
        return;
    }

    printf("📖 [OSD歌词] 加载配置从: %s\n", config_file);

    // 读取文件内容
    gchar *content;
    gsize length;
    GError *error = NULL;

    if (!g_file_get_contents(config_file, &content, &length, &error)) {
        g_warning("无法读取配置文件: %s", error->message);
        g_error_free(error);
        g_free(config_file);
        return;
    }

    // 解析JSON
    json_object *config = json_tokener_parse(content);
    if (!config) {
        g_warning("配置文件JSON格式错误");
        g_free(content);
        g_free(config_file);
        return;
    }

    // 加载窗口位置和大小
    json_object *window_x_obj, *window_y_obj, *window_width_obj, *window_height_obj;
    if (json_object_object_get_ex(config, "window_x", &window_x_obj) &&
        json_object_object_get_ex(config, "window_y", &window_y_obj)) {
        gint x = json_object_get_int(window_x_obj);
        gint y = json_object_get_int(window_y_obj);
        gtk_window_move(GTK_WINDOW(osd->window), x, y);
        printf("📍 [OSD歌词] 恢复窗口位置: (%d, %d)\n", x, y);
    }
    
    // 加载窗口大小
    if (json_object_object_get_ex(config, "window_width", &window_width_obj) &&
        json_object_object_get_ex(config, "window_height", &window_height_obj)) {
        gint width = json_object_get_int(window_width_obj);
        gint height = json_object_get_int(window_height_obj);
        
        // 限制窗口大小范围
        width = CLAMP(width, 300, 1000);
        height = CLAMP(height, 80, 100);
        
        osd->window_width = width;
        osd->window_height = height;
        
        gtk_window_resize(GTK_WINDOW(osd->window), width, height);
        
        printf("📐 [OSD歌词] 恢复窗口大小: %dx%d\n", width, height);
    }

    // 加载透明度
    json_object *opacity_obj;
    if (json_object_object_get_ex(config, "opacity", &opacity_obj)) {
        gdouble opacity = json_object_get_double(opacity_obj);
        osd->opacity = opacity;
        update_opacity(osd);
        printf("🔍 [OSD歌词] 恢复透明度: %.2f\n", opacity);
    }

    // 加载字体大小
    json_object *font_size_obj;
    if (json_object_object_get_ex(config, "font_size", &font_size_obj)) {
        gint font_size = json_object_get_int(font_size_obj);
        osd->font_size = font_size;
        update_font_size(osd);
        printf("🔤 [OSD歌词] 恢复字体大小: %d\n", font_size);
    }

    // 加载文字颜色
    json_object *red_obj, *green_obj, *blue_obj;
    if (json_object_object_get_ex(config, "text_color_red", &red_obj) &&
        json_object_object_get_ex(config, "text_color_green", &green_obj) &&
        json_object_object_get_ex(config, "text_color_blue", &blue_obj)) {
        osd->text_color.red = json_object_get_double(red_obj);
        osd->text_color.green = json_object_get_double(green_obj);
        osd->text_color.blue = json_object_get_double(blue_obj);
        update_color_button_appearance(osd);
        update_text_color(osd);
        printf("🎨 [OSD歌词] 恢复文字颜色: RGB(%.2f, %.2f, %.2f)\n",
               osd->text_color.red, osd->text_color.green, osd->text_color.blue);
    }

    // 不加载锁定状态 - 每次启动都使用默认值（解锁）
    // 确保锁定状态始终为解锁状态
    osd->is_locked = FALSE;
    gtk_button_set_label(GTK_BUTTON(osd->lock_button), "🔓");
    gtk_widget_set_tooltip_text(osd->lock_button, "锁定窗口（启用鼠标穿透）");
    update_mouse_through(osd);
    printf("🔓 [OSD歌词] 锁定状态重置为默认状态: 解锁\n");

    // 加载置顶状态
    json_object *always_on_top_obj;
    if (json_object_object_get_ex(config, "always_on_top", &always_on_top_obj)) {
        gboolean always_on_top = json_object_get_boolean(always_on_top_obj);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle), always_on_top);
        gtk_window_set_keep_above(GTK_WINDOW(osd->window), always_on_top);
        printf("📌 [OSD歌词] 恢复置顶状态: %s\n", always_on_top ? "启用" : "禁用");
    } else {
        // 如果配置文件中没有置顶设置，默认启用置顶
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle), TRUE);
        gtk_window_set_keep_above(GTK_WINDOW(osd->window), TRUE);
        printf("📌 [OSD歌词] 使用默认置顶状态: 启用\n");
    }

    // 强制重新应用窗口属性，确保置顶和无边框生效
    gtk_window_set_decorated(GTK_WINDOW(osd->window), FALSE);
    gboolean always_on_top = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osd->always_on_top_toggle));
    gtk_window_set_keep_above(GTK_WINDOW(osd->window), always_on_top);
    
    // GNOME兼容性：设置窗口类型提示
    if (always_on_top) {
        gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_DOCK);
        // 延迟强制置顶，确保在GNOME下生效
        g_timeout_add(200, (GSourceFunc)gtk_window_present, osd->window);
    } else {
        gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_NORMAL);
    }

    printf("✅ [OSD歌词] 配置加载完成，窗口属性已重新应用\n");

    json_object_put(config);
    g_free(content);
    g_free(config_file);
}

// 清理KRC渐进式播放状态
static void clear_krc_state(void) {
    printf("🔄 [KRC清理] 开始清理KRC状态\n");

    // 先标记为非活动状态，防止定时器回调继续执行
    krc_progress_state.is_active = FALSE;

    // 停止定时器
    if (krc_progress_state.timer_id > 0) {
        if (g_source_remove(krc_progress_state.timer_id)) {
            printf("🔄 [KRC清理] 已停止KRC渐进式播放定时器 (ID: %u)\n", krc_progress_state.timer_id);
        } else {
            printf("⚠️ [KRC清理] 定时器已不存在或已被移除 (ID: %u)\n", krc_progress_state.timer_id);
        }
        krc_progress_state.timer_id = 0;
    }

    // 清理KRC相关的全局状态
    if (krc_progress_state.current_krc_line) {
        g_free(krc_progress_state.current_krc_line);
        krc_progress_state.current_krc_line = NULL;
        printf("🔄 [KRC清理] 已清理当前KRC歌词行\n");
    }

    krc_progress_state.line_start_time = 0;

    printf("✅ [KRC清理] KRC状态清理完成\n");
}

// 启动KRC渐进式播放显示
static void osd_lyrics_start_krc_progressive_display(const char *krc_line) {
    if (!osd || !osd->initialized || !krc_line) return;

    printf("🎤 [KRC渐进] 启动渐进式播放: %s\n", krc_line);

    // 停止之前的定时器
    if (krc_progress_state.timer_id > 0) {
        g_source_remove(krc_progress_state.timer_id);
        krc_progress_state.timer_id = 0;
    }

    // 清理之前的数据
    if (krc_progress_state.current_krc_line) {
        g_free(krc_progress_state.current_krc_line);
    }

    // 保存当前KRC行
    krc_progress_state.current_krc_line = g_strdup(krc_line);
    krc_progress_state.line_start_time = g_get_monotonic_time() / 1000; // 转换为毫秒
    krc_progress_state.is_active = TRUE;

    // 立即显示第一次（全部未播放状态）
    osd_lyrics_update_krc_progress(NULL);

    // 启动定时器，每100ms更新一次
    krc_progress_state.timer_id = g_timeout_add(100, osd_lyrics_update_krc_progress, NULL);

    printf("🎤 [KRC渐进] 定时器已启动，ID: %u\n", krc_progress_state.timer_id);
}

// 处理原始KRC格式歌词行
static void osd_lyrics_process_krc_line(const char *krc_line) {
    if (!osd || !osd->initialized || !krc_line) return;

    printf("🎤 [KRC处理] 原始行: %s\n", krc_line);

    // 提取纯文本内容（移除时间戳标记）
    char *text_content = g_strdup("");
    const char *ptr = krc_line;

    // 跳过行时间戳 [171960,5040]
    if (*ptr == '[') {
        while (*ptr && *ptr != ']') ptr++;
        if (*ptr == ']') ptr++;
    }

    // 解析字符和时间戳
    GString *text_builder = g_string_new("");
    while (*ptr) {
        if (*ptr == '<') {
            // 跳过字符时间戳 <0,240,0>
            while (*ptr && *ptr != '>') ptr++;
            if (*ptr == '>') ptr++;
        } else {
            // 添加字符到文本
            g_string_append_c(text_builder, *ptr);
            ptr++;
        }
    }

    g_free(text_content);
    text_content = g_string_free(text_builder, FALSE);

    printf("🎤 [KRC处理] 提取文本: %s\n", text_content);

    // 显示提取的文本
    osd_lyrics_set_text_safe(text_content);

    g_free(text_content);
}

// KRC进度更新函数（定时器回调）
static gboolean osd_lyrics_update_krc_progress(gpointer data) {
    // 检查全局状态和OSD对象有效性
    if (!krc_progress_state.is_active || !krc_progress_state.current_krc_line || !osd || !osd->initialized) {
        printf("🔄 [KRC进度] 状态无效，停止定时器\n");
        krc_progress_state.timer_id = 0;
        return FALSE; // 停止定时器
    }

    // 计算当前播放进度
    gint64 current_time = g_get_monotonic_time() / 1000; // 转换为毫秒
    gint64 progress_ms = current_time - krc_progress_state.line_start_time;

    // printf("🎤 [KRC渐进] 更新进度: %ldms\n", progress_ms);

    const char *krc_line = krc_progress_state.current_krc_line;
    const char *ptr = krc_line;

    // 跳过行时间戳 [171960,5040]
    if (*ptr == '[') {
        while (*ptr && *ptr != ']') ptr++;
        if (*ptr == ']') ptr++;
    }

    // 构建带Pango标记的渐进式高亮文本
    GString *result_text = g_string_new("");
    long current_char_time = 0;
    gboolean in_played_section = TRUE;
    gboolean color_section_open = FALSE;

    while (*ptr) {
        if (*ptr == '<') {
            // 解析字符时间戳 <0,240,0>
            ptr++; // 跳过 '<'
            const char *time_start = ptr;

            // 找到第一个逗号（字符开始时间）
            while (*ptr && *ptr != ',') ptr++;
            if (*ptr == ',') {
                char *time_str = g_strndup(time_start, ptr - time_start);
                current_char_time = strtol(time_str, NULL, 10);
                g_free(time_str);
            }

            // 跳过到 '>'
            while (*ptr && *ptr != '>') ptr++;
            if (*ptr == '>') ptr++;

            // 检查当前字符是否应该高亮
            gboolean should_be_played = (current_char_time <= progress_ms);

            // 如果播放状态发生变化，需要切换颜色标记
            if (should_be_played != in_played_section) {
                // 关闭之前的颜色标记
                if (color_section_open) {
                    g_string_append(result_text, "</span>");
                    color_section_open = FALSE;
                }

                // 开启新的颜色标记
                if (should_be_played) {
                    // 已播放部分：使用用户选择的颜色
                    gchar *user_color = g_strdup_printf("#%02x%02x%02x", 
                        (int)(osd->text_color.red * 255),
                        (int)(osd->text_color.green * 255),
                        (int)(osd->text_color.blue * 255));
                    g_string_append_printf(result_text, "<span foreground=\"%s\">", user_color);
                    g_free(user_color);
                } else {
                    // 未播放部分：灰色
                    g_string_append(result_text, "<span foreground=\"#666666\">");
                }
                color_section_open = TRUE;
                in_played_section = should_be_played;
            }

            // 处理时间戳后面的字符（如果存在）
            if (*ptr) {
                // 如果还没有开启颜色标记，开启第一个
                if (!color_section_open) {
                    if (in_played_section) {
                        // 已播放部分：使用用户选择的颜色
                        gchar *user_color = g_strdup_printf("#%02x%02x%02x", 
                            (int)(osd->text_color.red * 255),
                            (int)(osd->text_color.green * 255),
                            (int)(osd->text_color.blue * 255));
                        g_string_append_printf(result_text, "<span foreground=\"%s\">", user_color);
                        g_free(user_color);
                    } else {
                        g_string_append(result_text, "<span foreground=\"#666666\">");
                    }
                    color_section_open = TRUE;
                }

                // 添加字符（需要转义特殊字符）
                if (*ptr == '<') {
                    g_string_append(result_text, "&lt;");
                } else if (*ptr == '>') {
                    g_string_append(result_text, "&gt;");
                } else if (*ptr == '&') {
                    g_string_append(result_text, "&amp;");
                } else {
                    // 处理UTF-8字符：对于多字节字符，需要完整读取
                    if ((*ptr & 0x80) == 0) {
                        // ASCII字符（单字节）
                        g_string_append_c(result_text, *ptr);
                    } else {
                        // UTF-8多字节字符，需要读取完整字符
                        const char *char_start = ptr;
                        int char_len = 1;

                        // 计算UTF-8字符长度
                        if ((*ptr & 0xE0) == 0xC0) char_len = 2;      // 110xxxxx
                        else if ((*ptr & 0xF0) == 0xE0) char_len = 3; // 1110xxxx
                        else if ((*ptr & 0xF8) == 0xF0) char_len = 4; // 11110xxx

                        // 添加完整的UTF-8字符
                        char *utf8_char = g_strndup(char_start, char_len);
                        g_string_append(result_text, utf8_char);
                        g_free(utf8_char);

                        // 跳过多字节字符的其余字节
                        ptr += char_len - 1;
                    }
                }
                ptr++;
            }

        } else {
            // 如果还没有开启颜色标记，开启第一个
            if (!color_section_open) {
                if (in_played_section) {
                    // 已播放部分：使用用户选择的颜色
                    gchar *user_color = g_strdup_printf("#%02x%02x%02x", 
                        (int)(osd->text_color.red * 255),
                        (int)(osd->text_color.green * 255),
                        (int)(osd->text_color.blue * 255));
                    g_string_append_printf(result_text, "<span foreground=\"%s\">", user_color);
                    g_free(user_color);
                } else {
                    g_string_append(result_text, "<span foreground=\"#666666\">");
                }
                color_section_open = TRUE;
            }

            // 添加字符（需要转义特殊字符）
            if (*ptr == '<') {
                g_string_append(result_text, "&lt;");
            } else if (*ptr == '>') {
                g_string_append(result_text, "&gt;");
            } else if (*ptr == '&') {
                g_string_append(result_text, "&amp;");
            } else {
                // 处理UTF-8字符：对于多字节字符，需要完整读取
                if ((*ptr & 0x80) == 0) {
                    // ASCII字符（单字节）
                    g_string_append_c(result_text, *ptr);
                } else {
                    // UTF-8多字节字符，需要读取完整字符
                    const char *char_start = ptr;
                    int char_len = 1;

                    // 计算UTF-8字符长度
                    if ((*ptr & 0xE0) == 0xC0) char_len = 2;      // 110xxxxx
                    else if ((*ptr & 0xF0) == 0xE0) char_len = 3; // 1110xxxx
                    else if ((*ptr & 0xF8) == 0xF0) char_len = 4; // 11110xxx

                    // 添加完整的UTF-8字符
                    char *utf8_char = g_strndup(char_start, char_len);
                    g_string_append(result_text, utf8_char);
                    g_free(utf8_char);

                    // 跳过多字节字符的其余字节
                    ptr += char_len - 1;
                }
            }
            ptr++;
        }
    }

    // 关闭最后的颜色标记
    if (color_section_open) {
        g_string_append(result_text, "</span>");
    }

    char *final_text = g_string_free(result_text, FALSE);

    // 使用Pango标记显示文本
    osd_lyrics_set_markup_text_safe(final_text);

    g_free(final_text);

    return TRUE; // 继续定时器
}

// 处理原始LRC格式歌词行
static void osd_lyrics_process_lrc_line(const char *lrc_line) {
    if (!osd || !osd->initialized || !lrc_line) return;

    printf("📝 [LRC处理] 原始行: %s\n", lrc_line);

    // 确保清理KRC状态（防止格式切换时的状态残留）
    clear_krc_state();

    // 提取文本内容（移除时间戳）
    const char *text_start = strchr(lrc_line, ']');
    if (text_start) {
        text_start++; // 跳过 ']'
        // 跳过可能的空格
        while (*text_start == ' ' || *text_start == '\t') text_start++;

        // 移除可能的换行符
        char *text_content = g_strdup(text_start);
        char *newline = strchr(text_content, '\r');
        if (newline) *newline = '\0';
        newline = strchr(text_content, '\n');
        if (newline) *newline = '\0';

        printf("📝 [LRC处理] 提取文本: %s\n", text_content);

        // 显示文本
        osd_lyrics_set_text_safe(text_content);

        g_free(text_content);
    } else {
        // 没有找到时间戳，直接显示原文
        osd_lyrics_set_text_safe(lrc_line);
    }
}

// 清理资源
void osd_lyrics_cleanup(void) {
    printf("🧹 [清理] 开始清理OSD歌词资源\n");

    // 首先标记为未初始化，防止其他线程继续访问
    if (osd) {
        osd->initialized = FALSE;
    }

    // 清理KRC状态（包括定时器）
    clear_krc_state();

    if (osd) {
        printf("🧹 [清理] 清理OSD对象资源\n");

        // 清理UI定时器
        if (osd->hide_timer_id > 0) {
            if (g_source_remove(osd->hide_timer_id)) {
                printf("🧹 [清理] 已停止隐藏定时器 (ID: %u)\n", osd->hide_timer_id);
            }
            osd->hide_timer_id = 0;
        }
        if (osd->unlock_timer_id > 0) {
            if (g_source_remove(osd->unlock_timer_id)) {
                printf("🧹 [清理] 已停止解锁定时器 (ID: %u)\n", osd->unlock_timer_id);
            }
            osd->unlock_timer_id = 0;
        }

        // 清理字符串资源
        if (osd->current_lyrics) {
            g_free(osd->current_lyrics);
            osd->current_lyrics = NULL;
        }
        if (osd->sse_url) {
            g_free(osd->sse_url);
            osd->sse_url = NULL;
        }

        // 销毁GTK窗口
        if (osd->window) {
            gtk_widget_destroy(osd->window);
            osd->window = NULL;
        }

        // 释放OSD结构体
        g_free(osd);
        osd = NULL;

        printf("🧹 [清理] OSD对象资源清理完成\n");
    }

    printf("✅ [清理] 所有资源清理完成\n");
}
