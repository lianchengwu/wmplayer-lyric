#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "osd_lyrics.h"

int main(int argc, char *argv[]) {
    gchar *sse_url = NULL;

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
        fprintf(stderr, "初始化OSD歌词系统失败\n");
        return 1;
    }

    // 显示窗口
    osd_lyrics_set_visible(TRUE);

    // 进入主循环
    gtk_main();

    // 清理资源
    osd_lyrics_cleanup();

    return 0;
}
