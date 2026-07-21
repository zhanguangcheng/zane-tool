# Zane Tool

基于 Qt6 的桌面端多功能工具箱，包含 ffmpeg 图形化批量处理、屏幕取色、截图贴图、窗口透明、计时器、Base64 编码、时间戳转换、Cron 解析、JWT 解析、随机字符串生成、二维码工具和批量下载等。

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| UI 框架 | Qt 6.8 (Widgets) |
| 构建系统 | CMake 3.16+ |
| 编译器 | MinGW GCC 13.1 (x86_64) |
| 外部依赖 | ffmpeg.exe, aria2c.exe (子进程调用) |
| 打包 | NSIS (安装包) |
| 样式 | Qt Style Sheets (Bootstrap v5 配色) |

## 项目结构

```
ZaneTool/
├── CMakeLists.txt
├── ffmpeg.exe
├── aria2c.exe
├── src/
│   ├── main.cpp                    # 入口 + 全局 QSS + ffmpeg/aria2 存在性检查
│   ├── mainwindow.h / .cpp         # 主窗口 UI + 全部标签页逻辑
│   ├── ffmpegprocess.h / .cpp      # QProcess 封装 (start/cancel/isRunning)
│   ├── imageprocessor.h / .cpp     # 图片任务结构 + ffmpeg 参数构建
│   ├── videoprocessor.h / .cpp     # 视频任务结构 + ffmpeg 参数构建
│   ├── audioprocessor.h / .cpp     # 音频任务结构 + ffmpeg 参数构建
│   ├── colorpicker.h / .cpp        # 全屏取色器 (屏幕任意位置拾取颜色)
│   ├── windowpicker.h / .cpp       # 窗口选择器 (全局钩子 → 获取 HWND)
│   ├── screenshotpicker.h / .cpp   # 区域截图选择器
│   ├── pinwindow.h / .cpp          # 截图贴图窗口 (拖拽/缩放)
│   ├── stopwatchtimer.h / .cpp     # 秒表引擎 (QTimer + QElapsedTimer)
│   ├── utils.h / .cpp              # 工具函数 (文件大小/格式检测/日志)
│   ├── third_party/
│   │   ├── qrcodegen.hpp / .cpp     # nayuki QR-Code-generator (MIT)
│   │   └── quirc.h / .c + ...       # Quirc 二维码解码库 (ISC)
│   ├── resources.qrc               # Qt 资源文件
│   └── app.rc                      # Windows 资源文件 (ICO)
├── build/                          # 编译输出 (已 gitignore)
├── logs/                           # 日志 (已 gitignore)
├── installer/
│   ├── installer.nsi               # NSIS 安装脚本
│   └── ZaneTool/                   # 打包临时目录 (已 gitignore)
└── dist/                           # 发布产物 (.zip)
```

## 架构概览

```
main.cpp  →  MainWindow (侧边栏 + QStackedWidget 多标签页)
                   │
                   ├─ ImageProcessor    (静态: 构建图片 ffmpeg 参数)
                   ├─ VideoProcessor    (静态: 构建视频 ffmpeg 参数)
                   ├─ AudioProcessor    (静态: 构建音频 ffmpeg 参数)
                   ├─ FFmpegProcess     (QProcess 封装: 启动/取消/监控)
                   ├─ ColorPicker       (全屏取色 Widget)
                   ├─ WindowPicker      (窗口选择 Widget)
                   ├─ ScreenshotPicker  (区域截图 Widget)
                   ├─ PinWindow         (贴图窗口 Widget)
                   ├─ StopwatchTimer    (秒表引擎 QObject)
                   └─ Utils             (静态: 格式检测/文件大小/日志)
```

- **单进程模型**: 每个标签页同时运行一个 ffmpeg 子进程，`QList<Task>` 串行队列
- **进度**: 文件级别 `(currentIndex / total) × 100`
- **取消**: 设置取消标志 → kill QProcess → 等待最多 3s → 恢复 UI
- **日志**: `Utils::logToFile()` 写入 `logs/app.log`

## 功能清单

### 媒体工具

#### 图片处理
- 质量压缩 (0–100 滑块，JPG/PNG/WebP)
- 缩放下采样 (仅缩小，不放大)
- 格式转换 (保持原格式 / JPG / PNG / WebP / BMP)
- 批量串行处理，分辨率预览

#### 视频处理
- 编码格式 (保持原格式 / H.264 / H.265 / VP9 / AVI / MOV)
- CRF 质量控制 (0–51 滑块)
- 缩放到预设分辨率 (1080p / 720p / 480p)
- 批量串行处理，视频信息预览

#### 音频处理
- 编码格式 (保持原格式 / MP3 / AAC / FLAC / WAV / OGG / Opus)
- 比特率选择 (64/96/128/192/256/320 kbps)
- 采样率 / 声道数控制
- 批量串行处理

