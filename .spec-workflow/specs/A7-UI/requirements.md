# Assignment 7-UI: User Interface (Inventory Screen, Crafting Interface)

**Due Date:** December 12, 2025 (Week 3)
**Estimated Duration:** 1 week
**Complexity:** Medium-High (UI refinement, mouse interaction)
**Implementation Standard:** ⭐ **Follow Minecraft Implementation Patterns** ⭐
**Dependencies:** ✅ Requires A7-Core completion (Registry, Inventory, ItemStack systems)

---

## Executive Summary

Assignment 7-UI implements the full user interface for SimpleMiner's inventory and crafting systems, building on the foundation established in A7-Core:

1. **Full Inventory Screen** - Modal dialog with 36 slots (27 main + 9 hotbar, Minecraft-authentic)
2. **Crafting Interface** - 2×2 crafting grid with 10 Minecraft-style recipes
3. **Mouse Interaction System** - Click, drag, drop, shift-click (Minecraft controls)
4. **WidgetSubsystem Refinements** - Modal dialogs, input routing, layouts (if needed)

This assignment completes the player-facing gameplay loop, enabling full inventory management and crafting before AI agent integration in A7-AI.

---

## Project Context

### Current State (Post-A7-Core)

**Existing Systems from A7-Core:**
- ✅ Registry<T> template for type-safe ID management
- ✅ ItemRegistry, BlockRegistry, RecipeRegistry (JSON-based)
- ✅ ItemStack data structure (itemID, quantity, durability)
- ✅ Player inventory (36 slots: 27 main + 9 hotbar)
- ✅ ItemEntity for dropped items (magnetic pickup, world physics)
- ✅ Mining mechanics (progressive break, block → item drop)
- ✅ Placement mechanics (consume inventory, place block)
- ✅ Basic HUD (hotbar widget, crosshair, Minecraft-style)

**Available Infrastructure:**
- Engine WidgetSubsystem: DX11-based UI rendering (may need refinement for production use)
- Existing HotbarWidget: 9-slot display at bottom-center (from A7-Core)
- Mouse input system: Available but may need integration with WidgetSubsystem

### Target State (Post-A7-UI)

**New Systems:**
- ✅ Full inventory screen (modal dialog, Minecraft-authentic layout)
- ✅ Crafting interface (2×2 grid with recipe matching, Minecraft mechanics)
- ✅ Mouse interaction (click, drag, drop, shift-click, Minecraft controls)
- ✅ Cursor item rendering (held item following mouse, Minecraft behavior)
- ✅ WidgetSubsystem enhancements (if needed for modal dialogs, input routing)

---

## Goals and Objectives

### Primary Goals

1. **Minecraft-Authentic UI** - Create pixel-perfect inventory screen matching Minecraft visual style
2. **Full Inventory Management** - Enable drag-and-drop, shift-click quick transfer (Minecraft controls)
3. **Functional Crafting** - Implement 2×2 crafting grid with 10 Minecraft recipes
4. **Modal Dialog System** - Pause gameplay when inventory open (Minecraft behavior)

### Secondary Goals

1. **WidgetSubsystem Refinement** - Improve for production quality (if needed)
2. **Performance** - Maintain 60 FPS with inventory screen open
3. **Extensibility** - Design for future UI elements (3×3 crafting, enchanting, etc.)

### Non-Goals (Explicitly Out of Scope for A7-UI)

- ❌ AI agents (deferred to A7-AI)
- ❌ Menu system (deferred to A7-Polish)
- ❌ Sound effects (deferred to A7-Polish)
- ❌ 3×3 crafting grid (only 2×2 for A7)
- ❌ Recipe book UI (Minecraft recipe book feature)
- ❌ Item tooltips (nice-to-have, not required)

---

## Functional Requirements

### FR-1: Full Inventory Screen (Minecraft-Authentic)

#### FR-1.1: Inventory Screen Layout (Minecraft-Style)
**Description**: Full inventory UI opened with 'E' key (pixel-perfect Minecraft layout).

**Requirements**:
- **Main Inventory Grid** (center, Minecraft layout):
  - 27 slots (3 rows × 9 columns, Minecraft standard)
  - Slot size: 16×16 pixels for items + 2px padding (Minecraft size)
  - Gray GUI background (Minecraft GUI texture style)
  - Slots numbered 0-26 (top-left to bottom-right)
- **Hotbar Section** (bottom, Minecraft layout):
  - 9 slots mirroring HUD hotbar (slots 27-35)
  - Visual connection showing it's the same inventory (Minecraft style)
  - Same selected slot highlight as HUD hotbar
