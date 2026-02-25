# VolcEngineRTC ARM64 Desktop Demo (C++)

基于火山引擎实时音视频 (VolcEngineRTC) Linux ARM64 SDK 的 Qt 桌面端演示工程。支持最多 4 人同时音视频通话。

## 平台与架构

- **目标平台**: Linux aarch64 (ARM64)
- **SDK 版本**: 3.60.103.4700
- **UI 框架**: Qt5

## 环境要求

- Linux aarch64 (ARM64) 系统，在 Ubuntu 24.04 ARM64 Desktop 上测试通过
- GCC/G++ (支持 C++14)
- CMake 3.10+
- Qt5 开发库 (Widgets, Core, Gui, Network)
- OpenSSL 开发库 (用于本地生成 Token)
- PulseAudio 开发库 (音频采集/播放)

### 安装依赖 (Ubuntu/Debian)

```sh
sudo apt update
sudo apt install build-essential cmake qtbase5-dev qt5-qmake qtchooser \
    libssl-dev libpulse-dev
```

> 注意: 不同发行版的包名可能有差异，请根据实际情况调整。

## 配置

编辑 `sources/Constants.h`，填入您的火山引擎 RTC 应用信息：

```cpp
const std::string APP_ID = "your_app_id";
const std::string APP_KEY = "your_app_key";
```

## 编译

```sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

编译完成后，需要确保运行时能找到 SDK 动态库：

```sh
# 方式一：设置 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$(pwd)/../VolcEngineRTC_arm64/lib:$LD_LIBRARY_PATH
./QuickStart

# 方式二：将 .so 文件复制到可执行文件同级目录
cp ../VolcEngineRTC_arm64/lib/*.so .
./QuickStart
```

## 打包 (可选)

工程提供了 `archive` 目标，可将可执行文件与依赖库打包为 AppImage：

```sh
make archive
```

> 需要预先安装 `patchelf` 和 `linuxdeployqt`。

## 项目结构

```
physical-ai-demo-arm64-desktop-cpp/
├── CMakeLists.txt                  # 构建配置
├── sources/                        # 应用源码
│   ├── main.cpp                    # 程序入口
│   ├── Constants.h                 # APP_ID / APP_KEY 等常量配置
│   ├── RoomMainWidget.h/cpp        # 主窗口，管理 RTC 引擎与房间
│   ├── LoginWidget.h/cpp           # 登录界面 (输入房间号与用户名)
│   ├── OperateWidget.h/cpp         # 操作面板 (挂断、静音等)
│   ├── VideoWidget.h/cpp           # 视频渲染组件
│   └── TokenGenerator/             # Token 本地生成工具
├── ui/                             # Qt Designer UI 文件
├── VolcEngineRTC_arm64/            # ARM64 SDK
│   ├── include/                    # 头文件
│   └── lib/                        # 动态库 (.so)
└── Resources/                      # 资源文件
```

## 常见问题

### 找不到动态库

```
error while loading shared libraries: libVolcEngineRTC.so: cannot open shared object file
```

请通过 `LD_LIBRARY_PATH` 或 rpath 指定 SDK lib 目录，参见上方"运行"一节。

### 音频设备问题

确保 PulseAudio 服务正在运行：

```sh
pulseaudio --start
```
