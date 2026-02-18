# Remus Theming Strategy - Comprehensive Review & Plan

## Executive Summary

The Remus application currently has a **fragmented theming architecture** where:
- Color constants are defined in C++ (`ui_theme.h`)
- A C++ QObject (`ThemeConstants`) exposes properties to QML
- Every QML control must manually bind to theme properties
- KDE Breeze style overrides these bindings, requiring explicit background/contentItem properties
- Light Mode text remains white/unreadable because controls rely on style defaults

**Root Problem**: Palette properties alone don't override KDE Breeze's defaults without explicit background customization on every single control.

---

## Current Architecture Analysis

### What We Have

**Color Definitions** (`src/core/constants/ui_theme.h`)
- 50+ color constants defined separately for Dark and Light modes
- Gruvbox palette with proper contrast ratios
- Good semantic organization (PRIMARY, SUCCESS, DANGER, etc.)

**Theme Exposure** (`src/ui/theme_constants.h/cpp`)
- ThemeConstants QObject with 40+ Q_PROPERTY getters
- Reads `QSettings("theme/darkMode", true)` on startup
- Emits `themeModeChanged()` signal for reactivity
- Properly switches between Dark/Light color sets

**QML Integration** (`src/ui/qml/*.qml`)
- MainWindow.qml sets palette at ApplicationWindow level
- Each view (SettingsView, LibraryView, etc.) has Page with palette bindings
- Individual controls should inherit palette, but KDE Breeze overrides them

### The KDE Breeze Problem

KDE Breeze style (strict palette inheritance model):
```
ApplicationWindow palette
└── Page palette (should inherit, but doesn't always)
    └── Control palette (requires explicit settings)
```

