# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Qt5-based desktop video conferencing application for Linux ARM64 (aarch64) that uses the VolcEngineRTC SDK. It supports up to 4 simultaneous video participants (1 local + 3 remote).

**Target Platform**: Linux aarch64 (ARM64)
**SDK Version**: VolcEngineRTC 3.60.103.4700
**UI Framework**: Qt5 (Widgets, Core, Gui, Network)

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
- Contains LoginWidget and OperateWidget as child widgets
- Handles user join/leave events and dynamically assigns remote users to available video slots

**LoginWidget** (sources/LoginWidget.h/cpp)
- Pre-call UI for entering room ID and user ID
- Validates input against SDK constraints (alphanumeric + `@._-`, max 128 chars)
- Emits signal to RoomMainWidget to initiate room join

**OperateWidget** (sources/OperateWidget.h/cpp)
- In-call control panel for mute/unmute audio/video and hangup
- Signals to RoomMainWidget to call SDK audio/video publish methods

**VideoWidget** (sources/VideoWidget.h/cpp)
- Reusable video rendering component
- Provides a QWidget whose `winId()` is passed to SDK as native render canvas
- Tracks active/inactive state for slot assignment

### RTC Engine Integration Flow

1. **Engine Creation** (`slotOnEnterRoom`):
   - Create `IRTCEngine` with app_id from Constants.h
   - Set video encoder config (360x640@15fps)
   - Start local audio/video capture
   - Set local video canvas to `ui.localWidget`

2. **Room Join**:
   - Create `IRTCRoom` via `createRTCRoom()`
   - Generate token locally using `TokenGenerator::generate()` (app_id + app_key + room_id + user_id)
   - Join with auto-publish/subscribe enabled

3. **Remote User Handling**:
   - `onFirstRemoteVideoFrameDecoded()` triggers `sigUserEnter` signal
   - Signal handler finds first inactive VideoWidget from `m_videoWidgetList` (max 3)
   - Calls `setRemoteVideoCanvas()` with the widget's `winId()`
   - Maps user_id to VideoWidget in `m_activeWidgetMap` for cleanup on leave

4. **Audio/Video Controls**:
   - Mute audio: `IRTCRoom::publishStreamAudio(false)`
   - Mute video: `IRTCEngine::stopVideoCapture()`

5. **Cleanup** (`slotOnHangup`):
   - `IRTCRoom::leaveRoom()` and `destroy()`
   - `IRTCEngine::destroyRTCEngine()` (static method)
   - Clear all video widget mappings

### Token Generation

The `TokenGenerator` (sources/TokenGenerator/) generates RTC authentication tokens **locally** using OpenSSL. This is for demo purposes only.

**Production deployment must generate tokens server-side** to protect the APP_KEY.

### Configuration

**sources/Constants.h** contains:
- `APP_ID`: VolcEngineRTC application identifier
- `APP_KEY`: Secret key for token generation (hardcoded - change for production)
- `INPUT_REGEX`: Validation pattern for room/user IDs

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
    Qt5::Widgets
    Qt5::Network
    VolcEngineRTC
    pulse-simple pulse
    atomic
    OpenSSL::Crypto
)
```

The SDK library path is `VolcEngineRTC_arm64/lib/libVolcEngineRTC.so`. The CMakeLists.txt sets rpath to `$ORIGIN` so .so files can be placed alongside the executable.

## UI Files

Qt Designer .ui files in `ui/` are auto-processed by CMake:
- `RoomMainWidget.ui`: Main window layout (title bar, 4 video slots, SDK version label)
- `LoginWidget.ui`: Room/user input form
- `OperateWidget.ui`: In-call control buttons
- `VideoWidget.ui`: Single video rendering area

## Development Notes

### Video Slot Management
Only 4 simultaneous video renders are supported (SDK performance limit or demo design choice). Additional users joining beyond 3 remotes are ignored (see RoomMainWidget.cpp:240-242).

### Qt Frameless Window
RoomMainWidget uses `Qt::FramelessWindowHint` and implements custom mouse event handlers (mousePressEvent, mouseMoveEvent, mouseReleaseEvent) for draggable window behavior.

### Event Handler Threading
RTC callbacks (`onUserJoined`, `onFirstRemoteVideoFrameDecoded`, etc.) may occur on SDK threads. The code uses Qt signals to marshal back to the main thread for UI updates.

### PulseAudio Dependency
The SDK uses PulseAudio for audio I/O. If audio capture/playback fails, verify `pulseaudio --start` is running.
