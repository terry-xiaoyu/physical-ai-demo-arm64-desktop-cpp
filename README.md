# VolcEngineRTC ARM64 Desktop Demo (C++)

基于火山引擎实时音视频 (VolcEngineRTC) Linux ARM64 SDK 的 Qt 桌面端演示工程。通过 MQTT 协议与智能体交互，发起语音会话并自动加入 RTC 房间。

## 平台与架构

- **目标平台**: Linux aarch64 (ARM64)
- **SDK 版本**: 3.60.103.4700
- **UI 框架**: Qt5

## 环境要求

- Linux aarch64 (ARM64) 系统，在 Ubuntu 24.04 ARM64 Desktop 上测试通过
- GCC/G++ (支持 C++17)
- CMake 3.14+
- Qt5 开发库 (Widgets, Core, Gui, Network)
- OpenSSL 开发库
- PulseAudio 开发库 (音频采集/播放)
- nlohmann/json (>= 3.9)
- Eclipse Paho MQTT C 库
- Eclipse Paho MQTT C++ 库
- mcp-over-mqtt-cpp-sdk（已包含在同级目录 `../mcp-over-mqtt-cpp-sdk`）

### 安装依赖 (Ubuntu/Debian)

#### 1. 系统基础依赖

```sh
sudo apt update
sudo apt install build-essential cmake git qtbase5-dev qt5-qmake qtchooser \
    libssl-dev libpulse-dev nlohmann-json3-dev
```

#### 2. 安装 Paho MQTT C 库

```sh
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..
```

#### 3. 安装 Paho MQTT C++ 库

```sh
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake .. -DPAHO_WITH_SSL=ON -DPAHO_BUILD_SHARED=ON
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..
```

#### 4. 获取 mcp-over-mqtt-cpp-sdk

将 mcp-over-mqtt-cpp-sdk 放在与本项目同级的目录下：

```sh
# 确保目录结构如下：
# parent_dir/
# ├── physical-ai-demo-arm64-desktop-cpp/   (本项目)
# └── mcp-over-mqtt-cpp-sdk/                (SDK)

git clone https://github.com/terry-xiaoyu/mcp-over-mqtt-cpp-sdk.git
```

> 注意: 不同发行版的包名可能有差异，请根据实际情况调整。如果系统包仓库中没有 `nlohmann-json3-dev`，也可以从源码安装：
> ```sh
> git clone https://github.com/nlohmann/json.git
> cd json && mkdir build && cd build
> cmake .. && sudo make install
> ```

#### 5. 获取 VolcEngineRTC SDK

从 https://www.volcengine.com/docs/6348/75707?lang=zh 或者火山技术团队获取 SDK 压缩包: VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip，然后解压到本项目根目录下：

```sh
unzip -q VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release.zip
mv VolcEngineRTC_Linux_3.60.103.4700_aarch64_Release VolcEngineRTC_arm64
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

## 使用说明

启动应用后，在登录界面填写以下信息：

1. **MQTT Broker 地址** — 如 `tcp://your-broker:1883`
2. **Agent ID** — 智能体 ID
3. **Client ID** — 客户端 ID（也用作 MQTT Client ID）

点击"开始语音通话"后，应用会自动：
1. 连接到 MQTT Broker
2. 向智能体发送 `initializeSession` 初始化会话
3. 发送 `startVoiceChat` 发起语音通话
4. 从智能体的应答中获取 `appId`、`roomId`、`token`、`userId`、`targetUserId`
5. 使用 `targetUserId` 作为本端用户 ID 加入 RTC 房间

挂断时，应用会发送 `stopVoiceChat` 和 `destroySession` 给智能体，并断开 MQTT 连接。

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
│   ├── AgentClient.h/cpp           # MQTT 智能体客户端（协议交互）
│   ├── Constants.h                 # 常量配置
│   ├── RoomMainWidget.h/cpp        # 主窗口，管理 RTC 引擎与房间
│   ├── LoginWidget.h/cpp           # 登录界面（MQTT 配置输入）
│   ├── OperateWidget.h/cpp         # 操作面板（挂断、静音等）
│   ├── VideoWidget.h/cpp           # 视频渲染组件
│   └── TokenGenerator/             # Token 本地生成工具（备用）
├── ui/                             # Qt Designer UI 文件
├── VolcEngineRTC_arm64/            # ARM64 SDK (不包含在代码库中，自行下载并放置)
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

### 找不到 PahoMqttCpp

如果 cmake 报 `Could not find a package configuration file provided by "PahoMqttCpp"`，请确保已按上述步骤安装 Paho MQTT C 和 C++ 库，并执行了 `sudo ldconfig`。如果安装到了非默认路径，可通过 `CMAKE_PREFIX_PATH` 指定：

```sh
cmake .. -DCMAKE_PREFIX_PATH=/usr/local
```

### 找不到 nlohmann/json

如果 cmake 报 `Could not find a package configuration file provided by "nlohmann_json"`，请确保已安装 `nlohmann-json3-dev` 包或从源码安装。
