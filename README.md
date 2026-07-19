# Zane Tool

基于 Qt6 的桌面端 ffmpeg 图形化批量处理工具，支持图片与视频的压缩、缩放、格式转换。

## 技术栈

| 层级 | 技术 |
|------|------|
| 语言 | C++17 |
| UI 框架 | Qt 6 (Widgets) |
| 构建系统 | CMake 3.16+ |
| 编译器 | MinGW GCC 13.1 (x86_64) |
| 外部依赖 | ffmpeg.exe (子进程调用) |
| 打包 | CPack (ZIP) + NSIS (安装包) |
| 样式 | Qt Style Sheets (Bootstrap v5 配色) |

## 项目结构

```
ZaneTool/
├── CMakeLists.txt              # 构建配置
├── ffmpeg.exe                  # 捆绑的 ffmpeg 二进制
├── src/
│   ├── main.cpp                # 入口 + 全局 QSS 样式表
│   ├── mainwindow.h/cpp        # 主窗口 UI + 批量处理逻辑
│   ├── ffmpegprocess.h/cpp     # QProcess 封装 (启动/取消/进度解析)
│   ├── imageprocessor.h/cpp    # 图片参数构建 + 输出路径
│   ├── videoprocessor.h/cpp    # 视频参数构建 + 输出路径
│   └── utils.h/cpp             # 工具函数 (文件大小格式化)
├── build/                      # 编译输出
├── installer/
│   ├── installer.nsi           # NSIS 安装脚本
│   └── ZaneTool/         # 打包阶段目录
└── dist/                       # 发布产物 (.zip)
```

## 架构概览

```
main.cpp  →  MainWindow (UI + 任务编排)
                  │
                  ├─ ImageProcessor (静态: 构建图片 ffmpeg 参数)
                  ├─ VideoProcessor (静态: 构建视频 ffmpeg 参数)
                  ├─ FFmpegProcess (QProcess 封装: 启动/取消/监控)
                  └─ Utils (静态: 文件大小格式化)
```

- **单进程模型**: ffmpeg 以子进程形式运行，`QProcess` 管理生命周期
- **批量队列**: `MainWindow` 维护 `QList<ImageTask>` / `QList<VideoTask>`，串行逐一处理
- **进度**: 文件级别 `(当前序号 / 总数) × 100`
- **取消**: kill 当前进程 + 清空剩余队列

## 数据模型

### ImageTask (`imageprocessor.h`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `inputPath` | QString | 输入文件路径 |
| `outputDir` | QString | 输出目录 |
| `outputName` | QString | 输出文件名 (不含扩展名) |
| `format` | QString | ""=保持原格式 / "jpg"/"png"/"webp"/"bmp" |
| `quality` | int | 0-100 质量值 |
| `enableScale` | bool | 是否启用缩放 |
| `scaleWidth` | int | 目标水平像素 (仅原图宽度 > 该值时下采样) |

### VideoTask (`videoprocessor.h`)

| 字段 | 类型 | 说明 |
|------|------|------|
| `inputPath` | QString | 输入文件路径 |
| `outputDir` | QString | 输出目录 |
| `outputName` | QString | 输出文件名 (不含扩展名) |
| `format` | QString | ""=保持原格式 / "mp4"/"mp4_hevc"/"webm"/"avi"/"mov" |
| `crf` | int | CRF 0-51 (越小质量越高) |
| `enableScale` | bool | 是否启用缩放 |
| `scaleRatio` | double | 缩放比例 (百分比模式) |
| `presetRes` | QString | 预设分辨率 "1080"/"720"/"480" 或 "original" |

## 界面布局

```
┌ QTabWidget ─────────────────────────────────────┐
│ [图片处理]  [视频处理]                             │
├──────────────────────────────────────────────────┤
│  ┌ 输入文件 (可多选) ───────────────────────┐     │
│  │ [文件列表]          (支持拖拽/多选/移除)  │     │
│  │ [添加文件] [移除选中] [清空]               │     │
│  └──────────────────────────────────────────┘     │
│  ┌ 压缩设置 ───┐ ┌ 缩放设置 ────────────────┐     │
│  │ 质量: ──○─ │ │ ☑ 启用缩放               │     │
│  │          75 │ │ 目标宽度: [1920 px]       │     │
│  └─────────────┘ └──────────────────────────┘     │
│  ┌ 输出格式 ───┐ ┌ 输出目录 ────────────────┐     │
│  │ 格式: [▼]  │ │ [__________] [浏览]      │     │
│  └─────────────┘ └──────────────────────────┘     │
├──────────────────────────────────────────────────┤
│ ▓▓▓▓▓▓▓▓▓░░░░░ 进度条                             │
│ 就绪                                              │
│ ffmpeg.exe -y -i "input.jpg" ...                   │
│ [开始处理]     [取消]                               │
└──────────────────────────────────────────────────┘
```

