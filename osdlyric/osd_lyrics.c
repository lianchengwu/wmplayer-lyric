#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <gtk/gtk.h>
#include "osd_lyrics.h"

// 信号处理器
static void signal_handler(int sig) {
    printf("🛑 [信号] 收到信号 %d，开始优雅退出\n", sig);

    // 清理资源
    osd_lyrics_cleanup();

    // 退出GTK主循环
    if (gtk_main_level() > 0) {
        gtk_main_quit();
    }

    printf("👋 [信号] 程序退出\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    gchar *sse_url = NULL;

    // 设置信号处理器
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // 终止信号
    signal(SIGHUP, signal_handler);   // 挂起信号

    printf("🚀 [启动] OSD歌词程序启动\n");

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sse-url") == 0 && i + 1 < argc) {
            sse_url = argv[i + 1];
            i++; // 跳过下一个参数
        }
    }

    // 初始化GTK
    gtk_init(&argc, &argv);

    // 初始化OSD歌词
    if (!osd_lyrics_init_with_sse(sse_url)) {
        fprintf(stderr, "❌ [启动] 初始化OSD歌词系统失败\n");
        return 1;
    }

    printf("✅ [启动] OSD歌词系统初始化成功\n");

    // 显示窗口
    osd_lyrics_set_visible(TRUE);

    printf("🎵 [启动] 进入主循环，等待歌词数据...\n");

    // 进入主循环
    gtk_main();

    printf("🛑 [退出] 主循环结束，开始清理资源\n");

    // 清理资源
    osd_lyrics_cleanup();

    printf("👋 [退出] 程序正常退出\n");
    return 0;
}
