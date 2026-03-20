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

## Building

### Quick Start

1. **Open the solution:** Double-click `StudioFeel.sln`
2. **Set startup project:** Right-click `StudioFeel.UI` → Set as Startup Project
3. **Build:** Press `Ctrl+Shift+B` or F5 to run

### What Gets Built

| Project | Output | Purpose |
|---------|--------|---------|
| `APO` | `StudioFeel.APO.dll` | The audio driver (EQ math) |
| `IPC` | `StudioFeel.IPC.dll` | Communication layer |
| `UI` | `StudioFeel.App.exe` | The app you see |

---

## Current Status

| Component | Status | Can Build? |
|-----------|--------|------------|
| DSP Engine (headers) | ✅ Complete | Yes |
| JSON Serialization | ✅ Complete | Yes |
| APO Implementation | ✅ Complete | Yes |
| IPC Implementation | ✅ Complete | Yes |
| Preset System | ✅ Complete | Yes |
| WinUI 3 App | ✅ Complete | Yes |
| Visualizer | ⚠️ Basic (no curve drawing yet) | Partial |
| Tests | ❌ Not started | No |

---

## What Works Right Now

✅ The UI opens and displays
✅ EQ sliders and controls
✅ Preset menu (with factory presets)
✅ Master gain control
✅ Power toggle

## What Doesn't Work Yet

❌ Actual audio processing (APO needs to be installed as a driver)
❌ Frequency curve visualization (needs Canvas drawing code)
❌ Device selection (just shows placeholder)
❌ Import/Export presets (menu items exist but not implemented)

---

## Testing Without APO Driver

You can still run the UI! It will work in "demo mode":
- Sliders move
- Presets load
- Visual display updates

The EQ just won't affect actual audio until the APO driver is installed.

---

## Installing the APO Driver (Advanced)

To make the EQ actually process audio, the APO DLL needs to be registered as a Windows audio effect.

**This requires:**
1. Building the APO project
2. Running the INF installer (`StudioFeel.inf`)
3. Restarting Windows Audio service

**Ask Claude for help with this step** — it's complex and can break audio if done wrong.

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

Then rebuild in Visual Studio.

---