- **Screen Positioning** (Minecraft-style):
  - Centered on screen (both horizontal and vertical)
  - Semi-transparent dark overlay behind inventory (Minecraft background dim)
- **Toggle Behavior** (Minecraft behavior):
  - 'E' key opens inventory screen
  - 'E' key or 'ESC' closes inventory screen
  - Opening freezes world updates (Minecraft pause)
  - Mouse cursor becomes visible

**UI Assets**:
- **Find Minecraft GUI textures online** (creative commons/fan resources) or **recreate programmatically**
- Must achieve Minecraft visual authenticity
- Reference: Minecraft Java Edition inventory screen

**WidgetSubsystem Integration**:
- May require **WidgetSubsystem improvements** for modal dialogs and mouse capture
- Create `InventoryWidget` class (extends `IWidget`)
- Register with owner ID = player entity ID
- Modal widget (blocks input to world, zOrder=200)

**Acceptance Criteria**:
- Inventory screen looks like Minecraft (visual authenticity)
- 'E' key toggles inventory screen (Minecraft control)
- All 36 slots visible and correctly positioned (Minecraft layout)
- Screen centers on display (Minecraft positioning)
- World pauses when inventory open (Minecraft behavior)

#### FR-1.2: Crafting Grid (Minecraft 2×2 Layout)
**Description**: 2×2 crafting grid with output slot (part of inventory screen, Minecraft layout).

**Requirements**:
- **Crafting Grid Position** (left side of inventory, Minecraft layout):
  - 2×2 input grid (4 slots, labeled as crafting slots 0-3)
  - 1 output slot (right of grid with arrow graphic, Minecraft style)
  - Arrow icon between input and output (Minecraft arrow texture)
  - Grid positioned to left of main inventory (Minecraft layout)
- **Crafting Slots** (Minecraft behavior):
  - Crafting slots separate from main inventory (slots 0-3)
  - Can place items in crafting grid from inventory (drag-and-drop)
  - Crafting grid clears when inventory closed (Minecraft behavior, items return to inventory)
- **Output Slot** (Minecraft mechanics):
  - Shows craftable item if valid recipe matches
  - Empty if no valid recipe
  - Cannot place items in output slot (read-only, Minecraft rule)
  - Clicking output consumes 1 of each input item (Minecraft behavior)

**Acceptance Criteria**:
- Crafting grid positioned left of inventory (Minecraft layout)
- Arrow graphic renders between input and output (Minecraft style)
- Can drag items into crafting grid from inventory
- Output slot shows valid craftable item (Minecraft recipe matching)
- Cannot drag items into output slot

---

### FR-2: Mouse Interaction System (Minecraft Controls)

#### FR-2.1: Cursor Item System (Minecraft Held Item)
**Description**: Render item icon following mouse cursor when holding items (Minecraft behavior).

**Requirements**:
- **Cursor Item State**:
  - `ItemStack m_cursorItem` - Currently held item by cursor
  - Empty when not holding anything (itemID = 0)
  - Contains full ItemStack (type, quantity, durability)
- **Cursor Item Rendering** (Minecraft style):
  - Render item icon at mouse position (16×16 pixels)
  - Render quantity text (bottom-right corner, if > 1, Minecraft position)
  - Render above all other UI elements (zOrder=1000)
  - No background (just item icon and count, Minecraft style)
- **Cursor Item Pickup/Place** (Minecraft behavior):
  - Left-click empty slot with held item: Place entire stack
  - Left-click slot with held item (same type): Merge stacks (up to max stack size)
  - Left-click slot with held item (different type): Swap with slot contents
  - Right-click empty slot with held item: Place single item (split stack)
  - Right-click slot with held item (same type): Add 1 item to slot

**Acceptance Criteria**:
- Item icon follows mouse cursor (Minecraft rendering)
- Quantity displays correctly (Minecraft text positioning)
- Cursor item renders above all UI (Minecraft z-order)
- Clicking slots picks up/places items (Minecraft behavior)

#### FR-2.2: Click Interaction (Minecraft Mouse Controls)
**Description**: Implement Minecraft mouse controls for inventory management.

**Requirements**:
- **Left-Click Behavior** (Minecraft controls):
  - **Empty cursor + Empty slot**: Nothing happens
  - **Empty cursor + Filled slot**: Pick up entire stack
  - **Held item + Empty slot**: Place entire stack
  - **Held item + Filled slot (same item)**: Merge stacks (up to max stack size, excess remains in cursor)
  - **Held item + Filled slot (different item)**: Swap items
