# CLAUDE.md — Layer 1: Permanent Rules

> **Loads every session. Does not remember anything between sessions.**
> These are immutable behavioral rules for Claude working on this project.

---

## Project Identity

**StudioFeel** is a system-wide audio equalizer for Windows.

- **APO Layer:** C++ COM Audio Processing Object (runs in audiodg.exe)
- **IPC Layer:** Named pipes for UI ↔ APO communication
- **UI Layer:** WinUI 3 companion app
- **Distribution:** Microsoft Hardware Support App (HSA) model

---

## Coding Standards

### C++ (APO)
- Use `std::atomic` for lock-free real-time safety
- **Never allocate memory in APOProcess** - pre-allocate in LockForProcess
- **No system calls in real-time path** - audiodg.exe is latency-sensitive
- Use biquad filters with normalized coefficients
- COM interfaces: `IAudioProcessingObject`, `IAudioProcessingObjectRT`, `IAudioSystemEffects2`

### IPC Protocol
- Message-based JSON over named pipes
- Pipe name: `StudioFeel.{endpointId}`
- Timeout: 1000ms for UI requests
- APO is server, UI is client

### UI (WinUI 3)
- MVVM pattern: ViewModels in `ViewModels/`, Views in `Pages/`
- Controls go in `Controls/`
- Converters for data binding in `Converters/`
- Async/await for all IPC calls

---

## Git Conventions

- **Main branch:** `main`
- **Feature branches:** `feature/feature-name`
- **Commit format:** `type(scope): description`
  - Types: `feat`, `fix`, `refactor`, `docs`, `test`, `perf`
  - Example: `feat(apo): add biquad filter chain`

---

## File Organization

```
StudioFeel/
├── APO/           # C++ audio processing (driver)
├── IPC/           # Communication protocol (shared)
├── UI/            # WinUI 3 companion app
├── Tests/         # Unit and integration tests
└── docs/          # Project documentation
```

---

## Development Priorities

1. **Real-time safety first** - APO must not glitch audio
2. **IPC reliability** - UI must handle APO unavailability
3. **Per-device persistence** - Settings store in endpoint property store
4. **Test coverage** - Especially for filter calculations

---

## What to Do When Starting Work

1. Read `primer.md` (Layer 2) for current project state
2. Run `memory.sh` (Layer 3) for git context
3. Check `docs/memory/` (Layer 4) for behavioral patterns
4. Ask if anything is unclear

---

## What to Do When Ending Work

1. Update `primer.md` with:
   - Active task completed
   - Exact next step
   - Any blockers encountered
2. Commit changes with descriptive message
3. Note any new behavioral patterns for Layer 4

---

## Prohibited Actions

- ❌ Adding emojis to code or documentation
- ❌ Creating unnecessary abstractions "for future use"
- ❌ Adding error handling for impossible scenarios
- ❌ Rewriting working code without a task
- ❌ Committing without user approval

---

## Communication Style

- Be direct and technical
- Show code references as `file.cpp:123`
- Ask when requirements are ambiguous
- Never guess - verify first

---
