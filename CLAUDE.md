# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Qt5-based desktop video conferencing application for Linux ARM64 (aarch64) that uses the VolcEngineRTC SDK. It connects to an AI agent via MQTT, initiates a voice chat session, and uses server-returned parameters (appId, roomId, token, userId, targetUserId) to join the RTC room.

**Target Platform**: Linux aarch64 (ARM64)
**SDK Version**: VolcEngineRTC 3.60.103.4700
**UI Framework**: Qt5 (Widgets, Core, Gui, Network)
**MQTT Protocol**: Client-Agent message protocol (see specs/client_agent_message_protocol.md)
**Dependencies**: Paho MQTT C++, nlohmann/json, mcp-over-mqtt-cpp-sdk

## Build Commands

### Initial Build
```sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running the Application
The application requires the VolcEngineRTC SDK dynamic libraries at runtime:

```sh
# From build directory
export LD_LIBRARY_PATH=$(pwd)/../VolcEngineRTC_arm64/lib:$LD_LIBRARY_PATH
./QuickStart
```

### Creating AppImage Package
```sh
# From build directory
make archive
```
This creates an AppImage bundle with all dependencies. Requires `patchelf` and `linuxdeployqt`.

## Architecture

### Widget Hierarchy and Responsibilities

**RoomMainWidget** (sources/RoomMainWidget.h/cpp)
- Main window and application entry point
- Manages the RTC engine lifecycle (create, join room, leave, destroy)
- Implements both `IRTCRoomEventHandler` and `IRTCEngineEventHandler` interfaces
- Orchestrates video rendering across 4 video widget slots (1 local + 3 remote)
- Contains LoginWidget, OperateWidget, and AgentClient
- Handles user join/leave events and dynamically assigns remote users to available video slots

**AgentClient** (sources/AgentClient.h/cpp)
- MQTT client that implements the client-agent message protocol
- Connects to MQTT broker via Paho MQTT C++ (MQTT 5.0)
- Sends `initializeSession` and `startVoiceChat` JSON-RPC requests to the agent
- Parses agent responses to extract RTC room parameters (appId, roomId, token, userId, targetUserId)
- Handles agent notifications (`voiceChatStopped`, `destroySession`)
- Uses `MqttCallbackBridge` inner class to marshal Paho callbacks to Qt main thread via `QMetaObject::invokeMethod`
- Uses mcp-over-mqtt-cpp-sdk's `JsonRpcRequest` and `JsonRpcNotification` for message serialization

**LoginWidget** (sources/LoginWidget.h/cpp)
- Pre-call UI for entering MQTT configuration (Broker URL, Agent ID, Client ID)
- Validates that all fields are non-empty
- Emits `sigStartVoiceChat` signal to RoomMainWidget

**OperateWidget** (sources/OperateWidget.h/cpp)
- In-call control panel for mute/unmute audio/video and hangup
- Signals to RoomMainWidget to call SDK audio/video publish methods

**VideoWidget** (sources/VideoWidget.h/cpp)
- Reusable video rendering component
- Provides a QWidget whose `winId()` is passed to SDK as native render canvas
- Tracks active/inactive state for slot assignment

### MQTT + RTC Integration Flow

1. **User Input** (LoginWidget):
   - User enters MQTT Broker URL, Agent ID, and Client ID
   - Clicks "开始语音通话" button

2. **MQTT Connection** (`slotOnStartVoiceChat`):
   - Creates `AgentClient` and connects to MQTT broker
   - Subscribes to `$agent-client/{clientId}/#`

3. **Session Initialization** (AgentClient):
   - Sends `initializeSession` JSON-RPC request to `$agent/{agentId}/{clientId}`
   - Waits for success response

4. **Voice Chat Start** (AgentClient):
   - On session initialized, sends `startVoiceChat` request
   - Agent responds with: `appId`, `roomId`, `token`, `userId`, `targetUserId`
   - Emits `voiceChatReady` signal

5. **RTC Room Join** (`slotOnVoiceChatReady`):
   - Creates `IRTCEngine` with server-provided `appId`
   - Uses `targetUserId` as the local user's `userId` (important!)
   - Joins room with server-provided `token` and `roomId`
   - Starts local audio/video capture
6. **Remote User Handling**:
   - `onFirstRemoteVideoFrameDecoded()` triggers `sigUserEnter` signal
   - Signal handler finds first inactive VideoWidget from `m_videoWidgetList` (max 3)
   - Calls `setRemoteVideoCanvas()` with the widget's `winId()`
   - Maps user_id to VideoWidget in `m_activeWidgetMap` for cleanup on leave

7. **Audio/Video Controls**:
   - Mute audio: `IRTCRoom::publishStreamAudio(false)`
   - Mute video: `IRTCEngine::stopVideoCapture()`

8. **Cleanup** (`slotOnHangup`):
   - Sends `stopVoiceChat` and `destroySession` to agent via AgentClient
   - Disconnects MQTT
   - `IRTCRoom::leaveRoom()` and `destroy()`
   - `IRTCEngine::destroyRTCEngine()` (static method)
   - Clear all video widget mappings

### Configuration

RTC room parameters (appId, roomId, token, userId) are now obtained from the agent server via MQTT.
The user only needs to provide MQTT Broker URL, Agent ID, and Client ID in the login UI.

`Constants.h` and `TokenGenerator/` have been removed as they are no longer needed.

## SDK Integration Details

### Include Paths
```cmake
${BYTERTC_SDK_DIR}/include
${BYTERTC_SDK_DIR}/include/rtc
${BYTERTC_SDK_DIR}/include/game
```

### Linking
```cmake
target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt5::Widgets Qt5::Network
    VolcEngineRTC
    pulse-simple pulse atomic
    OpenSSL::Crypto
    mcp_mqtt_server
    PahoMqttCpp::paho-mqttpp3
)
```

The mcp-over-mqtt-cpp-sdk is included via `add_subdirectory(../mcp-over-mqtt-cpp-sdk)`.

## UI Files

Qt Designer .ui files in `ui/` are auto-processed by CMake:
- `RoomMainWidget.ui`: Main window layout (title bar, 4 video slots, SDK version label)
- `LoginWidget.ui`: MQTT configuration form (Broker URL, Agent ID, Client ID)
- `OperateWidget.ui`: In-call control buttons
- `VideoWidget.ui`: Single video rendering area

## Development Notes

### Video Slot Management
Only 4 simultaneous video renders are supported (SDK performance limit or demo design choice). Additional users joining beyond 3 remotes are ignored (see RoomMainWidget.cpp:240-242).

### Qt Frameless Window
RoomMainWidget uses `Qt::FramelessWindowHint` and implements custom mouse event handlers (mousePressEvent, mouseMoveEvent, mouseReleaseEvent) for draggable window behavior.

### Event Handler Threading
RTC callbacks (`onUserJoined`, `onFirstRemoteVideoFrameDecoded`, etc.) may occur on SDK threads. The code uses Qt signals to marshal back to the main thread for UI updates.

MQTT callbacks from Paho also arrive on internal threads. `AgentClient::MqttCallbackBridge` uses `QMetaObject::invokeMethod(Qt::QueuedConnection)` to forward messages to the Qt main thread.

### PulseAudio Dependency
The SDK uses PulseAudio for audio I/O. If audio capture/playback fails, verify `pulseaudio --start` is running.