- **Right-Click Behavior** (Minecraft controls):
  - **Empty cursor + Filled slot**: Pick up half stack (rounded up)
  - **Held item + Empty slot**: Place 1 item
  - **Held item + Filled slot (same item)**: Add 1 item to slot (if not at max)
  - **Held item + Filled slot (different item)**: Swap items (same as left-click)
- **Shift-Left-Click Behavior** (Minecraft quick transfer):
  - **Main inventory → Hotbar**: Transfer entire stack to first available hotbar slot
  - **Hotbar → Main inventory**: Transfer entire stack to first available main inventory slot
  - If no space available, nothing happens
- **Click Outside Inventory** (Minecraft drop behavior):
  - Left-click outside inventory UI with held item: Drop entire stack into world (spawn ItemEntity)
  - Right-click outside inventory with held item: Drop 1 item into world

**Acceptance Criteria**:
- Left-click picks up/places entire stacks (Minecraft behavior)
- Right-click picks up/places single items (Minecraft behavior)
- Shift-left-click quick transfers between inventory and hotbar (Minecraft convenience)
- Stack merging respects max stack size (Minecraft rules)
- Clicking outside inventory drops items (Minecraft behavior)

#### FR-2.3: Crafting Interaction (Minecraft Crafting Controls)
**Description**: Implement Minecraft mouse controls for crafting grid.

**Requirements**:
- **Crafting Grid Interaction** (Minecraft behavior):
  - Left-click/right-click crafting slots: Same as inventory slots (pick up, place, split)
  - Shift-left-click crafting slot: Quick transfer to inventory
  - Crafting slots accept all item types (any item can be placed)
- **Output Slot Interaction** (Minecraft behavior):
  - **Left-click output slot**:
    - Pick up output item (add to cursor)
    - Consume 1 of each input item in crafting grid
    - Update output slot (show next craftable item if materials remain)
  - **Shift-left-click output slot**:
    - Craft as many items as possible (loop until ingredients run out)
    - Add crafted items directly to inventory (skip cursor)
    - Stop when inventory full or ingredients depleted
  - **Right-click output slot**: Same as left-click (craft 1 item)
  - **Cannot place items in output slot**: Read-only (Minecraft rule)
- **Recipe Matching** (Minecraft mechanics):
  - Check crafting grid contents against RecipeRegistry every frame
  - Match shaped recipes (exact pattern, any position in 2×2 grid)
  - Match shapeless recipes (any arrangement of ingredients)
  - Update output slot with matched recipe result
  - If no recipe matches, output slot empty

**Acceptance Criteria**:
- Can place items in crafting grid (Minecraft controls)
- Output slot shows valid recipe result (Minecraft recipe matching)
- Left-click output crafts 1 item (Minecraft behavior)
- Shift-left-click output crafts multiple items (Minecraft convenience)
- Cannot place items in output slot (Minecraft rule)
- Recipe matching works for all 10 recipes (from A7-Core RecipeRegistry)

---

### FR-3: Recipe System Integration (Minecraft Crafting Mechanics)

#### FR-3.1: Recipe Matching Algorithm (Minecraft Pattern Matching)
**Description**: Match crafting grid contents against 10 Minecraft-style recipes.

**Requirements**:
- **Recipe Types** (from A7-Core RecipeRegistry):
  - **Shaped Recipes**: Pattern matters (e.g., 4 planks in 2×2 → crafting table)
  - **Shapeless Recipes**: Pattern doesn't matter (e.g., 1 log anywhere → 4 planks)
- **Shaped Recipe Matching** (Minecraft algorithm):
  - Check if crafting grid pattern matches recipe pattern
  - Allow pattern at any position in grid (e.g., top-left, top-right, bottom-left, bottom-right)
  - Empty slots must match recipe's empty slots
  - Item types must match exactly
- **Shapeless Recipe Matching** (Minecraft algorithm):
  - Check if crafting grid contains all required ingredients
  - Ignore position and arrangement
  - Allow extra empty slots
  - Item counts must match exactly
- **Recipe Priority** (Minecraft behavior):
  - Check shaped recipes first (more specific)
  - Check shapeless recipes second (more general)
  - Return first matching recipe
  - If no match, return nullptr

