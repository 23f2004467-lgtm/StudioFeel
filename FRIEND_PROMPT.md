# FRIEND_PROMPT.md — Give This to Claude

> Copy the text below and paste it into Claude on your Windows laptop.

---

**PROMPT START**

```
I'm helping test StudioFeel on Windows. Here's the context:

PROJECT: StudioFeel — System-wide audio equalizer for Windows
REPO: https://github.com/23f2004467-lgtm/StudioFeel

I'm on Windows with Visual Studio 2022. My friend (the main developer) is on Mac and can't build/test.

Please:
1. Analyze the current codebase
2. Tell me what's implemented vs missing
3. Tell me what needs to be done next
4. Help me implement the missing pieces so we can test

Start by exploring the project structure and giving me a status report.
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
