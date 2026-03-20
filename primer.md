# primer.md — Layer 2: Project State

> **Rewrites itself at the end of every session.**
> Always current. Active project, last completed, exact next step, open blockers.

---

## Session Metadata

| Field | Value |
|-------|-------|
| **Last Updated** | 2026-03-21 |
| **Session Count** | 1 |
| **Current Branch** | (not a git repo yet) |

---

## Active Project

**StudioFeel** — System-wide audio equalizer for Windows

### Project Status

| Component | Status | Completeness |
|-----------|--------|--------------|
| APO (C++) | Skeleton defined | 15% |
| IPC | Headers defined, partial impl | 20% |
| UI (WinUI 3) | Folders empty, manifest done | 5% |
| Tests | Empty | 0% |
| Docs | Empty | 0% |

---

## Last Completed (2026-03-21)

1. **Initial project exploration** — Analyzed StudioFeel codebase structure
2. **Created 5-layer memory system** — Set up CLAUDE.md, primer.md, memory.sh, agent memory, and Obsidian integration
3. **Set up project skeleton** — Created organizational template

---

## Exact Next Step

**Implement APO DSP engine** — The core audio processing component

Starting file: `StudioFeel/APO/src/DSPEngine.h`

Tasks:
1. Complete biquad filter implementation (`BiquadFilter.h`)
2. Implement filter chain in `DSPEngine.h`
3. Add coefficient calculation functions
4. Ensure real-time safety (no allocations in process path)

---

## Open Blockers

| ID | Description | Priority | Assigned To |
|----|-------------|----------|-------------|
| #1 | Project not yet a git repository | High | — |
| #2 | Windows development environment not verified | High | — |
| #3 | APO testing strategy not defined | Medium | — |

---

## Dependencies

| Dependency | Version | Required For | Status |
|------------|---------|--------------|--------|
| Windows SDK | 10.0.26100.0 | APO development | Not installed |
| Visual Studio | 2022 | C++ compilation | Not verified |
| Windows App SDK | 1.6 | WinUI 3 | Not installed |

---

## Notes

- This is the first session with the 5-layer memory system
- No git history yet — initialize repo before next session
- Consider setting up CI/CD for Windows builds

---
