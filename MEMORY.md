# Session Memory - Riftwalker

## Key Facts
- **Project:** Riftwalker, C++17 roguelike with SDL2 + ECS architecture
- **Status (2026-03-28):** Core gameplay complete, 17 enemy types + 6 bosses, visual testing framework, NG+ system
- **Build:** `cmake` → `cmake --build . --config Release` after new files
- **Testing:** F6 (smoke test), F3 (debug), F9 (screenshot)

## Autonomous Work Pattern
Claude works well autonomously during long sessions:
- Can work the entire evening if requested ("mach autonom")
- Each tool-call has 10min timeout, but session has no limit
- Context window compression happens, but work continues
- For very long nights: consider starting new session with `/remember`

## Recent Learnings (Needs Cleanup = < 200 lines target)

### Session 2026-04-02 (Deep Code Audit + Settings Hardening)
- **11 Bugfixes**: Animation OOB, 19 null texture derefs, missing save calls, achievement triggers, leaderboard load, deserialization validation, settings bounds
- **Build Status**: Compiles clean, no warnings. 6 commits, 13 files, all critical systems audited
- **Pending Verification**: ScreenEffects/Particles and Weapon/Class systems still verifying in background agents
