# Assignment 7-UI: Design Document
# User Interface (Inventory Screen, Crafting Interface)

**Version:** 1.0
**Date:** 2025-11-27
**Status:** Design Phase
**Dependencies:** ✅ Requires A7-Core completion (Registry, Inventory, ItemStack systems)

---

## Executive Summary

This design document provides the technical architecture for A7-UI's implementation of full inventory and crafting user interface. The design covers:

1. **Full Inventory Screen** - Modal dialog with 36 slots (Minecraft-authentic layout)
2. **Crafting Interface** - 2×2 crafting grid with recipe matching
3. **Mouse Interaction System** - Click, drag, drop, shift-click (Minecraft controls)
4. **Cursor Item Rendering** - Held item following mouse (Minecraft behavior)
5. **WidgetSubsystem Refinements** - Modal dialogs, input routing (if needed)

**Design Philosophy:**
- **Minecraft UI Fidelity** - Pixel-perfect recreation of Minecraft inventory screen
- **Event-Driven Architecture** - Mouse events propagate through widget hierarchy
- **State Machine Pattern** - Clear cursor item state transitions
- **Reuse A7-Core Systems** - Leverage existing Inventory, ItemStack, RecipeRegistry

---

## Table of Contents

1. [System Architecture Overview](#1-system-architecture-overview)
2. [InventoryWidget Design](#2-inventorywidget-design)
3. [Mouse Interaction System](#3-mouse-interaction-system)
4. [Crafting System Integration](#4-crafting-system-integration)
5. [WidgetSubsystem Refinements](#5-widgetsubsystem-refinements)
6. [UI Layout and Rendering](#6-ui-layout-and-rendering)
7. [State Machines](#7-state-machines)
8. [Implementation Strategy](#8-implementation-strategy)

---

## 1. System Architecture Overview

### Component Architecture

```
┌─────────────────────────────────────────────┐
│         InventoryWidget (Modal)             │
│  ┌─────────────────────────────────────┐   │
│  │  InventoryGrid (27 slots)           │   │
│  │  HotbarGrid (9 slots)               │   │
│  │  CraftingGrid (4 input + 1 output)  │   │
│  │  CursorItem (held item rendering)   │   │
│  └─────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
                    ↓↑
┌─────────────────────────────────────────────┐
│       WidgetSubsystem (Engine)              │
│    - Modal widget support                   │
│    - Mouse event routing                    │
│    - Input capture                          │
└─────────────────────────────────────────────┘
                    ↓↑
┌─────────────────────────────────────────────┐
│       Player Inventory (A7-Core)            │
│    - 36 slots (27 main + 9 hotbar)          │
│    - ItemStack storage                      │
└─────────────────────────────────────────────┘
```

### Data Flow

```
Mouse Click Event
     ↓
WidgetSubsystem::OnMouseEvent()
     ↓
InventoryWidget::OnMouseEvent()
     ↓
Determine Clicked Slot → ClickHandler
     ↓
Update Cursor Item State
     ↓
Update Player Inventory
     ↓
Render Updated UI
```

---

## 2. InventoryWidget Design

### Class Structure

**File:** `SimpleMiner\Code\Game\UI\InventoryWidget.hpp`

```cpp
class InventoryWidget : public IWidget
{
public:
    InventoryWidget(Player* player);
    ~InventoryWidget();

    // IWidget Interface
    virtual void Update(float deltaSeconds) override;
    virtual void Render() const override;
    virtual bool OnMouseEvent(MouseEvent const& event) override;
    virtual bool OnKeyEvent(KeyEvent const& event) override;

    // Visibility
    void Show();
    void Hide();
    bool IsVisible() const { return m_isVisible; }

private:
    Player* m_player = nullptr;
    bool m_isVisible = false;

    // Cursor Item (held by mouse)
    ItemStack m_cursorItem;  // Empty when not holding anything

    // Layout (calculated in constructor)
    Vec2 m_screenCenter;
    AABB2 m_inventoryGridBounds;   // 27 slots (3×9)
    AABB2 m_hotbarGridBounds;      // 9 slots (1×9)
    AABB2 m_craftingGridBounds;    // 4 slots (2×2)
    AABB2 m_craftingOutputBounds;  // 1 slot
    std::vector<AABB2> m_allSlotBounds;  // All 40 slots (36 inventory + 4 crafting)

    // Textures
    Texture* m_backgroundTexture = nullptr;
    Texture* m_slotTexture = nullptr;
    Texture* m_arrowTexture = nullptr;  // Crafting arrow
    Texture* m_itemSpriteSheet = nullptr;

    // Rendering
    void RenderBackground() const;
    void RenderInventoryGrid() const;
    void RenderHotbarGrid() const;
    void RenderCraftingGrid() const;
    void RenderCraftingOutput() const;
    void RenderSlot(AABB2 const& bounds, ItemStack const& item, bool isSelected) const;
    void RenderCursorItem() const;

    // Input Handling
    int GetSlotIndexAtPosition(Vec2 const& mousePos) const;  // Returns -1 if none
    void HandleLeftClick(int slotIndex);
    void HandleRightClick(int slotIndex);
    void HandleShiftLeftClick(int slotIndex);
    void HandleClickOutside();

    // Crafting
    void UpdateCraftingOutput();
    Recipe* m_currentRecipe = nullptr;  // Cached recipe match
};
```

### Layout Calculations

**Minecraft Inventory Layout (pixels):**
```
┌────────────────────────────────────────────┐
│  Crafting Grid (2×2)      Output Slot      │  ← Top-left
│  [0][1]          →        [OUT]            │
│  [2][3]                                    │
│                                            │
│  Main Inventory (3×9)                      │  ← Center
│  [0 ][1 ][2 ][3 ][4 ][5 ][6 ][7 ][8 ]     │
│  [9 ][10][11][12][13][14][15][16][17]     │
│  [18][19][20][21][22][23][24][25][26]     │
│                                            │
│  Hotbar (1×9)                              │  ← Bottom
│  [27][28][29][30][31][32][33][34][35]     │
└────────────────────────────────────────────┘

Dimensions (Minecraft-style):
- Slot size: 18×18 pixels (16px item + 1px padding)
- Spacing: 2px between slots
- Background: 176×166 pixels
```

**Code:**
```cpp
void InventoryWidget::CalculateLayout()
{
    // Center on screen
    Vec2 screenDims = g_window->GetDimensions();
    m_screenCenter = screenDims * 0.5f;

    // Background (centered)
    Vec2 backgroundSize(176.0f, 166.0f);
    Vec2 backgroundMin = m_screenCenter - backgroundSize * 0.5f;

    // Crafting Grid (top-left, 2×2)
    Vec2 craftingStart = backgroundMin + Vec2(30.0f, 17.0f);
    for (int row = 0; row < 2; row++)
    {
        for (int col = 0; col < 2; col++)
        {
            int slotIndex = row * 2 + col;
            Vec2 slotMin = craftingStart + Vec2(col * 20.0f, row * 20.0f);
            m_allSlotBounds[36 + slotIndex] = AABB2(slotMin, slotMin + Vec2(18.0f, 18.0f));
        }
    }

    // Crafting Output (right of crafting grid)
    Vec2 outputMin = craftingStart + Vec2(94.0f, 9.0f);
    m_craftingOutputBounds = AABB2(outputMin, outputMin + Vec2(18.0f, 18.0f));

    // Main Inventory (3×9)
    Vec2 inventoryStart = backgroundMin + Vec2(8.0f, 84.0f);
    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 9; col++)
        {
            int slotIndex = row * 9 + col;
            Vec2 slotMin = inventoryStart + Vec2(col * 20.0f, row * 20.0f);
            m_allSlotBounds[slotIndex] = AABB2(slotMin, slotMin + Vec2(18.0f, 18.0f));
        }
    }

    // Hotbar (1×9)
    Vec2 hotbarStart = backgroundMin + Vec2(8.0f, 142.0f);
    for (int col = 0; col < 9; col++)
    {
        Vec2 slotMin = hotbarStart + Vec2(col * 20.0f, 0.0f);
        m_allSlotBounds[27 + col] = AABB2(slotMin, slotMin + Vec2(18.0f, 18.0f));
    }
}
```

---

## 3. Mouse Interaction System

### Mouse Event Flow

```cpp
bool InventoryWidget::OnMouseEvent(MouseEvent const& event)
{
    if (!m_isVisible) return false;

    Vec2 mousePos = event.position;
    int slotIndex = GetSlotIndexAtPosition(mousePos);

    if (event.type == eMouseEventType::LEFT_BUTTON_DOWN)
    {
        if (slotIndex >= 0)
        {
            if (event.modifiers & eKeyModifier::SHIFT)
                HandleShiftLeftClick(slotIndex);
            else
                HandleLeftClick(slotIndex);
        }
        else
        {
            HandleClickOutside();
        }
        return true;  // Consume event
    }
    else if (event.type == eMouseEventType::RIGHT_BUTTON_DOWN)
    {
        if (slotIndex >= 0)
        {
            HandleRightClick(slotIndex);
        }
        return true;
    }

    return false;
}
```

### Left-Click Handler (Minecraft Behavior)

```cpp
void InventoryWidget::HandleLeftClick(int slotIndex)
{
    // Special case: Crafting output slot
    if (slotIndex == CRAFTING_OUTPUT_SLOT)
    {
        HandleCraftingOutputClick();
        return;
    }

    ItemStack& slotItem = GetSlotItemStack(slotIndex);

    // Case 1: Empty cursor + Empty slot → Nothing
    if (m_cursorItem.IsEmpty() && slotItem.IsEmpty())
    {
        return;
    }

    // Case 2: Empty cursor + Filled slot → Pick up entire stack
    if (m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
    {
        m_cursorItem = slotItem;
        slotItem.Clear();
        return;
    }

    // Case 3: Held item + Empty slot → Place entire stack
    if (!m_cursorItem.IsEmpty() && slotItem.IsEmpty())
    {
        slotItem = m_cursorItem;
        m_cursorItem.Clear();
        return;
    }

    // Case 4: Held item + Filled slot (same item) → Merge stacks
    if (!m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
    {
        if (m_cursorItem.itemID == slotItem.itemID)
        {
            ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
            uint8_t maxStack = itemDef->m_maxStackSize;

            uint8_t space = maxStack - slotItem.quantity;
            uint8_t toTransfer = std::min(space, m_cursorItem.quantity);

            slotItem.quantity += toTransfer;
            m_cursorItem.quantity -= toTransfer;

            if (m_cursorItem.quantity == 0)
                m_cursorItem.Clear();

            return;
        }

        // Case 5: Held item + Filled slot (different item) → Swap
        ItemStack temp = m_cursorItem;
        m_cursorItem = slotItem;
        slotItem = temp;
    }
}
```

### Right-Click Handler (Minecraft Behavior)

```cpp
void InventoryWidget::HandleRightClick(int slotIndex)
{
    ItemStack& slotItem = GetSlotItemStack(slotIndex);

    // Case 1: Empty cursor + Filled slot → Pick up half stack (rounded up)
    if (m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
    {
        uint8_t half = (slotItem.quantity + 1) / 2;  // Round up
        m_cursorItem.itemID = slotItem.itemID;
        m_cursorItem.quantity = half;
        m_cursorItem.durability = slotItem.durability;

        slotItem.quantity -= half;
        if (slotItem.quantity == 0)
            slotItem.Clear();

        return;
    }

    // Case 2: Held item + Empty slot → Place 1 item
    if (!m_cursorItem.IsEmpty() && slotItem.IsEmpty())
    {
        slotItem.itemID = m_cursorItem.itemID;
        slotItem.quantity = 1;
        slotItem.durability = m_cursorItem.durability;

        m_cursorItem.quantity--;
        if (m_cursorItem.quantity == 0)
            m_cursorItem.Clear();

        return;
    }

    // Case 3: Held item + Filled slot (same item) → Add 1 item to slot
    if (!m_cursorItem.IsEmpty() && !slotItem.IsEmpty())
    {
        if (m_cursorItem.itemID == slotItem.itemID)
        {
            ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
            if (slotItem.quantity < itemDef->m_maxStackSize)
            {
                slotItem.quantity++;
                m_cursorItem.quantity--;

                if (m_cursorItem.quantity == 0)
                    m_cursorItem.Clear();
            }
            return;
        }

        // Case 4: Different item → Swap (same as left-click)
        ItemStack temp = m_cursorItem;
        m_cursorItem = slotItem;
        slotItem = temp;
    }
}
```

### Shift-Left-Click Handler (Quick Transfer)

```cpp
void InventoryWidget::HandleShiftLeftClick(int slotIndex)
{
    ItemStack& slotItem = GetSlotItemStack(slotIndex);
    if (slotItem.IsEmpty()) return;

    // Determine source and destination ranges
    bool isHotbar = (slotIndex >= 27 && slotIndex <= 35);
    bool isCrafting = (slotIndex >= 36 && slotIndex <= 39);

    int destStart, destEnd;
    if (isHotbar || isCrafting)
    {
        // Hotbar/Crafting → Main Inventory
        destStart = 0;
        destEnd = 26;
    }
    else
    {
        // Main Inventory → Hotbar
        destStart = 27;
        destEnd = 35;
    }

    // Try to merge with existing stacks
    ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(slotItem.itemID);
    for (int i = destStart; i <= destEnd; i++)
    {
        ItemStack& destSlot = GetSlotItemStack(i);
        if (destSlot.itemID == slotItem.itemID && destSlot.quantity < itemDef->m_maxStackSize)
        {
            uint8_t space = itemDef->m_maxStackSize - destSlot.quantity;
            uint8_t toTransfer = std::min(space, slotItem.quantity);

            destSlot.quantity += toTransfer;
            slotItem.quantity -= toTransfer;

            if (slotItem.quantity == 0)
            {
                slotItem.Clear();
                return;
            }
        }
    }

    // Try to find empty slot
    for (int i = destStart; i <= destEnd; i++)
    {
        ItemStack& destSlot = GetSlotItemStack(i);
        if (destSlot.IsEmpty())
        {
            destSlot = slotItem;
            slotItem.Clear();
            return;
        }
    }

    // No space available, do nothing
}
```

---

## 4. Crafting System Integration

### Recipe Matching Algorithm

```cpp
void InventoryWidget::UpdateCraftingOutput()
{
    // Get crafting grid items
    std::array<ItemStack, 4> craftingGrid;
    for (int i = 0; i < 4; i++)
    {
        craftingGrid[i] = GetSlotItemStack(36 + i);  // Slots 36-39
    }

    // Convert to itemID array for recipe matching
    std::array<uint16_t, 4> pattern;
    for (int i = 0; i < 4; i++)
    {
        pattern[i] = craftingGrid[i].itemID;
    }

    // Check all recipes
    RecipeRegistry& recipeReg = RecipeRegistry::GetInstance();
    m_currentRecipe = nullptr;

    for (Recipe* recipe : recipeReg.GetAll())
    {
        if (recipe->Matches(pattern))
        {
            m_currentRecipe = recipe;
            break;
        }
    }
}
```

### Recipe::Matches Implementation

```cpp
bool Recipe::Matches(std::array<uint16_t, 4> const& craftingGrid) const
{
    if (m_type == eRecipeType::SHAPED)
    {
        // Shaped: Check all 4 possible positions in 2×2 grid
        // (pattern can be anywhere: top-left, top-right, bottom-left, bottom-right)

        // Position 1: Top-left (no offset)
        if (MatchesShapedPattern(craftingGrid, 0, 0)) return true;

        // Position 2: Top-right (offset col +1)
        // Position 3: Bottom-left (offset row +1)
        // Position 4: Bottom-right (offset both +1)
        // ... (check all positions)

        return false;
    }
    else  // SHAPELESS
    {
        // Count ingredients in grid
        std::map<uint16_t, int> gridCounts;
        for (uint16_t itemID : craftingGrid)
        {
            if (itemID != 0)
                gridCounts[itemID]++;
        }

        // Count required ingredients
        std::map<uint16_t, int> recipeCounts;
        for (uint16_t itemID : m_ingredients)
        {
            recipeCounts[itemID]++;
        }

        // Must match exactly
        return gridCounts == recipeCounts;
    }
}
```

### Crafting Output Click Handler

```cpp
void InventoryWidget::HandleCraftingOutputClick()
{
    if (!m_currentRecipe) return;  // No valid recipe

    // Check if cursor can hold output
    ItemStack output(m_currentRecipe->m_outputItemID, m_currentRecipe->m_outputQuantity);

    if (!m_cursorItem.IsEmpty() && m_cursorItem.itemID != output.itemID)
        return;  // Cannot merge different items

    // Consume ingredients from crafting grid
    for (int i = 0; i < 4; i++)
    {
        ItemStack& slot = GetSlotItemStack(36 + i);
        if (!slot.IsEmpty())
        {
            slot.quantity--;
            if (slot.quantity == 0)
                slot.Clear();
        }
    }

    // Add output to cursor
    if (m_cursorItem.IsEmpty())
    {
        m_cursorItem = output;
    }
    else
    {
        m_cursorItem.quantity += output.quantity;
    }

    // Update crafting output (re-check recipe with remaining items)
    UpdateCraftingOutput();
}
```

### Shift-Click Crafting (Craft Multiple)

```cpp
void InventoryWidget::HandleShiftCraftingOutputClick()
{
    if (!m_currentRecipe) return;

    // Craft as many as possible
    while (m_currentRecipe != nullptr)
    {
        // Try to add output to inventory
        ItemStack output(m_currentRecipe->m_outputItemID, m_currentRecipe->m_outputQuantity);
        if (!m_player->GetInventory().AddItem(output))
            break;  // Inventory full

        // Consume ingredients
        for (int i = 0; i < 4; i++)
        {
            ItemStack& slot = GetSlotItemStack(36 + i);
            if (!slot.IsEmpty())
            {
                slot.quantity--;
                if (slot.quantity == 0)
                    slot.Clear();
            }
        }

        // Re-check recipe
        UpdateCraftingOutput();
    }
}
```

---

## 5. WidgetSubsystem Refinements

### Modal Widget Support (If Needed)

**File:** `Engine\Code\Engine\Widget\WidgetSubsystem.hpp`

```cpp
// Add to IWidget interface (if not already present):
class IWidget
{
public:
    virtual bool IsModal() const { return false; }  // Override for modal widgets
    // ...
};

// Add to WidgetSubsystem:
class WidgetSubsystem
{
private:
    std::vector<IWidget*> m_widgets;  // Sorted by zOrder

public:
    bool OnMouseEvent(MouseEvent const& event)
    {
        // Route to topmost modal widget first
        for (auto it = m_widgets.rbegin(); it != m_widgets.rend(); ++it)
        {
            IWidget* widget = *it;
            if (widget->IsModal() && widget->IsVisible())
            {
                if (widget->OnMouseEvent(event))
                    return true;  // Consumed by modal widget
                break;  // Stop at first modal
            }
        }

        // If no modal consumed, route to all widgets (top to bottom)
        for (auto it = m_widgets.rbegin(); it != m_widgets.rend(); ++it)
        {
            if ((*it)->OnMouseEvent(event))
                return true;
        }

        return false;
    }
};
```

### Input Capture for Inventory

```cpp
class InventoryWidget : public IWidget
{
public:
    virtual bool IsModal() const override { return m_isVisible; }

    bool OnKeyEvent(KeyEvent const& event) override
    {
        if (event.key == KEY_E || event.key == KEY_ESCAPE)
        {
            if (event.type == eKeyEventType::KEY_DOWN)
            {
                Hide();
                return true;  // Consume event
            }
        }
        return false;
    }
};
```

---

## 6. UI Layout and Rendering

### Rendering Pipeline

```cpp
void InventoryWidget::Render() const
{
    if (!m_isVisible) return;

    // 1. Render dark overlay (pause background)
    RenderDarkOverlay();

    // 2. Render background GUI texture
    RenderBackground();

    // 3. Render crafting grid slots
    RenderCraftingGrid();

    // 4. Render crafting arrow
    RenderCraftingArrow();

    // 5. Render crafting output
    RenderCraftingOutput();

    // 6. Render main inventory grid
    RenderInventoryGrid();

    // 7. Render hotbar grid
    RenderHotbarGrid();

    // 8. Render cursor item (topmost layer)
    RenderCursorItem();
}
```

### Slot Rendering (Reusable)

```cpp
void InventoryWidget::RenderSlot(AABB2 const& bounds, ItemStack const& item, bool isSelected) const
{
    // Render slot background
    g_renderer->DrawTexturedQuad(bounds, m_slotTexture, AABB2::ZERO_TO_ONE, Rgba8::WHITE);

    // Render selection highlight (if selected)
    if (isSelected)
    {
        g_renderer->DrawQuadOutline(bounds, 2.0f, Rgba8::WHITE);
    }

    // Render item icon (if slot not empty)
    if (!item.IsEmpty())
    {
        ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(item.itemID);
        if (itemDef)
        {
            // Get sprite UVs from item definition
            AABB2 uvs = GetUVsForSpriteCoords(itemDef->m_spriteCoords, IntVec2(16, 16));
            Vec2 itemSize(16.0f, 16.0f);
            Vec2 itemPos = bounds.GetCenter() - itemSize * 0.5f;

            g_renderer->DrawTexturedQuad(AABB2(itemPos, itemPos + itemSize), m_itemSpriteSheet, uvs);

            // Render quantity (if > 1)
            if (item.quantity > 1)
            {
                Vec2 textPos = bounds.maxs - Vec2(2.0f, 2.0f);  // Bottom-right
                std::string quantityText = std::to_string(item.quantity);
                g_renderer->DrawText2D(textPos, quantityText, 8.0f, Rgba8::WHITE);
            }

            // Render durability bar (if tool)
            if (itemDef->m_type == eItemType::TOOL && item.durability < itemDef->m_maxDurability)
            {
                float durabilityPercent = (float)item.durability / (float)itemDef->m_maxDurability;
                Vec2 barSize(14.0f, 2.0f);
                Vec2 barPos = bounds.mins + Vec2(2.0f, bounds.GetDimensions().y - 4.0f);
                AABB2 barBounds(barPos, barPos + Vec2(barSize.x * durabilityPercent, barSize.y));

                Rgba8 barColor = Rgba8::GREEN;
                if (durabilityPercent < 0.3f) barColor = Rgba8::RED;
                else if (durabilityPercent < 0.6f) barColor = Rgba8::YELLOW;

                g_renderer->DrawQuad(barBounds, barColor);
            }
        }
    }
}
```

### Cursor Item Rendering

```cpp
void InventoryWidget::RenderCursorItem() const
{
    if (m_cursorItem.IsEmpty()) return;

    Vec2 mousePos = g_inputSystem->GetMousePosition();

    // Render item icon at mouse position
    ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(m_cursorItem.itemID);
    if (itemDef)
    {
        AABB2 uvs = GetUVsForSpriteCoords(itemDef->m_spriteCoords, IntVec2(16, 16));
        Vec2 itemSize(16.0f, 16.0f);
        AABB2 itemBounds(mousePos - itemSize * 0.5f, mousePos + itemSize * 0.5f);

        g_renderer->DrawTexturedQuad(itemBounds, m_itemSpriteSheet, uvs);

        // Render quantity
        if (m_cursorItem.quantity > 1)
        {
            Vec2 textPos = itemBounds.maxs - Vec2(2.0f, 2.0f);
            std::string quantityText = std::to_string(m_cursorItem.quantity);
            g_renderer->DrawText2D(textPos, quantityText, 8.0f, Rgba8::WHITE);
        }
    }
}
```

---

## 7. State Machines

### Inventory Screen State Machine

```
┌──────────┐
│  CLOSED  │ ◄──────┐
└──────────┘        │
     │              │
     │ Press 'E'    │ Press 'E' or ESC
     ↓              │
┌──────────┐        │
│  OPEN    │────────┘
│ (Modal)  │
└──────────┘
```

### Cursor Item State Machine

```
┌─────────────┐
│ EMPTY       │ ◄──────────┐
│ (no item)   │            │
└─────────────┘            │
     │                     │
     │ Left-Click Slot     │ Place All
     ↓                     │
┌─────────────┐            │
│ HOLDING     │────────────┘
│ (has item)  │
└─────────────┘
     │
     │ Right-Click Slot
     ↓
┌─────────────┐
│ HOLDING     │
│ (quantity-1)│
└─────────────┘
```

---

## 8. Implementation Strategy

### Phase 1: InventoryWidget Foundation (4-6 hours)
1. Create `InventoryWidget` class (extends `IWidget`)
2. Implement layout calculations (slot positions)
3. Load UI textures (background, slots, arrow)
4. Implement basic rendering (background, empty slots)
5. Test widget visibility toggle ('E' key)

### Phase 2: Mouse Interaction (6-8 hours)
1. Implement `GetSlotIndexAtPosition()` (hit testing)
2. Implement `HandleLeftClick()` (all 5 cases)
3. Implement `HandleRightClick()` (all 4 cases)
4. Implement `HandleShiftLeftClick()` (quick transfer)
5. Implement cursor item rendering
6. Test all click interactions with inventory

### Phase 3: Crafting Integration (4-6 hours)
1. Implement `UpdateCraftingOutput()` (recipe matching)
2. Implement `Recipe::Matches()` (shaped + shapeless)
3. Implement crafting output click handler
4. Implement shift-click crafting (craft multiple)
5. Test all 10 recipes from A7-Core

### Phase 4: WidgetSubsystem Refinements (2-4 hours, if needed)
1. Add modal widget support to `WidgetSubsystem`
2. Implement input routing for modal widgets
3. Test inventory modal behavior (blocks world input)
4. Test ESC key to close inventory

### Phase 5: Polish and Testing (2-4 hours)
1. Add durability bars for tools
2. Add hover effects (optional)
3. Test edge cases (full inventory, empty slots, etc.)
4. Visual polish (Minecraft fidelity check)

### Total Estimated Time: 18-28 hours (1 week)

---

**Next Steps:**
1. Approve this design.md via spec-workflow
2. Create tasks.md breaking down implementation into specific tasks
3. Begin Phase 1 (InventoryWidget Foundation)
4. Proceed sequentially through phases
