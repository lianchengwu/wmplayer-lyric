import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents

PlasmoidItem {
    preferredRepresentation: fullRepresentation

    property string currentLyrics: ""
    property string songName: ""
    property string artist: ""
    property bool isConnected: false
    property bool isConnecting: false  // 新增：是否正在连接中
    property bool isKrcFormat: false
    property var sseRequest: null

    // 卡拉OK相关属性
    property var karaokeChars: []
    property var karaokeTimings: []
    property int currentCharIndex: 0
    property real startTime: 0
    property bool karaokeActive: false

    // 主题颜色属性 (直接使用PlasmaCore.Theme)
    readonly property color textColor: PlasmaCore.Theme.textColor
    readonly property color highlightColor: PlasmaCore.Theme.highlightColor

    fullRepresentation: Item {
        // 动态宽度：根据歌词内容自动调整
        Layout.preferredWidth: Math.max(200, lyricsContainer.implicitWidth + 50) // 最小200px，为状态指示器预留空间
        Layout.preferredHeight: Math.max(40, lyricsContainer.implicitHeight + 20)

        // 歌词显示区域 - 使用ScrollView实现水平滚动
        ScrollView {
            id: lyricsContainer
            anchors.left: parent.left
            anchors.right: statusIndicator.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 5   // 只在左侧留边距
            anchors.rightMargin: 5 // 给状态指示器留出空间

            // 只有连接成功时才显示歌词
            visible: isConnected

            // 只启用水平滚动
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff
            
            // 确保滚动条从右侧开始
            ScrollBar.horizontal.position: 1.0 - ScrollBar.horizontal.size

            // 滚动内容区域
            contentWidth: isKrcFormat ? karaokeWrapper.width : lyricsWrapper.width
            contentHeight: height

            property real implicitWidth: isKrcFormat ? karaokeRow.implicitWidth : lyricsWrapper.implicitWidth
            property real implicitHeight: isKrcFormat ? karaokeRow.implicitHeight : lyricsWrapper.implicitHeight

            // 歌词包装器 - 用于实现右对齐
            Item {
                id: lyricsWrapper
                width: Math.max(lyricsContainer.width, lyric_label.implicitWidth)
                height: lyricsContainer.height
                visible: !isKrcFormat

                property real implicitWidth: lyric_label.implicitWidth
                property real implicitHeight: lyric_label.implicitHeight

                // 普通歌词显示 (LRC格式)
                Label {
                    id: lyric_label
                    text: currentLyrics
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                    color: textColor
                    font.bold: true
                    font.pointSize: PlasmaCore.Theme.defaultFont.pointSize
                    wrapMode: Text.NoWrap // 不换行，让ScrollView处理滚动
                    width: implicitWidth
                }
            }

            // 卡拉OK歌词显示 (KRC格式)
            Item {
                id: karaokeWrapper
                width: Math.max(lyricsContainer.width, karaokeRow.implicitWidth)
                height: lyricsContainer.height
                visible: isKrcFormat

                Row {
                    id: karaokeRow
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    spacing: 0

                    // 使用自然宽度
                    width: implicitWidth

                Repeater {
                    id: karaokeRepeater
                    model: karaokeChars

                    Label {
                        text: modelData.char
                        color: modelData.highlighted ? highlightColor : textColor
                        font.bold: true
                        font.pointSize: PlasmaCore.Theme.defaultFont.pointSize

                        // 高亮动画
                        Behavior on color {
                            ColorAnimation {
                                duration: 200
                                easing.type: Easing.OutQuad
                            }
                        }

                        // 缩放动画
                        transform: Scale {
                            origin.x: width / 2
                            origin.y: height / 2
                            xScale: modelData.highlighted ? 1.1 : 1.0
                            yScale: modelData.highlighted ? 1.1 : 1.0

                            Behavior on xScale {
                                NumberAnimation {
                                    duration: 200
                                    easing.type: Easing.OutBack
                                }
                            }
                            Behavior on yScale {
                                NumberAnimation {
                                    duration: 200
                                    easing.type: Easing.OutBack
                                }
                            }
                        }
                    }
                }
                }
            }
        }

        // 连接状态指示器
        Rectangle {
            id: statusIndicator
            width: 10
            height: 10
            radius: 5
            color: isConnecting ? "orange" : (isConnected ? "green" : "red")
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 5
            opacity: 0.8
            z: 10 // 确保在最上层

            ToolTip.visible: statusMouseArea.containsMouse
            ToolTip.text: isConnecting ? "正在连接歌词服务..." : (isConnected ? "已连接到歌词服务" : "未连接到歌词服务")

            MouseArea {
                id: statusMouseArea
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        // SSE连接管理
        Component.onCompleted: {
            console.log("Plasma歌词组件初始化")
            connectToSSE()
        }

        Component.onDestruction: {
            console.log("Plasma歌词组件销毁")
            
            // 停止所有定时器
            reconnectTimer.stop()
            heartbeatTimer.stop()
            karaokeTimer.stop()
            
            // 断开SSE连接
            disconnectSSE()
            
            // 清理数组引用
            karaokeChars = []
            karaokeTimings = []
        }

        // 重连定时器
        Timer {
            id: reconnectTimer
            interval: 3000
            repeat: false
            onTriggered: {
                console.log("尝试重新连接SSE...")
                connectToSSE()
            }
        }

        // 心跳检测定时器 - 优化检查频率
        Timer {
            id: heartbeatTimer
            interval: 60000 // 60秒检查一次，减少CPU占用
            repeat: true
            running: !isConnected && !isConnecting // 只在未连接且未连接中时运行
            onTriggered: {
                if (!isConnected && !isConnecting) {
                    console.log("心跳检测：连接已断开，尝试重连")
                    connectToSSE()
                }
            }
        }

        // 卡拉OK动画定时器 - 优化频率降低CPU占用
        Timer {
            id: karaokeTimer
            interval: 200 // 200ms更新一次，平衡流畅度和性能
            repeat: true
            running: karaokeActive && isKrcFormat
            onTriggered: {
                updateKaraokeHighlight()
                updateScrollPosition()
            }
        }

        // 内存清理定时器 - 定期清理不必要的数据
        Timer {
            id: memoryCleanupTimer
            interval: 300000 // 5分钟清理一次
            repeat: true
            running: true
            onTriggered: {
                // 强制垃圾回收（如果可用）
                if (typeof gc !== 'undefined') {
                    gc()
                }
                
                // 清理长时间未使用的数据
                if (!karaokeActive && karaokeChars.length > 0) {
                    console.log("清理卡拉OK数据，释放内存")
                    karaokeChars = []
                    karaokeTimings = []
                }
            }
        }

        // 自动滚动到当前字符 - 优化性能
        property int lastScrollCharIndex: -1 // 缓存上次滚动的字符索引

        function updateScrollPosition() {
            if (!karaokeActive || currentCharIndex < 0 || !isKrcFormat) return

            // 只在字符索引变化时才更新滚动位置
            if (currentCharIndex === lastScrollCharIndex) return
            lastScrollCharIndex = currentCharIndex

            // 计算当前字符的大概位置
            var charWidth = 16 // 大约一个字符的宽度
            var currentCharX = currentCharIndex * charWidth
            var viewWidth = lyricsContainer.width
            var contentWidth = karaokeRow.implicitWidth

            // 如果内容宽度小于视图宽度，不需要滚动
            if (contentWidth <= viewWidth) return

            // 计算滚动位置，让当前字符保持在视图中央
            var targetPosition = Math.max(0, Math.min(
                currentCharX - viewWidth / 2,
                contentWidth - viewWidth
            ))

            // 平滑滚动到目标位置
            if (Math.abs(lyricsContainer.ScrollBar.horizontal.position - targetPosition) > 5) {
                lyricsContainer.ScrollBar.horizontal.position = targetPosition
            }
        }

        // 连接到SSE服务
        function connectToSSE() {
            if (sseRequest) {
                disconnectSSE()
            }

            console.log("连接到SSE服务: http://127.0.0.1:18911/api/osd-lyrics/sse")

            // 设置连接状态
            isConnecting = true
            isConnected = false

            sseRequest = new XMLHttpRequest()
            sseRequest.open("GET", "http://127.0.0.1:18911/api/osd-lyrics/sse")
            sseRequest.setRequestHeader("Accept", "text/event-stream")
            sseRequest.setRequestHeader("Cache-Control", "no-cache")

            var buffer = ""
            var lastProcessedLength = 0

            sseRequest.onreadystatechange = function() {
                if (sseRequest.readyState === XMLHttpRequest.HEADERS_RECEIVED) {
                    console.log("SSE连接建立，状态码:", sseRequest.status)
                    if (sseRequest.status === 200) {
                        isConnected = true
                        isConnecting = false  // 连接成功，结束连接状态
                        heartbeatTimer.running = false // 连接成功时停止心跳检测
                    }
                } else if (sseRequest.readyState === XMLHttpRequest.LOADING) {
                    // 处理流式数据，避免缓冲区无限增长
                    var responseText = sseRequest.responseText
                    var newData = responseText.substring(lastProcessedLength)
                    lastProcessedLength = responseText.length

                    if (newData) {
                        processSSEData(newData)
                    }

                    // 定期清理缓冲区，防止内存泄露
                    if (responseText.length > 10000) { // 10KB限制
                        lastProcessedLength = 0
                        console.log("清理SSE缓冲区，防止内存泄露")
                    }
                } else if (sseRequest.readyState === XMLHttpRequest.DONE) {
                    console.log("SSE连接断开，状态码:", sseRequest.status)
                    isConnected = false
                    isConnecting = false  // 连接结束
                    heartbeatTimer.running = true // 连接断开时启动心跳检测

                    // 清空歌词内容
                    currentLyrics = ""
                    resetKaraoke()

                    // 清理缓冲区变量
                    buffer = ""
                    lastProcessedLength = 0

                    if (sseRequest.status !== 200) {
                        console.log("SSE连接失败，3秒后重试")
                        reconnectTimer.start()
                    }
                }
            }

            sseRequest.onerror = function() {
                console.log("SSE连接错误")
                isConnected = false
                isConnecting = false  // 连接错误，结束连接状态
                heartbeatTimer.running = true // 连接错误时启动心跳检测

                // 清空歌词内容
                currentLyrics = ""
                resetKaraoke()

                reconnectTimer.start()
            }

            try {
                sseRequest.send()
            } catch (e) {
                console.log("发送SSE请求失败:", e)
                isConnected = false
                isConnecting = false  // 发送失败，结束连接状态
                heartbeatTimer.running = true // 发送失败时启动心跳检测

                // 清空歌词内容
                currentLyrics = ""
                resetKaraoke()

                reconnectTimer.start()
            }
        }

        // 断开SSE连接
        function disconnectSSE() {
            if (sseRequest) {
                try {
                    // 清理事件处理器，防止内存泄露
                    sseRequest.onreadystatechange = null
                    sseRequest.onerror = null
                    sseRequest.abort()
                } catch (e) {
                    console.log("断开SSE连接时出错:", e)
                }
                sseRequest = null
            }
            isConnected = false
            isConnecting = false  // 断开连接，结束连接状态
            heartbeatTimer.running = true // 断开连接时启动心跳检测

            // 清空歌词内容
            currentLyrics = ""
            resetKaraoke()
        }

        // 处理SSE数据
        function processSSEData(data) {
            var lines = data.split('\n')

            for (var i = 0; i < lines.length; i++) {
                var line = lines[i].trim()

                if (line.startsWith('data: ')) {
                    var jsonData = line.substring(6) // 移除 'data: ' 前缀

                    try {
                        var eventData = JSON.parse(jsonData)
                        handleSSEEvent(eventData)
                    } catch (e) {
                        console.log("解析SSE JSON数据失败:", e, "数据:", jsonData)
                    }
                }
            }
        }

        // 处理SSE事件
        function handleSSEEvent(eventData) {
            console.log("收到SSE事件:", eventData.type)

            switch (eventData.type) {
                case "connected":
                    console.log("SSE连接成功")
                    isConnected = true
                    break

                case "lyrics_update":
                    handleLyricsUpdate(eventData)
                    break

                case "heartbeat":
                    console.log("收到心跳")
                    isConnected = true
                    break

                default:
                    console.log("未知SSE事件类型:", eventData.type)
            }
        }

        // 处理歌词更新
        function handleLyricsUpdate(eventData) {
            songName = eventData.songName || ""
            artist = eventData.artist || ""
            var format = eventData.format || "lrc"
            var text = eventData.text || ""

            console.log("歌词更新 (" + format + "):", songName, "-", artist)

            if (format === "krc") {
                // KRC格式：处理渐进式歌词
                isKrcFormat = true
                processKrcLyrics(text)
            } else {
                // LRC格式：提取纯文本
                isKrcFormat = false
                processLrcLyrics(text)
            }
        }

        // 重置滚动位置 - 对于右对齐，应该滚动到最右侧
        function resetScrollPosition() {
            if (lyricsContainer.contentWidth > lyricsContainer.width) {
                // 如果内容超出容器宽度，滚动到最右侧
                lyricsContainer.ScrollBar.horizontal.position = 1.0 - lyricsContainer.ScrollBar.horizontal.size
            } else {
                // 如果内容没有超出，保持在0位置
                lyricsContainer.ScrollBar.horizontal.position = 0
            }
        }

        // 处理KRC格式歌词
        function processKrcLyrics(krcText) {
            if (!krcText) {
                currentLyrics = ""
                resetKaraoke()
                return
            }

            console.log("处理KRC歌词:", krcText)

            // KRC格式示例: [171960,5040]<0,240,0>你<240,150,0>走<390,240,0>之<630,240,0>后
            // 解析时间标记和字符
            var timeMatch = krcText.match(/\[(\d+),(\d+)\]/)
            if (!timeMatch) {
                // 如果没有时间标记，当作普通文本处理
                currentLyrics = krcText.replace(/<[\d,]+>/g, '')
                resetKaraoke()
                return
            }

            var lineStartTime = parseInt(timeMatch[1]) // 行开始时间 (毫秒)
            var lineDuration = parseInt(timeMatch[2])  // 行持续时间 (毫秒)

            // 提取字符和时间信息
            var chars = []
            var timings = []
            var remainingText = krcText.replace(/\[[\d,]+\]/, '') // 移除行时间标记

            // 解析每个字符的时间标记 <开始时间,持续时间,保留字段>字符
            // 修复：正确处理KRC格式，提取时间戳后到下一个时间戳前的所有字符

            // 先找到所有时间标记的位置
            var timeMarkPattern = /<(\d+),(\d+),\d+>/g
            var timeMarks = []
            var timeMatch

            while ((timeMatch = timeMarkPattern.exec(remainingText)) !== null) {
                timeMarks.push({
                    startTime: parseInt(timeMatch[1]),
                    duration: parseInt(timeMatch[2]),
                    markEnd: timeMatch.index + timeMatch[0].length,
                    markStart: timeMatch.index
                })
            }

            // 处理每个时间标记后的文本段
            for (var i = 0; i < timeMarks.length; i++) {
                var timeMark = timeMarks[i]
                var nextMarkStart = (i + 1 < timeMarks.length) ?
                    timeMarks[i + 1].markStart : remainingText.length

                // 提取时间标记后到下一个时间标记前的所有字符
                var textSegment = remainingText.substring(timeMark.markEnd, nextMarkStart)

                // 将文本段作为一个整体处理，或者按字符分解
                if (textSegment.length > 0) {
                    // 按字符分解文本段，每个字符使用相同的时间戳
                    for (var j = 0; j < textSegment.length; j++) {
                        var character = textSegment.charAt(j)

                        chars.push({
                            char: character,
                            highlighted: false
                        })

                        timings.push({
                            startTime: timeMark.startTime,
                            duration: timeMark.duration,
                            absoluteStartTime: lineStartTime + timeMark.startTime
                        })
                    }
                }
            }



            if (chars.length > 0) {
                karaokeChars = chars
                karaokeTimings = timings
                startTime = Date.now()
                currentCharIndex = 0
                karaokeActive = true

                console.log("KRC解析完成，字符数:", chars.length)
                console.log("时间信息:", timings.slice(0, 3)) // 显示前3个字符的时间信息

                // 重置滚动位置到开始
                resetScrollPosition()
            } else {
                // 如果解析失败，显示纯文本
                var textOnly = krcText.replace(/\[[\d,]+\]/g, '')
                                     .replace(/<[\d,]+>/g, '')
                currentLyrics = textOnly.trim() || "♪ 音乐播放中 ♪"
                resetKaraoke()
            }
        }

        // 处理LRC格式歌词
        function processLrcLyrics(lrcText) {
            if (!lrcText) {
                currentLyrics = ""
                resetKaraoke()
                return
            }

            // LRC格式示例: [02:51.96]你走之后我又 再为谁等候
            // 提取歌词文本，移除时间标记
            var textOnly = lrcText.replace(/\[\d{2}:\d{2}\.\d{2}\]/g, '').trim()

            if (textOnly) {
                currentLyrics = textOnly
            } else {
                currentLyrics = "♪ 音乐播放中 ♪"
            }

            resetKaraoke()
            resetScrollPosition()
        }

        // 重置卡拉OK状态
        function resetKaraoke() {
            karaokeActive = false
            karaokeChars = []
            karaokeTimings = []
            currentCharIndex = 0
            resetScrollPosition()
        }

        // 更新卡拉OK高亮效果 - 优化性能，避免内存泄露
        function updateKaraokeHighlight() {
            if (!karaokeActive || karaokeTimings.length === 0) {
                return
            }

            var currentTime = Date.now() - startTime
            var updated = false
            var allCompleted = true

            // 单次遍历检查高亮状态和完成状态
            for (var i = 0; i < karaokeTimings.length; i++) {
                var timing = karaokeTimings[i]
                var shouldHighlight = currentTime >= timing.startTime &&
                                    currentTime < (timing.startTime + timing.duration)

                // 检查是否需要更新高亮状态
                if (karaokeChars[i].highlighted !== shouldHighlight) {
                    karaokeChars[i].highlighted = shouldHighlight
                    updated = true
                }

                // 同时检查是否完成
                if (currentTime < (timing.startTime + timing.duration)) {
                    allCompleted = false
                }
            }

            // 只在有实际更新时才触发UI刷新
            // 避免频繁重建数组，使用信号通知更新
            if (updated) {
                // 触发模型更新，但不重新分配数组
                karaokeRepeater.model = null
                karaokeRepeater.model = karaokeChars
            }

            // 如果所有字符都完成了，停止卡拉OK动画
            if (allCompleted && karaokeTimings.length > 0) {
                console.log("卡拉OK动画完成")
                karaokeActive = false
                // 清理完成后的数据，释放内存
                Qt.callLater(function() {
                    if (!karaokeActive) {
                        karaokeChars = []
                        karaokeTimings = []
                    }
                })
            }
        }
    }
}