### 系统工具

#### 屏幕取色
- 全屏取色器，支持鼠标移动实时预览
- HEX / RGB / HSL 三格式显示，一键复制
- 最多 9 个颜色历史记录

#### 截图贴图
- 全局热键 (默认 F4) 触发区域截图
- 截图自动贴到屏幕 (可拖拽、滚轮缩放、右键关闭)
- 支持同时贴多张图

#### 窗口透明
- 列出所有顶层窗口，选择后调节透明度 (30%–100%)
- 窗口选择器辅助拾取

#### 计时器
- 毫秒级秒表 (精确到 0.01s)
- 计次记录 (分段/累计时间)
- 开始/暂停/停止/计次 控制

### 开发工具

#### 图片转 Base64
- 选择或拖放图片文件，生成 Data URI 格式 Base64
- 一键复制到剪贴板

#### 时间戳转换
- 当前时间戳实时刷新 (秒级/毫秒级)
- 时间戳 → 日期时间 (支持秒/毫秒切换)
- 日期时间 → 时间戳 (日历选择器，秒+毫秒双输出)
- 各字段一键复制

#### 定时任务 (Cron 解析)
- Cron 表达式输入与预设选择（每1分钟/每5分钟/每小时/每天零点等 10 种预设）
- 字段解析（分钟/小时/日期/月份/星期）高亮显示，无效表达式标红提示
- 未来执行时间表，支持显示 5/10/20/50 次，相对时间自动刷新
- 复制全部按钮

#### JWT 解析
- JWT 输入，按 `.` 分隔 Header/Payload/Signature 三部分
- Base64url 解码后 JSON 美化展示（三标签页切换）
- Payload 自动识别时间戳字段 (iat/exp/nbf) 转换为可读日期
- 复制当前 / 复制全部按钮

#### 随机字符串
- 字符集选择：大写字母 / 小写字母 / 数字 / 特殊符号，支持排除字符
- 长度 1–256，数量 1–1000 可配置
- 使用 QRandomGenerator 生成，结果以 Consolas 等宽字体展示
- 生成 / 复制全部按钮

#### 二维码工具
- **生成**：文本输入 → QR Code 生成 (nayuki 库)，支持 ECC 级别和缩放倍率
- **解码**：从文件或屏幕截图识别二维码 (Quirc 库)，支持拖放图片
- 保存为 PNG / 复制图像 / 复制结果 / 打开链接

### 网络工具

#### 批量下载
- 基于 aria2c 子进程的多文件并发下载
- URL 输入支持自定义文件名（`URL 文件名` 格式）
- 实时进度表：文件名 / 进度条 / 速度 / ETA / 状态
- 可配置最大并发数、连接数、速度限制
- 取消下载、完成后自动清理临时文件

## 批量处理流程

```
onStart()
  → 构建任务队列 QList<Task> (每个文件一个 Task)
  → 重置统计 → setUiEnabled(false)
  → processNext():
      ↳ 已完成: showBatchSummary() → 显示汇总对话框
      ↳ 未完成: buildArgs() → ffmpeg.start()
  → onFinished():
      ↳ 成功: 累加统计
      ↳ 失败: 记录失败文件列表
      → currentIndex++ → processNext()
```

## 构建

### 前提

- Qt 6.8+ (Widgets 模块)
- CMake 3.16+
- MinGW GCC 13.1 (x86_64)
- ffmpeg.exe 放置于项目根目录
- aria2c.exe 放置于项目根目录（批量下载功能需要）

### 编译

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="C:/qt/6.8.3/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/qt/Tools/mingw1310_64/bin/c++.exe" `
  -DCMAKE_RC_COMPILER="C:/qt/Tools/mingw1310_64/bin/windres.exe"

C:\Qt\Tools\CMake_64\bin\cmake.exe --build build --target ZaneTool
```

### 部署 Qt 运行时 DLL

```powershell
C:\qt\6.8.3\mingw_64\bin\windeployqt.exe build\ZaneTool.exe --no-translations --no-opengl-sw
```

### 打包

```powershell
# NSIS installer
makensis installer\installer.nsi
```

## 全局样式

QSS 嵌入 `src/main.cpp`，Bootstrap v5 配色：

| 元素 | 色值 | 说明 |
|------|------|------|
| 背景 | #f8f9fa | 主窗口 + 内容区 |
| 卡片 | #ffffff + border #dee2e6 | QGroupBox |
| 主按钮 | #0d6efd | QPushButton |
| 危险按钮 | #dc3545 | #dangerBtn |
| 警告按钮 | #fd7e14 | 计时器暂停 |
| 成功按钮 | #198754 | 计时器开始 |
| 聚焦高亮 | #86b7fe | 输入框焦点 |
| 文字 | #212529 | 默认文字色 |

## 许可

内部工具，未指定开源许可。
