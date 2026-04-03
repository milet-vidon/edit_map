# PGM Map Editor for ROS / ROS 2

[中文说明](./README.zh-CN.md)

A lightweight Qt desktop application for creating and editing occupancy-grid maps in `PGM + YAML` format.

This project is intended for ROS map workflows. It is **not** a ROS node itself, but the exported map files follow the standard occupancy-grid map format commonly used by:

- ROS 1 `map_server`
- ROS 2 `nav2_map_server` / `map_server`

## Features

- Open existing `PGM` maps
- Edit maps with obstacle / free / unknown brushes
- Create a new blank map with custom width and height
- Configure map resolution when creating a new map
- Zoom and grid overlay for pixel-level editing
- Undo support
- Export `PGM + YAML`
- Works with either Qt5 or Qt6

## Preview

![Map preview](./docs/images/map-preview.svg)

## ROS / ROS 2 Compatibility

This repository should be described as:

`A Qt-based occupancy-grid map editor that exports ROS / ROS 2 compatible PGM + YAML maps.`

Important:

- This project currently builds as a standalone Qt application
- It does not require ROS to compile
- It does not provide a ROS 1 package or ROS 2 package yet
- The compatibility claim refers to the exported map file format

So the correct statement is:

`Compatible with ROS 1 and ROS 2 map file workflows`

Instead of:

`This is a native ROS / ROS 2 package`

unless you later add `package.xml`, `CMakeLists.txt` for catkin/ament integration, launch files, and install rules.

## Project Structure

```text
.
├── CMakeLists.txt
├── main.cpp
└── README.md
```

## Build Requirements

- CMake 3.10+
- A C++17 compiler
- Qt Widgets and Qt Gui
- Qt5 or Qt6

On Ubuntu, install one of the following:

### Option 1: Qt5

```bash
sudo apt update
sudo apt install -y build-essential cmake qtbase5-dev
```

### Option 2: Qt6

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev
```

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/map_editor
```

## Usage

### Create a New Map

1. Click `New Map` or press `Ctrl+N`
2. Enter:
   - map width in pixels
   - map height in pixels
   - resolution in meters per pixel
   - initial fill value
3. Click `OK`
4. Edit the map
5. Use `Save PGM As` to export the map files

### Edit an Existing Map

1. Click `Open PGM` or press `Ctrl+O`
2. Paint with the selected brush
3. Save with `Ctrl+S`

### Brush Values

- `Obstacle = 0`
- `Free = 254`
- `Unknown = 205`

### Shortcuts

- `Ctrl+N`: Create a new map
- `Ctrl+O`: Open a map
- `Ctrl+S`: Save map as
- `Ctrl+Z`: Undo
- `1`: Obstacle brush
- `2`: Free brush
- `3`: Unknown brush
- `G`: Toggle grid

## Output Format

The editor saves:

- `your_map.pgm`
- `your_map.yaml`

Example YAML:

```yaml
image: your_map.pgm
resolution: 0.010
origin: [0.0, 0.0, 0.0]
negate: 0
occupied_thresh: 0.65
free_thresh: 0.196
mode: trinary
```

## Using the Exported Map in ROS 1

The generated map files follow the standard map YAML format used by ROS 1 map workflows.

Typical usage:

```bash
map_server your_map.yaml
```

In many ROS 1 setups, you may also launch the map through your own launch file instead of calling the executable directly.

## Using the Exported Map in ROS 2

The generated map files are also suitable for ROS 2 navigation map workflows.

Typical usage is to point `nav2_map_server` at a parameter file that contains the exported map YAML path.

Example parameter file:

```yaml
map_server:
  ros__parameters:
    yaml_filename: "/absolute/path/to/your_map.yaml"
```

Then run:

```bash
ros2 run nav2_map_server map_server __params:=map_server_params.yaml
```

In real projects, it is more common to provide the map YAML path through a Nav2 launch file or parameter file.


## Roadmap

- Add map metadata editing UI
- Add metric-size input mode in meters
- Add fill / line / rectangle tools
- Add ROS package wrappers for ROS 1 and ROS 2
- Add screenshot examples to the README

## Disclaimer

This tool edits standard occupancy-grid map files. Compatibility depends on the target ROS stack accepting standard `PGM + YAML` occupancy maps and the expected metadata fields.