**10 Required Recipes** (from A7-Core):
1. LOG → 4 PLANKS (shapeless)
2. 4 PLANKS (2×2) → CRAFTING_TABLE (shaped)
3. 2 PLANKS (vertical) → 4 STICKS (shaped)
4. 3 PLANKS + 2 STICKS → WOODEN_PICKAXE (shaped, requires specific pattern)
5. 3 PLANKS + 2 STICKS → WOODEN_AXE (shaped, requires specific pattern)
6. 1 PLANK + 2 STICKS → WOODEN_SHOVEL (shaped, requires specific pattern)
7. 4 COBBLESTONE (2×2) → FURNACE (shaped)
8. 2 COBBLESTONE (vertical) → STONE_STAIRS (shaped)
9. 3 COBBLESTONE (horizontal) → STONE_SLAB (shaped)
10. 1 COAL + 1 STICK → TORCH (shapeless)

**Acceptance Criteria**:
- All 10 recipes match correctly (Minecraft behavior)
- Shaped recipes require correct pattern (Minecraft rules)
- Shapeless recipes work with any arrangement (Minecraft rules)
- Recipe matching updates every frame (real-time feedback)
- Output slot shows correct result item and quantity

#### FR-3.2: Crafting Grid Clearing (Minecraft Behavior)
**Description**: Handle crafting grid when inventory closed (Minecraft behavior).

**Requirements**:
- **On Inventory Close** (Minecraft behavior):
  - Move all items from crafting grid back to inventory
  - If inventory full, drop items in world as ItemEntity (Minecraft overflow behavior)
  - Clear crafting slots (set to empty)
  - Clear output slot preview
- **On Crafting Complete** (Minecraft behavior):
  - Consume 1 of each input item
  - If stack becomes empty (quantity = 0), clear slot
  - Update output slot (re-check recipe with remaining items)

**Acceptance Criteria**:
- Closing inventory returns crafting items to inventory (Minecraft behavior)
- If inventory full, items drop in world (Minecraft overflow)
- Crafting grid clears on inventory close (Minecraft reset)
- Crafting consumes correct quantities (Minecraft recipe rules)

---

### FR-4: WidgetSubsystem Refinements (If Needed)

#### FR-4.1: Modal Dialog Support
**Description**: Extend WidgetSubsystem to support modal dialogs (if not already functional).

**Requirements** (only if WidgetSubsystem lacks these features):
- **Modal Widget Flag**:
  - Widgets can be marked as `isModal = true`
  - Modal widgets block input to underlying widgets
  - Only topmost modal widget receives input
- **Input Routing**:
  - Mouse events routed to topmost visible widget at cursor position
  - Keyboard events routed to focused widget (e.g., inventory screen)
  - World input disabled when modal widget active
- **zOrder Rendering**:
  - Widgets render in zOrder (lowest to highest)
  - Modal widgets typically have high zOrder (e.g., 200)
  - Cursor item renders at highest zOrder (e.g., 1000)

**Acceptance Criteria** (only if refinement needed):
- Inventory screen blocks world input when open
- Mouse clicks on inventory slots work correctly
- Clicking outside inventory closes it (or drops held item)
- ESC key closes inventory screen

**Note**: If current WidgetSubsystem already supports modal dialogs and input routing, skip this requirement.

#### FR-4.2: Layout System (Optional Nice-to-Have)
**Description**: Helper system for positioning widgets in grid layouts (optional, not required).

**Requirements** (optional, nice-to-have):
- `GridLayout` helper class for positioning inventory slots
- Automatic spacing and padding calculations
- Centered positioning on screen

**Note**: Can be implemented manually without layout system. This is a stretch goal for code quality.

---

## Non-Functional Requirements

### NFR-1: Performance
- **Target FPS**: 60 FPS sustained with inventory screen open
- **UI Rendering**: < 2ms per frame for inventory + crafting UI
- **Recipe Matching**: < 0.1ms per frame (real-time matching)
- **Mouse Interaction**: < 5ms response time (one frame at 60 FPS)

### NFR-2: Memory
- **Inventory UI**: < 5MB texture memory for GUI sprites
- **Cursor Item**: Negligible (single ItemStack = 6 bytes)

### NFR-3: Usability (Minecraft Standard)
- **Input Responsiveness**: Immediate visual feedback on clicks (Minecraft polish)
- **Visual Fidelity**: Pixel-perfect Minecraft UI (or as close as possible)
- **Error Prevention**: Invalid operations do nothing (no crashes, Minecraft robustness)

### NFR-4: Extensibility
- **Recipe System**: Easy to add more recipes via JSON (A7-Core RecipeRegistry)
- **UI Widgets**: Easy to add new UI elements (future features)
- **Layout**: Grid layout system allows easy repositioning (if implemented)

### NFR-5: Code Quality
- **SOLID Principles**: InventoryWidget has single responsibility (rendering inventory)
- **DRY**: Reuse ItemStack rendering logic for cursor and slots
- **KISS**: Simple mouse state machine (cursor item, slot interaction)
- **Minecraft Fidelity**: All UI follows Minecraft implementation patterns

