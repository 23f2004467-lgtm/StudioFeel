# BUILD.md — Build Instructions for Windows

> For the friend helping test StudioFeel. You need Visual Studio 2022.

---

## Prerequisites

### 1. Install Visual Studio 2022 (Free)

Download **Community Edition**: https://visualstudio.microsoft.com/downloads/

During installation, select these workloads:
- ✅ **Desktop development with C++**
- ✅ **.NET desktop development** (for WinUI 3 app)

### 2. Install Windows App SDK

Download: https://aka.ms/windowsappsdk/1.6.0/windowsappruntimeinstall-x64.exe

Or via Visual Studio Installer → Individual Components → search "Windows App SDK"

### 3. Clone the Repo

```powershell
git clone https://github.com/23f2004467-lgtm/StudioFeel.git
cd StudioFeel
```

---

## Project Structure

```
StudioFeel/
├── APO/           # C++ audio driver (build as DLL)
├── IPC/           # Shared communication code
├── UI/            # WinUI 3 app
├── Tests/         # Unit tests
└── docs/          # Documentation
```

---

## Building the APO (C++ DLL)

**Note**: The APO cannot be built yet. Missing implementation files:
- `IPC/src/NamedPipeIPC.cpp` — Not implemented
- `IPC/src/PresetFormat.cpp` — Not implemented

Ask Claude (the AI) to implement these first.

---

## Current Status

| Component | Status | Can Build? |
|-----------|--------|------------|
| DSP Engine (headers) | ✅ Complete | Yes |
| JSON Serialization | ✅ Complete | Yes |
| APO Implementation | ✅ Complete | Yes |
| IPC Implementation | ❌ Missing | No |
| Preset System | ❌ Missing | No |
| WinUI 3 App | ❌ Empty | No |

---

## What to Tell Claude (Your Friend)

**Copy and paste this prompt to Claude:**

```
I'm helping test StudioFeel on Windows.

What's the current status of the project?
What needs to be implemented next?
Can you implement the missing IPC layer (NamedPipeIPC.cpp)?
```

---

## For the Developer (Mac User)

When you make changes on Mac:
```bash
git add -A
git commit -m "your message"
git push
```

Friend on Windows pulls:
```bash
git pull
```

---
