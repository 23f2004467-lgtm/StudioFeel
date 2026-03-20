# OBSIDIAN.md — Layer 5: Vault Integration

> **Your entire knowledge base becomes Claude's working directory.**
> Every note you have ever written is now context.

---

## Purpose

Layer 5 connects your Obsidian vault to Claude. When enabled, Claude can read and reference all your notes, documentation, and knowledge during sessions.

---

## Setup

### 1. Create a Symlink to Your Vault

```bash
# Replace this path with your actual Obsidian vault location
ln -s ~/Path/To/Your/Obsidian/Vault ./vault
```

### 2. Configure Vault Structure

Ensure your vault has these optional (but recommended) folders:

```
vault/
├── _Claude/                    # Claude-specific notes
│   ├── prompts/               # Reusable prompt templates
│   ├── snippets/              # Code snippets by language
│   └── contexts/              # Project-specific context
├── _Projects/                 # Project documentation
├── _Reference/                # Reference material
└── [your other folders]
```

### 3. Add to .gitignore

```bash
echo "vault" >> .gitignore
```

Your personal notes should not be in the project repository.

---

## Usage

### For Claude (at session start)

When starting a session, Claude should:

1. Check if `./vault` exists
2. If yes, scan `_Claude/contexts/` for project-relevant notes
3. Scan `_Claude/prompts/` for relevant prompt templates
4. Reference vault content when answering questions

### For User

- Store project-specific context in `_Claude/contexts/[project-name].md`
- Store reusable prompts in `_Claude/prompts/`
- Tag notes with `#claude` for easy discovery

---

## Example Vault Context File

`vault/_Claude/contexts/StudioFeel.md`:

```markdown
# StudioFeel Project Context

## Project Goals
- System-wide audio equalizer for Windows
- Professional parametric EQ
- Per-device settings persistence

## Key Constraints
- APO runs in audiodg.exe (real-time, no allocations)
- Must pass Windows HLK testing
- HSA distribution model via Microsoft Store

## Personal Notes
- I prefer biquad filters over FIR for latency
- Named pipes need 1000ms timeout for UX
- WinUI 3 chosen over WinUI 2 for modern features
```

---

## Current Status

```
Vault link: Not created
Vault path: (you need to set this)
```

---

## How to Activate

Run this command (update the path first):

```bash
ln -s ~/Library/Mobile\ Documents/iCloud~md~obsidian/Documents/YourVault ./vault
```

Then tell Claude: "Vault is now available at ./vault"
