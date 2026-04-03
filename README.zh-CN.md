# PGM Map Editor for ROS / ROS 2

[English README](./README.md)

一个轻量级的 Qt 桌面应用，用于创建和编辑 `PGM + YAML` 格式的占据栅格地图。

这个项目面向 ROS 地图工作流，但它本身**不是** ROS 节点。它导出的地图文件遵循常见的占据栅格地图格式，可直接用于：

- ROS 1 `map_server`
- ROS 2 `nav2_map_server` / `map_server`

## 功能特性

- 打开已有的 `PGM` 地图
- 使用障碍物 / 空闲 / 未知三种画笔编辑地图
- 新建指定宽高的空白地图
- 新建地图时设置分辨率
- 支持缩放和网格叠加，便于逐像素编辑
- 支持撤销
- 导出 `PGM + YAML`
- 同时兼容 Qt5 和 Qt6

## ROS / ROS 2 兼容性说明

建议你在仓库介绍里这样描述本项目：

`A Qt-based occupancy-grid map editor that exports ROS / ROS 2 compatible PGM + YAML maps.`

重点要说明清楚：

- 当前项目是一个独立的 Qt 桌面程序
- 编译时不依赖 ROS
- 目前还不是 ROS 1 包，也不是 ROS 2 包
- 所谓兼容 ROS / ROS 2，指的是导出的地图文件格式兼容相关工作流

因此更准确的说法是：

`Compatible with ROS 1 and ROS 2 map file workflows`

而不是：

`This is a native ROS / ROS 2 package`

除非你后续补上 `package.xml`、catkin/ament 构建配置、launch 文件和安装规则。

## 项目结构

```text
.
├── CMakeLists.txt
├── main.cpp
├── README.md
└── README.zh-CN.md
```

## 构建要求

- CMake 3.10+
- 支持 C++17 的编译器
- Qt Widgets 和 Qt Gui
- Qt5 或 Qt6

在 Ubuntu 上可安装以下依赖之一：

### 方案 1：Qt5

```bash
sudo apt update
sudo apt install -y build-essential cmake qtbase5-dev
```

### 方案 2：Qt6

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev
```

## 编译

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

```bash
./build/map_editor
```

## 使用方法

### 新建地图

1. 点击 `New Map` 或按 `Ctrl+N`
2. 输入以下参数：
   - 地图宽度，单位像素
   - 地图高度，单位像素
   - 分辨率，单位米/像素
   - 初始填充值
3. 点击 `OK`
4. 编辑地图
5. 使用 `Save PGM As` 导出地图文件

### 编辑已有地图

1. 点击 `Open PGM` 或按 `Ctrl+O`
2. 使用当前选中的画笔进行绘制
3. 按 `Ctrl+S` 保存

### 画笔数值

- `Obstacle = 0`
- `Free = 254`
- `Unknown = 205`

### 快捷键

- `Ctrl+N`：新建地图
- `Ctrl+O`：打开地图
- `Ctrl+S`：另存为地图
- `Ctrl+Z`：撤销
- `1`：障碍物画笔
- `2`：空闲画笔
- `3`：未知画笔
- `G`：显示/隐藏网格

## 导出格式

程序会保存：

- `your_map.pgm`
- `your_map.yaml`

YAML 示例：

```yaml
image: your_map.pgm
resolution: 0.010
origin: [0.0, 0.0, 0.0]
negate: 0
occupied_thresh: 0.65
free_thresh: 0.196
mode: trinary
```

## 在 ROS 1 中使用导出的地图

生成的地图文件遵循 ROS 1 常见地图工作流使用的标准 YAML 格式。

典型用法：

```bash
map_server your_map.yaml
```

在很多 ROS 1 项目中，也可以通过你自己的 launch 文件来加载地图，而不是直接手动调用命令。

## 在 ROS 2 中使用导出的地图

生成的地图文件同样可用于 ROS 2 导航地图工作流。

常见做法是给 `nav2_map_server` 提供一个参数文件，在参数文件里指定导出的地图 YAML 路径。

示例参数文件：

```yaml
map_server:
  ros__parameters:
    yaml_filename: "/absolute/path/to/your_map.yaml"
```

然后执行：

```bash
ros2 run nav2_map_server map_server __params:=map_server_params.yaml
```

在实际项目中，更常见的做法是通过 Nav2 的 launch 文件或参数文件统一传入地图路径。

## GitHub 仓库信息建议

### 建议的仓库名

- `pgm-map-editor`
- `ros-pgm-map-editor`
- `qt-map-editor-ros`

### 建议的仓库描述

`Qt-based occupancy-grid map editor for ROS / ROS 2 PGM + YAML map workflows`

### 建议添加的 topics

- `ros`
- `ros2`
- `pgm`
- `yaml`
- `map-editor`
- `occupancy-grid`
- `navigation`
- `qt`
- `qt5`
- `qt6`

## 公开前建议补充的内容

建议在仓库公开前再加上：

- `LICENSE` 文件
- 几张界面截图或一个演示 GIF
- 简单的更新日志
- 如 `v1.0.0` 这样的 release 标签

如果没有许可证文件，仓库虽然可见，但从严格意义上说并不能算可自由复用的开源项目。

## 许可证建议

如果你希望别人更容易使用、修改和二次分发，`MIT` 是最常见、最简洁的选择。

如果你希望许可证里包含更明确的专利条款，可以考虑 `Apache-2.0`。

建议你在公开前明确选择一种许可证。

## 后续规划

- 增加地图元数据编辑界面
- 增加按实际米数输入地图尺寸的模式
- 增加填充、直线、矩形等工具
- 增加 ROS 1 / ROS 2 包装结构
- 在 README 中加入截图示例

## 免责声明

本工具编辑的是标准占据栅格地图文件。最终兼容性取决于目标 ROS 系统是否接受标准 `PGM + YAML` 地图，以及相关元数据字段是否符合其预期。
