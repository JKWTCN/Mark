# Mark - Detection Point Annotation Tool

[English](#english) | [中文](#中文)

---

`<a id="english"></a>`

## English

### Overview

Mark is a lightweight detection point annotation tool built with **Qt 6 / C++17**. Click on images to mark detection points, then export their coordinates and colors in various formats. Ideal for computer vision workflows, calibration data collection, and color sampling.

### Features

- **Point Annotation** - Click to add detection points on images
- **Dual Coordinate Systems** - Pixel coordinates and normalized coordinates (0.0–1.0)
- **Multiple Color Formats** - RGB, HEX, HSL, HSV, CMYK
- **Folder Browsing** - Navigate image sequences with arrow keys
- **Zoom** - Smooth zoom up to 3200%, pixel-perfect rendering at high zoom
- **Minimap** - Overview navigation widget
- **Drag & Drop** - Drop images directly into the window
- **JSON Export/Import** - Save and load annotation configs with full metadata
- **Copy & Paste** - Copy coordinates, colors, or image files to clipboard
- **Explorer Integration** - Open file location or select in explorer
- **Customizable Precision** - Adjustable decimal places for coordinates and colors
- **Keyboard Shortcuts** - Full keyboard shortcut support for common operations

### Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+O` | Open image |
| `Ctrl+Shift+O` | Open folder |
| `Ctrl+L` | Load config |
| `Ctrl+S` | Save config |
| `Ctrl+Shift+S` | Save config as... |
| `←` | Previous image |
| `→` | Next image |
| `Ctrl+=` | Zoom in |
| `Ctrl+-` | Zoom out |
| `Ctrl+0` | Fit to screen |
| `Ctrl+1` | Actual size (100%) |
| `Delete` | Delete selected point |

### Build

**Prerequisites:** CMake >= 3.16, Qt 6, C++17 compiler

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

The executable will be in `bin/`.

---

### Coordinate System (Important for Integration)

#### Normalized Coordinate Formula

Mark normalizes by dividing by **(width - 1)** and **(height - 1)**, with a lower bound of 1 to avoid division by zero:

```
normalizedX = pixelX / max(1, imageWidth  - 1)
normalizedY = pixelY / max(1, imageHeight - 1)
```

This formula is used consistently across display, JSON export (`toJson`), and JSON import (`fromJson`).

**Example:** On a 1920x1080 image, the bottom-right pixel (1919, 1079) maps to exactly (1.0, 1.0). The center pixel (960, 540) maps to (0.500261, 0.500466).

#### Restoring Pixel Coordinates

To convert normalized coordinates back to pixel coordinates:

```
pixelX = round(normalizedX * max(1, imageWidth  - 1))
pixelY = round(normalizedY * max(1, imageHeight - 1))
```

**Notes:**

1. The denominator is `width - 1` / `height - 1` (clamped to minimum 1), **not** `width` / `height`. The first pixel (0, 0) maps to 0.0 and the last pixel maps to exactly 1.0.
2. Use `qRound()` (consistent with how Mark does it internally).
3. Normalized values are clamped to [0.0, 1.0] on import.
4. `imageWidth` and `imageHeight` are always saved in the JSON, so pixel positions can always be recovered.

#### C++ Example

```cpp
#include <cmath>
#include <algorithm>

// Convert Mark's normalized coordinates back to pixel coordinates
void normalizedToPixel(double normX, double normY,
                       int imgWidth, int imgHeight,
                       int &outPixelX, int &outPixelY)
{
    outPixelX = qRound(normX * std::max(1, imgWidth  - 1));
    outPixelY = qRound(normY * std::max(1, imgHeight - 1));
}

// Convert pixel coordinates to Mark's normalized coordinates
void pixelToNormalized(int pixelX, int pixelY,
                       int imgWidth, int imgHeight,
                       double &outNormX, double &outNormY)
{
    outNormX = static_cast<double>(pixelX) / std::max(1, imgWidth  - 1);
    outNormY = static_cast<double>(pixelY) / std::max(1, imgHeight - 1);
}

// Usage
int main()
{
    int imgWidth = 1920, imgHeight = 1080;

    // Normalized -> Pixel
    int px, py;
    normalizedToPixel(0.5, 0.5, imgWidth, imgHeight, px, py);
    // px = 960, py = 540

    // Pixel -> Normalized
    double nx, ny;
    pixelToNormalized(960, 540, imgWidth, imgHeight, nx, ny);
    // nx = 0.500261..., ny = 0.500466...

    return 0;
}
```

#### JSON Config Format

```json
{
    "xyFormat": "normalized",
    "colorFormat": "rgb",
    "imageWidth": 1920,
    "imageHeight": 1080,
    "normalizedDecimals": 6,
    "colorDecimals": 0,
    "configName": "my_annotation",
    "pointsListVisibleRows": 5,
    "points": [
        [0.500261, 0.500466, 255, 128, 0],
        [0.250130, 0.750233, 0, 200, 100]
    ]
}
```

**Point array layout by format:**

| xyFormat       | colorFormat | Array layout                      |
| -------------- | ----------- | --------------------------------- |
| `pixel`      | `rgb`     | `[x, y, r, g, b]`               |
| `normalized` | `rgb`     | `[normX, normY, r, g, b]`       |
| `pixel`      | `hex`     | `[x, y, "#rrggbb"]`             |
| `normalized` | `hsv`     | `[normX, normY, h_deg, s%, v%]` |
| `normalized` | `hsl`     | `[normX, normY, h_deg, s%, l%]` |
| `normalized` | `cmyk`    | `[normX, normY, c, m, y, k]`    |

HSV/HSL values: hue in degrees (0-359), saturation/value/lightness in percent (0-100). CMYK values are integers (0-255 range as returned by Qt).

---

`<a id="中文"></a>`

## 中文

### 概述

Mark 是一个基于 **Qt 6 / C++17** 构建的轻量级检测点标注工具。在图片上点击添加检测点，并以多种格式导出坐标和颜色。适用于计算机视觉工作流、标定数据采集和颜色取样。

### 功能特性

- **点标注** - 在图片上点击添加检测点
- **双坐标系统** - 像素坐标与归一化坐标 (0.0–1.0)
- **多种颜色格式** - RGB、HEX、HSL、HSV、CMYK
- **文件夹浏览** - 使用方向键在图片序列间导航
- **缩放** - 平滑缩放最高达 3200%，高倍缩放下像素级渲染
- **小地图** - 概览导航组件
- **拖拽打开** - 直接拖拽图片到窗口
- **JSON 导入/导出** - 带完整元数据的配置文件保存与加载
- **复制粘贴** - 复制坐标、颜色或图片文件到剪贴板
- **资源管理器集成** - 在资源管理器中打开或选中文件
- **可调精度** - 坐标和颜色值的小数位数可自定义
- **键盘快捷键** - 常用操作全面支持键盘快捷键

### 键盘快捷键

| 快捷键 | 操作 |
|---|---|
| `Ctrl+O` | 打开图片 |
| `Ctrl+Shift+O` | 打开文件夹 |
| `Ctrl+L` | 加载配置 |
| `Ctrl+S` | 保存配置 |
| `Ctrl+Shift+S` | 另存为... |
| `←` | 上一张图片 |
| `→` | 下一张图片 |
| `Ctrl+=` | 放大 |
| `Ctrl+-` | 缩小 |
| `Ctrl+0` | 适应屏幕 |
| `Ctrl+1` | 实际大小 (100%) |
| `Delete` | 删除选中检测点 |

### 构建

**前置条件：** CMake >= 3.16、Qt 6、C++17 编译器

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

可执行文件生成在 `bin/` 目录中。

---

### 坐标系统（对接必读）

#### 归一化坐标公式

Mark 使用 **(宽度 - 1)** 和 **(高度 - 1)** 作为分母，并限制下界为 1 以避免除零：

```
归一化X = 像素X / max(1, 图片宽度  - 1)
归一化Y = 像素Y / max(1, 图片高度 - 1)
```

该公式在界面显示、JSON 导出（`toJson`）和 JSON 导入（`fromJson`）中保持一致。

**示例：** 1920x1080 的图片上，右下角像素 (1919, 1079) 映射到恰好 (1.0, 1.0)。中心像素 (960, 540) 映射到 (0.500261, 0.500466)。

#### 从归一化坐标还原像素坐标

```
像素X = round(归一化X * max(1, 图片宽度  - 1))
像素Y = round(归一化Y * max(1, 图片高度 - 1))
```

**注意事项：**

1. 分母是 `width - 1` / `height - 1`（最小为 1），**不是** `width` / `height`。第一个像素 (0, 0) 映射到 0.0，最后一个像素映射到恰好 1.0。
2. 使用 `qRound()` 四舍五入（与 Mark 内部实现一致）。
3. 导入时归一化值会被限制在 [0.0, 1.0] 范围内。
4. `imageWidth` 和 `imageHeight` 始终保存在 JSON 中，因此总能还原精确像素位置。

#### C++ 示例

```cpp
#include <cmath>
#include <algorithm>

// 将 Mark 的归一化坐标转换回像素坐标
void normalizedToPixel(double normX, double normY,
                       int imgWidth, int imgHeight,
                       int &outPixelX, int &outPixelY)
{
    outPixelX = qRound(normX * std::max(1, imgWidth  - 1));
    outPixelY = qRound(normY * std::max(1, imgHeight - 1));
}

// 将像素坐标转换为 Mark 的归一化坐标
void pixelToNormalized(int pixelX, int pixelY,
                       int imgWidth, int imgHeight,
                       double &outNormX, double &outNormY)
{
    outNormX = static_cast<double>(pixelX) / std::max(1, imgWidth  - 1);
    outNormY = static_cast<double>(pixelY) / std::max(1, imgHeight - 1);
}

// 用法
int main()
{
    int imgWidth = 1920, imgHeight = 1080;

    // 归一化 -> 像素
    int px, py;
    normalizedToPixel(0.5, 0.5, imgWidth, imgHeight, px, py);
    // px = 960, py = 540

    // 像素 -> 归一化
    double nx, ny;
    pixelToNormalized(960, 540, imgWidth, imgHeight, nx, ny);
    // nx = 0.500261..., ny = 0.500466...

    return 0;
}
```

#### JSON 配置格式

```json
{
    "xyFormat": "normalized",
    "colorFormat": "rgb",
    "imageWidth": 1920,
    "imageHeight": 1080,
    "normalizedDecimals": 6,
    "colorDecimals": 0,
    "configName": "my_annotation",
    "pointsListVisibleRows": 5,
    "points": [
        [0.500261, 0.500466, 255, 128, 0],
        [0.250130, 0.750233, 0, 200, 100]
    ]
}
```

**不同格式下的点数组结构：**

| xyFormat       | colorFormat | 数组布局                           |
| -------------- | ----------- | ---------------------------------- |
| `pixel`      | `rgb`     | `[x, y, r, g, b]`                |
| `normalized` | `rgb`     | `[normX, normY, r, g, b]`        |
| `pixel`      | `hex`     | `[x, y, "#rrggbb"]`              |
| `normalized` | `hsv`     | `[normX, normY, h_度, s_%, v_%]` |
| `normalized` | `hsl`     | `[normX, normY, h_度, s_%, l_%]` |
| `normalized` | `cmyk`    | `[normX, normY, c, m, y, k]`     |

HSV/HSL 值：色相为度数 (0-359)，饱和度/明度/亮度为百分比 (0-100)。CMYK 值为整数（Qt 返回的 0-255 范围）。
