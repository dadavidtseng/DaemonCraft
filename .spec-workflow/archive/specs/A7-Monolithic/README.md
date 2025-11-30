# Archived: A7 Monolithic Spec

**Archive Date:** 2025-11-27
**Reason:** Split into 3 separate specs (A7-Core, A7-UI, A7-AI)

---

## Original Spec

This directory contains the original monolithic Assignment 7 specification that covered all systems in one large spec:

- **requirements.md** (v3, approved) - Original requirements with 7 major systems
- **design.md** (partial, ~1,000 lines) - Only 4 sections completed before split

---

## Why Split?

Based on user feedback during design.md creation:

1. **Scope Too Large**: 7 major systems (Registry, Inventory, Mining, UI, Crafting, AI, Menu, Sound) was too much for single spec
2. **Implementation Complexity**: Different systems have different dependencies and timelines
3. **Better Organization**: Splitting allows phased implementation (Foundation → UI → AI)

---

## New Structure

The monolithic A7 spec has been replaced by 3 sequential specs:

### **A7-Core** (Foundation - Week 1-2)
- Rendering Bug Fix (chunk boundary faces)
- Registry System (Block, Item, Recipe)
- Inventory System (ItemStack, 36 slots)
- Mining & Placement Mechanics
- Basic HUD (Hotbar only)

### **A7-UI** (User Interface - Week 3)
- Full Inventory Screen (27 main + 9 hotbar)
- Crafting Interface (2×2 grid, 10 recipes)
- Mouse Interaction (click, drag, shift-click)
- WidgetSubsystem Refinements (if needed)

### **A7-AI** (Agent Integration - Week 4)
- AIAgent Entity Class
- Command System (MOVE, MINE, PLACE, CRAFT, WAIT)
- Agent Vision System
- KADI WebSocket Integration (7 MCP tools)
- TypeScript Agent Framework

---

## Key Changes from Original

1. **Removed "P-1" Terminology**: Now called "Rendering Bug Fix"
2. **AI Agent Framework Last**: Moved to separate A7-AI spec (Week 4)
3. **Clear Dependencies**: A7-Core → A7-UI → A7-AI
4. **Menu System Deferred**: Moved to A7-Polish (future stretch goals)
5. **Sound Effects Deferred**: Moved to A7-Polish (future stretch goals)

---

## References

- **New Specs**: `.spec-workflow/specs/A7-Core/`, `A7-UI/`, `A7-AI/`
- **Steering Documents**: `.spec-workflow/steering/` (product.md, tech.md, structure.md)
