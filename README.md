# PCLite

PCLite 是一个面向大规模点云的轻量级测量与可视化应用，使用 C++20 + OpenGL + SDL2 构建。

## 项目目标

主流点云格式（LAS、PCD 等）不支持分块加载，当点云规模很大时，整个文件无法一次性载入内存，也就无法实现可视化和后续处理。PCLite 围绕这一问题展开，目标是：

- **大点云支持**：自研支持分级/分块加载的内部数据格式（PCLite 格式，类似 Potree）
- **点云可视化**：基于 OpenGL 的实时渲染，支持按屏幕空间误差动态加载/剔除节点（LOD）
- **实时点云测量**：在三维场景中进行距离、角度、面积、体积等交互式测量（规划中）
- **高性能**：C++20 + 多线程 I/O，控制内存占用与磁盘吞吐

## 架构概览

```
原始点云 (LAS/PCD/...)
      │  converter：分块 → 建立层级骨架 → 按分块采样 → 跨分块合并
      ▼
PCLite 格式数据集 (metadata.json + hierarchy.bin + 点数据)
      │  viewer：按需加载 → 动态 LOD（加载/剔除） → OpenGL 渲染
      ▼
交互式可视化 + 点云测量（measurement，规划中）
```

| 模块 | 路径 | 职责 | 现状 |
|---|---|---|---|
| core | `src/core` | 基础数据结构：vec3/mat、BoundingBox、Attributes、Node | 已实现 |
| converter | `src/converter` | 通用点云格式 → PCLite 分块格式 | 设计完成，未接入主构建 |
| viewer | `src/viewer` | 窗口/相机/图层/节点管理/绘制 | 基本实现，有测试覆盖 |
| measurement | `src/measurement` | 点云测量（距离/角度/面积/体积等） | 规划中 |
| utilities | `src/utilities` | 线程池等基础设施 | 已实现 |

完整设计见 [docs/zh/总体设计.md](docs/zh/总体设计.md)。

## 目录结构

```
PCLite/
├── CMakeLists.txt / CMakePresets.json   顶层构建配置
├── vcpkg.json                            vcpkg 依赖声明（sdl2）
├── main.cpp                              主程序入口（模板代码）
├── 3rd_party/                            第三方依赖（FetchContent 配置 + vendored ThreadPool）
├── cmake/                                CMake 工具模块
├── src/                                  core / converter / viewer / measurement / utilities
├── tools/pcl_viewer.cpp                  独立点云查看器工具
├── test_data/                            集成测试数据
└── docs/zh/                              中文设计文档
```

## 构建

### 依赖

- CMake ≥ 3.20，支持 C++20 的编译器（MSVC / GCC / Clang）
- [vcpkg](https://github.com/microsoft/vcpkg)（需设置 `VCPKG_ROOT` 环境变量），用于安装 SDL2
- OpenGL

其余依赖（nlohmann/json、spdlog、googletest、ThreadPool）通过 CMake `FetchContent` 自动下载，无需手动安装。

### 使用 CMake Presets

```bash
# Linux
cmake --preset linux-Debug
cmake --build --preset linux-Debug

# Windows
cmake --preset x64-Debug
cmake --build --preset x64-Debug
```

## 运行

`pcl_viewer` 是独立的点云查看器，打开一个 PCLite 格式数据集（包含 `metadata.json`/`hierarchy.bin`/点数据文件）：

```bash
./pcl_viewer <dataset_dir>
```

支持鼠标左键旋转、右键平移、滚轮缩放（Arcball 相机）。

## 测试

测试基于 GoogleTest + CTest，分为不需要 GL 上下文的单元测试和读取 `test_data/` 的集成测试，详见 [docs/zh/viewer测试文档.md](docs/zh/viewer测试文档.md)。

```bash
cmake --build <build_dir> --target viewer_unit_test viewer_integration_test -j4
cd <build_dir> && ctest --output-on-failure
```

## 文档索引

- [总体设计文档](docs/zh/总体设计.md) —— 项目目标、架构、技术选型、测试策略、路线图
- [convert 流程设计](docs/zh/convert流程.md) —— PCLite 格式与转换流水线
- [visualize 流程设计](docs/zh/visualize流程.md) —— viewer 应用分层与类设计
- [相机控制器设计](docs/zh/相机控制器设计.md) —— Arcball / Turntable / FirstPerson
- [viewer 测试文档](docs/zh/viewer测试文档.md) —— 单元/集成测试设计

## 项目状态

- ✅ core 基础数据结构、viewer 主体功能（窗口/相机/动态加载剔除/绘制）及测试
- 🚧 converter：格式与流水线设计已完成，代码待按设计重写并接入主构建
- 📋 measurement：尚未开始
