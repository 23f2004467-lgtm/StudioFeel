# docs/memory/ — Layer 4: Self-Learning Agent Memory

> **Extracts behavioral patterns from past sessions.**
> Changes how Claude responds in future ones.
> Not retrieval — actual learning.

---

## Purpose

This directory stores behavioral patterns learned from sessions. When a pattern is identified, it's documented here. Future sessions read these patterns and adapt responses accordingly.

---

## Pattern Categories

### `/user-preferences/`
How the user likes to work.

```
user-preferences/
├── communication-style.md      # terse vs verbose, examples preferred
├── code-style.md               # naming, formatting, patterns
├── decision-making.md          # user decides vs Claude decides
└── error-handling.md           # how to handle failures
```

### `/project-patterns/`
Recurring patterns in this specific project.

```
project-patterns/
├── common-mistakes.md          # errors we keep making
├── successful-patterns.md      # approaches that work well
├── architecture-decisions.md   # why we built it this way
└── context-gaps.md             # things Claude keeps missing
```

### `/session-summaries/`
One file per session, extracts learnings.

```
session-summaries/
├── 2026-03-21.md               # first session
└── YYYY-MM-DD.md               # future sessions
```

---

## How to Use

### At Session Start

Claude should:
1. Read all files in `user-preferences/`
2. Read all files in `project-patterns/`
3. Read the last 3 `session-summaries/`

### During Session

Claude should:
1. Note patterns in user behavior
2. Track what approaches work vs fail
3. Document decisions and their rationale

### At Session End

Claude should:
1. Create `session-summaries/YYYY-MM-DD.md`
2. Update any pattern files that changed
3. Note new patterns for next time

---

## Example Pattern Entry

### `user-preferences/communication-style.md`

```markdown
# Communication Style

## Observed Patterns

- Prefers code examples over explanations
- Wants file paths with line numbers: `file.cpp:123`
- Asks "why" when uncertain about approach
- Likes parallel tool use for speed

## Adaptation

- Show code first, explain briefly
- Always reference file locations
- Provide rationale for decisions
- Use parallel tool calls when possible
```

---

## Current State

As of 2026-03-21, this is a fresh system. No patterns learned yet.

---
