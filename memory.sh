#!/bin/bash
# memory.sh — Layer 3: Live Git Context Injection
# Injects live git context at launch. Last 5 commits, modified files, current branch.
# Claude knows what changed before you say a word.

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root detection
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

cd "$PROJECT_ROOT"

# Header
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  LAYER 3: MEMORY.SH — Git Context Injection${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Check if we're in a git repo
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠ Not a git repository yet${NC}"
    echo "Initialize with: git init"
    echo ""
    exit 0
fi

# 1. Current Branch
echo -e "${GREEN}▸ Current Branch:${NC}"
BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo "  $BRANCH"
echo ""

# 2. Last 5 Commits
echo -e "${GREEN}▸ Last 5 Commits:${NC}"
git log -5 --pretty=format:"  %h - %s (%ar)" --color=never 2>/dev/null || echo "  No commits yet"
echo ""

# 3. Modified Files (staged and unstaged)
echo -e "${GREEN}▸ Modified Files:${NC}"

# Staged changes
STAGED=$(git diff --cached --name-only)
if [ -n "$STAGED" ]; then
    echo "  ${YELLOW}Staged:${NC}"
    echo "$STAGED" | while read -r file; do
        echo "    + $file"
    done
fi

# Unstaged changes
UNSTAGED=$(git diff --name-only)
if [ -n "$UNSTAGED" ]; then
    echo "  ${YELLOW}Unstaged:${NC}"
    echo "$UNSTAGED" | while read -r file; do
        echo "    ~ $file"
    done
fi

# Untracked files
UNTRACKED=$(git ls-files --others --exclude-standard | head -10)
if [ -n "$UNTRACKED" ]; then
    echo "  ${YELLOW}Untracked:${NC}"
    echo "$UNTRACKED" | while read -r file; do
        echo "    ? $file"
    done
    MORE=$(git ls-files --others --exclude-standard | tail -n +11 | wc -l)
    if [ "$MORE" -gt 0 ]; then
        echo "    ... and $MORE more"
    fi
fi

if [ -z "$STAGED" ] && [ -z "$UNSTAGED" ] && [ -z "$UNTRACKED" ]; then
    echo "  (working directory clean)"
fi
echo ""

# 4. Stash Status
STASH_COUNT=$(git stash list | wc -l | tr -d ' ')
if [ "$STASH_COUNT" -gt 0 ]; then
    echo -e "${GREEN}▸ Stashes:${NC} $STASH_COUNT"
    git stash list | head -3 | while read -r stash; do
        echo "  $stash"
    done
    echo ""
fi

# 5. Recent Branches
echo -e "${GREEN}▸ Recent Branches:${NC}"
git branch -a --sort=-committerdate | head -5 | while read -r branch; do
    if [[ "$branch" == "*"* ]]; then
        echo -e "  ${YELLOW}${branch}${NC}"
    else
        echo "  $branch"
    fi
done
echo ""

# 6. Summary for Claude
echo -e "${BLUE}───────────────────────────────────────────────────────────────${NC}"
echo -e "${BLUE}  Summary for Claude${NC}"
echo -e "${BLUE}───────────────────────────────────────────────────────────────${NC}"
echo ""

echo -e "${GREEN}Claude, here's what you need to know:${NC}"
echo ""

# Generate a summary paragraph
if [ -n "$STAGED" ] || [ -n "$UNSTAGED" ]; then
    echo -e "  ${YELLOW}Active work in progress:${NC}"
    [ -n "$STAGED" ] && echo "$STAGED" | while read -r file; do echo "    - $file (staged)"; done
    [ -n "$UNSTAGED" ] && echo "$UNSTAGED" | while read -r file; do echo "    - $file (modified)"; done
    echo ""
fi

LAST_COMMIT=$(git log -1 --pretty=format:"%s" 2>/dev/null)
if [ -n "$LAST_COMMIT" ]; then
    echo "  Last commit: $LAST_COMMIT"
fi

echo ""
echo "  Current branch: $BRANCH"
echo ""