---

## Constraints and Assumptions

### Technical Constraints
1. **DirectX 11**: All UI must use WidgetSubsystem
2. **Windows Only**: No cross-platform support required
3. **Single-threaded Gameplay**: UI updates on main thread
4. **WidgetSubsystem**: May need refinement for modal dialogs (unknown current state)

### Assumptions
1. **A7-Core Complete**: Inventory, ItemStack, Registry systems functional
2. **Minecraft Assets**: GUI sprite sheets can be found online or recreated programmatically
3. **Mouse Input**: Engine provides mouse position and button state
4. **WidgetSubsystem**: Can be refined if needed (not complete redesign)

---

## Success Criteria

### Minimum Viable Product (MVP)
**These features MUST work for A7-UI completion**:

1. ✅ Full inventory screen opens with 'E' key (Minecraft-authentic)
2. ✅ All 36 slots visible (27 main + 9 hotbar, Minecraft layout)
3. ✅ Mouse interaction works (left-click, right-click, Minecraft controls)
4. ✅ Drag-and-drop items between slots (Minecraft behavior)
5. ✅ Shift-left-click quick transfer (Minecraft convenience)
6. ✅ Crafting grid with 2×2 input + 1 output slot (Minecraft layout)
7. ✅ All 10 recipes work correctly (Minecraft recipe matching)
8. ✅ Crafting consumes ingredients (Minecraft mechanics)
9. ✅ Inventory screen pauses gameplay (Minecraft behavior)

**Demo Scenario**:
```
1. Player opens inventory with 'E' key
2. Inventory screen appears (Minecraft-authentic UI)
3. Player drags logs from inventory to crafting grid
4. Output slot shows 4 planks (recipe matched)
5. Player shift-clicks output (crafts multiple planks)
6. Player places planks in 2×2 pattern in crafting grid
7. Output slot shows crafting table (recipe matched)
8. Player left-clicks output (crafts 1 crafting table)
9. Player closes inventory with 'E' or ESC
10. Crafting grid items return to inventory
```

### Stretch Goals (Nice-to-Have)
**These features are bonus, not required**:

- ⭐ Pixel-perfect Minecraft UI recreation (very high visual fidelity)
- ⭐ Item tooltips on hover (show item name, durability)
- ⭐ Recipe preview tooltip (show recipe pattern on hover)
- ⭐ Smooth item drag animation (Minecraft-style lerp)
- ⭐ Layout system for automatic widget positioning
- ⭐ WidgetSubsystem full production-ready redesign (if time permits)

---

## Dependencies

**A7-Core Systems** (REQUIRED):
1. **Registry<T>**: Item and Recipe registries must be functional
2. **ItemStack**: Data structure for item representation
3. **Inventory**: 36-slot inventory system in Player
4. **RecipeRegistry**: 10 Minecraft recipes loaded from JSON
5. **HotbarWidget**: Basic hotbar rendering (extend for full inventory)

**Engine Systems**:
1. **WidgetSubsystem**: DX11 UI rendering (may need refinement)
2. **Mouse Input**: Mouse position and button state
3. **Renderer**: Orthographic projection for screen-space UI

**A7 Spec Sequence**:
- **A7-Core** (complete) → **A7-UI** (this spec) → **A7-AI** (agents, KADI)
- A7-AI depends on A7-Core inventory system (agents have inventory)
- A7-AI does NOT depend on A7-UI (agents use programmatic inventory API)

---

## References

### Minecraft Systems (Implementation Reference)
- [Minecraft Wiki: Inventory](https://minecraft.wiki/w/Inventory)
- [Minecraft Wiki: Crafting](https://minecraft.wiki/w/Crafting)
- [Minecraft Wiki: Inventory GUI](https://minecraft.wiki/w/Inventory#GUI)

### SimpleMiner Codebase
- **Engine**: `C:\p4\Personal\SD\Engine\Code\Engine\`
  - `Widget\` - WidgetSubsystem (needs refinement)
- **SimpleMiner**: `C:\p4\Personal\SD\SimpleMiner\Code\Game\`
  - `UI\HotbarWidget.hpp` (from A7-Core, extend for InventoryWidget)
  - `Gameplay\Player.cpp` (inventory management)

---

**Next Steps**:
1. Review and approve this requirements.md via spec-workflow
2. Create design.md for A7-UI with detailed UI architecture
3. After design approval, create tasks.md and begin implementation
4. After A7-UI complete, proceed to A7-AI spec
