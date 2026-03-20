# FRIEND_PROMPT.md — Give This to Claude

> Copy the text below and paste it into Claude on your Windows laptop.

---

**PROMPT START**

```
I'm helping test StudioFeel on Windows. Here's the context:

PROJECT: StudioFeel — System-wide audio equalizer for Windows
REPO: https://github.com/23f2004467-lgtm/StudioFeel

Current status (as of latest commit):
- IPC layer: ✅ Complete (NamedPipeIPC.cpp, IPCInterface.cpp)
- Preset system: ✅ Complete (PresetFormat.cpp)
- APO implementation: ✅ Complete (C++ audio driver)
- WinUI 3 UI: ✅ Complete (Main page with EQ controls)
- Visualizer: ⚠️ Basic (no curve drawing yet)
- Tests: ❌ Not started

I'm on Windows with Visual Studio 2022. My friend (the main developer) is on Mac and can't build/test.

What's the current status?
What should I build/test first?
```

**PROMPT END**

---

## Additional Prompts (use as needed)

### To build the project:
```
Help me build StudioFeel in Visual Studio 2022. I have the repo cloned locally.
Walk me through creating a solution file and building the APO DLL.
```

### To implement missing features:
```
Implement the missing IPC layer:
- IPC/src/NamedPipeIPC.cpp
- IPC/src/PresetFormat.cpp
- IPC/src/IPCInterface.cpp

Follow the patterns in the existing headers. This is Windows-only code using named pipes.
```

### To run tests:
```
Help me set up and run the tests in the Tests/ folder.
```

---