**Issue**: Controls like TextField and ComboBox in Breeze style:
- Ignore button/base palette for background color
- Use hardcoded style colors (#3c3c3c dark gray in Light Mode!)
- Require explicit **background: Rectangle** to override
- Require explicit **contentItem: Text** for custom text color

### Current Use of Theme Properties

**Properly Themed** (color bindings work):
- Labels, Text (simple color property)
- Page backgrounds (color property on Rectangle)
- Navigation buttons (custom contentItem override)

**Problematic** (palette inherited but Breeze overrides it):
- TextFields (palette.base/text ignored, background is hardcoded gray)
- ComboBoxes (palette.button/base ignored, dropdown text color wrong)
- TextAreas (similar to TextFields)
- Button text color (palette.buttonText ignored without contentItem)

---

## Qt 6 Best Practices (from provided resources)

### 1. Material Style vs Breeze Style

**Option A: Qt Quick Controls Material Style**
- Provides unified, material design-based appearance
- Accent color + primary color system
- Automatic palette cascading that works reliably
- Recommended for modern applications

**Option B: Qt Quick Controls Fusion Style (fallback)**
- More controlled than Breeze
- Better palette inheritance
- Less native-looking but more predictable

**Option C: Accept KDE Breeze + Override Everything**
- Keep current approach but be consistent
- Accept that we need `background` on many controls
- Create a reusable QML component library

### 2. Hypr-QT6-Engine & Uniform Styling

From Hyprland documentation:
- System theme environment variables (QT_STYLE_OVERRIDE)
- Theme propagation through Qt.Application.palette
- Custom palette objects in QML

**Strategy**: Set environment or use Material style for consistency across linux desktops.

---

## Identified Issues in Current Implementation

### Issue 1: Text Colors Wrong in Light Mode
**Symptom**: ComboBox text appears as default (white/light) in Light Mode
**Cause**: `palette.buttonText` binding works, but Breeze style has default text rendering
**Location**: All ComboBoxes in all views
**Solution**: Add explicit `contentItem: Text { color: ... }`

### Issue 2: Input Field Backgrounds Wrong
**Symptom**: TextFields show dark gray (#3c3c3c) background in Light Mode
**Cause**: Breeze style hardcodes `background` property; palette.base ignored
**Location**: All TextFields in SettingsView, ConversionView, GameDetailsView, VerificationView
**Solution**: Add explicit `background: Rectangle { color: theme.mainBg ... }`

### Issue 3: Inconsistent Styling Pattern
**Symptom**: Some controls have theming, others don't; no component library
**Cause**: Ad-hoc additions without systematic approach
**Solution**: Create reusable ThemedButton, ThemedTextField, ThemedComboBox components

### Issue 4: No Visual Hierarchy in Light Mode
**Symptom**: Light backgrounds (mainBg, cardBg) are too similar
**Cause**: Gruvbox light palette has limited background values
**Solution**: Ensure proper borders and visual separation via border color

---

## Implementation Plan: Three-Tier Approach

### Tier 1: Create Custom Control Library (Recommended)

**Goal**: Single source of truth for styled controls. No more palette binding in views.

**Files to Create**:
```
src/ui/qml/components/
├── ThemedButton.qml
├── ThemedTextField.qml
├── ThemedComboBox.qml
├── ThemedCheckBox.qml
├── ThemedTextArea.qml
└── ThemedLabel.qml
```

**ThemedButton.qml Example**:
```qml
Button {
    property string colorRole: "primary" // "primary", "success", "danger", etc.
    
    palette.button: theme.primary
    palette.buttonText: theme.textLight
    
    background: Rectangle {
        color: parent.highlighted ? theme.primaryHover : theme.primary
        radius: 4
        border.color: theme.border
        border.width: 1
    }
}
```

**Advantages**:
- ✅ Consistent styling across entire app
- ✅ Easy to modify theme behavior globally
- ✅ No color/palette bindings scattered in views
- ✅ Encapsulates KDE Breeze workarounds
- ✅ Testable component

**Implementation Steps**:
1. Create component directory structure
2. Implement each ThemedControl component
3. Update all QML views to use components instead of raw Controls
4. Remove redundant palette bindings from views

---

### Tier 2: Standardize Color Usage (Document + Refactor)

**Goal**: Clear semantic meaning for each color; consistent usage across views

**Create `COLOR-USAGE.md`**:
```
sidebarBg       → Sidebar background only
mainBg          → Main content area background
cardBg          → Card/contained element background
contentBg       → Content area (slightly different from mainBg)

textPrimary     → All main text (body text, labels)
textSecondary   → Secondary text (helper text, muted info)
textMuted       → Very subtle text (placeholders in content)
textPlaceholder → TextField/TextArea placeholder text

primary         → Action buttons, active states
success/warning/danger → Status indicators

border          → Normal borders (cardBg-level)
borderLight     → Subtle borders (mainBg-level)
```

**Create `TYPOGRAPHY.md`**:
```
Font sizes:      24px (title), 18px (heading), 14px (body), 12px (small)
Font weights:    Bold for headings, normal for body
Colors:          Follow textPrimary/Secondary/Muted guidelines
```

---

### Tier 3: Alternative - Use Material Style (If Tier 1 Too Complex)

**Goal**: Replace KDE Breeze with Qt Quick Controls Material style

**Changes Required**:
1. Add to CMakeLists.txt:
   ```cmake
   find_package(Qt6QuickControls2 COMPONENTS Material REQUIRED)
   ```

2. Update main.qml:
   ```qml
   import QtQuick.Controls.Material

   MaterialStyle.primary: theme.primary
   MaterialStyle.accent: theme.primary
   MaterialStyle.foreground: theme.textPrimary
   ```

3. Simplify control styling - Material handles palette cascading

**Advantages**:
- ✅ Material Design consistency across platforms
- ✅ Reliable palette inheritance (no Breeze overrides)
- ✅ Reduced per-control customization

**Disadvantages**:
- ❌ Doesn't match desktop theme (KDE Breeze)
- ❌ Requires refactoring all Button styles
- ❌ Less native appearance on Linux

---

## Recommended Path Forward: Tier 1 + Tier 2

### Phase 1: Documentation & Strategy (This Document + COLOR-USAGE)
- ✅ Define clear color semantics
- ✅ Document typography standards
- ✅ Create component design specs

### Phase 2: Component Library (Week 1)
1. Create `src/ui/qml/components/` directory
2. Implement ThemedButton, ThemedTextField, ThemedComboBox
3. Test components in isolation
4. Document component API

### Phase 3: Gradual View Migration (Week 2-3)
1. Update SettingsView to use ThemedButton, ThemedTextField, ThemedComboBox
2. Update LibraryView filters
3. Update GameDetailsView metadata fields
4. Update remaining views (ConversionView, VerificationView, ExportView)
5. Remove old palette bindings from individual views

### Phase 4: Refine & Polish (Week 4)
1. Add hover/active states to all themed controls
2. Ensure Light/Dark mode fully consistent
3. Test on multiple desktop environments (KDE Breeze, GNOME, etc.)
4. Performance optimization (memoization of theme colors)

### Phase 5: Document & Maintain
1. Create COMPONENT_USAGE.md - how to use themed controls
2. Add colors to design system doc
3. Create "Theming Checklist" for future features

---

## Color Verification Checklist

### Dark Mode (Current Implementation)
```
Background: #282828 (mainBg)
Text:       #fe8019 (textPrimary - orange, HIGH VISIBILITY)
Sidebar:    #1d2021 (sidebarBg)
Cards:      #3c3836 (cardBg)
Accent:     #8ec07c (primary - aqua)
Borders:    #504945 (border)
```

### Light Mode (Currently Broken)
```
Background: #f9f5d9 (mainBg - cream)
Text:       #282828 (textPrimary - BLACK, not white!)
Sidebar:    #fbf1c7 (sidebarBg - lighter cream)
Cards:      #ebdbb2 (cardBg - tan)
Accent:     #689d6a (primary - dim aqua)
Borders:    #d5c4a1 (border - light gray)
```

**Problem**: Light Mode template is correct in code, but controls show white text because Breeze style defaults override our settings.

---

## Files to Review/Modify

### Current Files
- `src/core/constants/ui_theme.h` - ✅ Colors correct
- `src/ui/theme_constants.h/cpp` - ✅ Properly exposes theme
- `src/ui/qml/MainWindow.qml` - ✅ Has page-level palette
- `src/ui/qml/*.View.qml` - ⚠️ Has palette but KDE Breeze overrides

### To Create (New)
- `src/ui/qml/components/ThemedButton.qml`
- `src/ui/qml/components/ThemedTextField.qml`
- `src/ui/qml/components/ThemedComboBox.qml`
- `src/ui/qml/components/ThemedCheckBox.qml`
- `src/ui/qml/components/ThemedTextArea.qml`
- `docs/COLOR-USAGE.md`
- `docs/TYPOGRAPHY.md`
- `docs/COMPONENT-USAGE.md`

### To Document
- Create `THEMING-ARCHITECTURE.md` - how theming system works

---

## Quick Win Option (If Tier 1 Takes Too Long)

Instead of full component library, systematically add:

```qml
TextField {
    // ... existing properties
    color: theme.textPrimary  // Add this
    background: Rectangle {   // Add this
        color: theme.mainBg
        border.color: theme.border
        border.width: 1
        radius: 4
    }
}

ComboBox {
    // ... existing properties
    background: Rectangle {   // Add this
        color: theme.mainBg
        border.color: theme.border
        border.width: 1
        radius: 4
    }
    contentItem: Text {       // Add this
        text: displayText
        color: theme.textPrimary
    }
}
```

**Drawback**: Still scattered theming code, but fixes immediately.
**Timeline**: 2-3 hours
**Benefit**: Immediate Light Mode visual fix

---

## Decision Point

**Recommend Tier 1 + Tier 2**: Better architecture, future-proof
**If Time-Critical**: Use "Quick Win" approach first, refactor to components later

What would you prefer? I can start immediately with whichever approach you choose.