视频 Tab 布局类似，编码设置 + 缩放设置并排，输出目录独占一行。

## 功能清单

### 图片处理

| 功能 | 参数 | ffmpeg 实现 |
|------|------|------------|
| 质量压缩 | 0-100 滑块 | JPG: `-q:v` (反比映射 2-31) + `-huffman optimal` |
| | | PNG: `-pred mixed` |
| | | WebP: `-quality N -lossless 0` |
| 缩放下采样 | 1-10000 px 数字框 | `scale='if(gt(iw,N),N,iw)':-1` (仅缩小) |
| 格式转换 | 保持原格式 / JPG / PNG / WebP / BMP | 自动检测输入扩展名 |
| 批量 | 多文件同时选择 | 串行队列，统一设置 |
| 分辨率预览 | 选中单个文件时 | 显示原图尺寸 + 预估输出尺寸 |

### 视频处理

| 功能 | 参数 | ffmpeg 实现 |
|------|------|------------|
| 编码格式 | 保持原格式 / H.264 / H.265 / VP9 / AVI / MOV | 容器+编码器自动映射 |
| CRF 质量 | 0-51 滑块 (默认 23) | `-crf N` |
| 缩放到预设分辨率 | 1080p / 720p / 480p | `scale=-2:HEIGHT` |
| 音频 | 固定直通 | `-c:a copy` |
| 批量 | 多文件同时选择 | 串行队列，统一设置 |

### 编码器映射

| 格式 | 容器 | 编码器 | 特殊参数 |
|------|------|--------|----------|
| MP4 (H.264) | .mp4 | libx264 | - |
| MP4 (H.265) | .mp4 | libx265 | `-tag:v hvc1` |
| WebM (VP9) | .webm | libvpx-vp9 | `-b:v 0` |
| AVI | .avi | mpeg4 | `-q:v` (CRF→q映射) |
| MOV | .mov | libx264 | - |

## ffmpeg 调用示例

### 图片 (JPG, 质量 75, 缩放到 1920px)

```
ffmpeg -y -i "photo.jpg" -map_metadata -1 \
  -vf "scale='if(gt(iw,1920),1920,iw)':-1" \
  -q:v 10 -huffman optimal \
  "output/photo.jpg"
```

### 视频 (H.264, CRF 23, 缩放到 1080p)

```
ffmpeg -y -i "video.mp4" -map_metadata -1 \
  -vf "scale=-2:1080" \
  -c:v libx264 -crf 23 -c:a copy \
  "output/video.mp4"
```

## 批量处理流程

```
onStart()
  → 构建任务队列 QList<Task> (每个文件一个 Task)
  → startBatch(): 重置计数器/统计
  → processNext():
      ↳ 已完成: showBatchSummary() → 显示汇总对话框
      ↳ 未完成: buildArgs() → ffmpeg.start()
              → 进度条更新 + 显示当前执行命令
  → onFinished():
      ↳ 成功: 累加文件大小统计
      ↳ 失败: 记录失败文件
      → currentIndex++ → processNext()

取消: onCancel() → cancelling=true → ffmpeg.kill() → finishBatch()
```

## 构建

### 前提

- Qt 6.8+ (Widgets 模块)
- CMake 3.16+
- MinGW GCC (x86_64)
- ffmpeg.exe 放置于项目根目录

### 命令

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/c++.exe"

cmake --build build --target ZaneTool
```

构建后运行 windeployqt 部署 Qt 运行时 DLL：

```powershell
windeployqt build\ZaneTool.exe --no-translations --no-opengl-sw
```

### 打包

```powershell
# ZIP (portable)
cmake --build build --target ZaneTool
# 手动将 build 目录文件打包为 zip

# NSIS installer
makensis installer\installer.nsi
```

## 样式表

全局 QSS 样式嵌入 `src/main.cpp`，采用 Bootstrap v5 配色方案：

| 元素 | 主色 |
|------|------|
| 按钮 (primary) | #0d6efd |
| 按钮 (danger) | #dc3545 |
| 按钮 (warning) | #ffc107 |
| 输入框边框 | #ced4da |
| 聚焦高亮 | #86b7fe |
| 背景 | #f8f9fa |
| 深色文字 | #212529 |

## 许可

内部工具，未指定开源许可。
