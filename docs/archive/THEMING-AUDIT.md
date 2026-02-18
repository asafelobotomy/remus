# Comprehensive Theming Audit Checklist

## Current State Assessment

### Color Constants Review
- [x] Dark Mode colors defined in ui_theme.h
- [x] Light Mode colors defined in ui_theme.h
- [x] Color contrast ratios verified
- [ ] Document which colors are unused
- [ ] Verify all 50+ constants are intentional

### Theme Infrastructure
- [x] ThemeConstants QObject created
- [x] Q_PROPERTY getters for all colors
- [x] setDarkMode() implementation working
- [x] QSettings persistence working
- [x] themeModeChanged() signal emitting
- [ ] Performance test (signal overhead?)

### QML Implementation Analysis

#### MainWindow.qml
- [x] Has ApplicationWindow.palette bindings (good!)
- [x] Has sidebar theming
- [x] Has navigation button styling
- [ ] Check all Button elements for proper styling
- [ ] Verify StackView content area is properly themed

#### View Files (10 total)
Each view needs verification:

**SettingsView.qml**
- [x] Page has background: Rectangle color binding
- [x] Page has palette bindings
- [ ] Check: How many TextFields? (Should be 5+)
- [ ] Check: How many ComboBoxes? (Should be 3+)
- [ ] Check: Do TextFields show dark backgrounds in Light Mode?
- [ ] Check: Do ComboBoxes show white text in Light Mode?

**LibraryView.qml**
- [ ] System filter ComboBox styling
- [ ] "Matched only" CheckBox styling
- [ ] File list item styling

**MatchReviewView.qml**
- [ ] Confidence filter ComboBox
- [ ] Match list items
- [ ] Details panel styling

**ConversionView.qml**
- [ ] Input path TextFields (3 of them)
- [ ] Codec ComboBox
- [ ] Browse buttons

**GameDetailsView.qml**
- [ ] 8 metadata TextFields (Title, System, Region, Year, Publisher, Developer, Genre, Players)
- [ ] Description TextArea
- [ ] Edit mode toggle CheckBox
- [ ] Artwork display

**VerificationView.qml**
- [ ] DAT import controls
- [ ] System ComboBox
- [ ] Search TextField
- [ ] Filter ComboBox
- [ ] Results list

**ExportView.qml**
- [ ] Export type ComboBox
- [ ] Emit/checkbox controls
- [ ] Export button

**PatchView.qml & ArtworkView.qml**
- [ ] Any input fields?
- [ ] Any controls?

### KDE Breeze Style Compatibility

**Confirmed Issues**:
- [ ] TextField background color (should be theme.mainBg, shows gray)
- [ ] ComboBox text color (should be theme.textPrimary, shows white)
- [ ] ComboBox background (should be theme.mainBg, shows gray)
- [ ] TextArea background (should be theme.mainBg, shows gray)
- [ ] Button text (inconsistent across views)

**Breeze Workarounds Needed**:
- [ ] TextField: Add background: Rectangle
- [ ] TextField: Add color property
- [ ] ComboBox: Add background: Rectangle
- [ ] ComboBox: Add contentItem: Text
- [ ] TextArea: Add background: Rectangle
- [ ] TextArea: Add color property

### Light Mode Specific Tests

Test in Light Mode:
- [ ] Sidebar text is dark (#282828), not orange
- [ ] Main content background is cream (#f9f5d9)
- [ ] Card backgrounds are tan (#ebdbb2)
- [ ] All text is #282828 (black), not white
- [ ] Placeholder text is #504945 (medium gray)
- [ ] Accent color is #689d6a (dim aqua), not bright
- [ ] Buttons have proper contrast
- [ ] Input fields are readable

### Dark Mode Verification

Test in Dark Mode:
- [ ] Sidebar background is very dark (#1d2021)
- [ ] Main content background is dark (#282828)
- [ ] Text is orange (#fe8019) for good contrast
- [ ] Accent color is bright aqua (#8ec07c)
- [ ] All controls are distinguishable from background

---

## Component Library Creation Checklist

### ThemedButton.qml
- [ ] Accepts `text` property
- [ ] Accepts `colorRole` property (primary, success, danger, etc.)
- [ ] Implements background Rectangle with proper styling
- [ ] Implements hover state
- [ ] Implements pressed state
- [ ] Implements disabled state
- [ ] Binds to theme.themeModeChanged signal (live update)
- [ ] Compatible with iconstext combinations
- [ ] Tested in Light Mode
- [ ] Tested in Dark Mode

### ThemedTextField.qml
- [ ] Inherits from TextField
- [ ] Auto-applies palette bindings
- [ ] Auto-applies background Rectangle
- [ ] Shows theme.mainBg background
- [ ] Shows theme.textPrimary text
- [ ] Shows theme.textMuted placeholder
- [ ] Has border with theme.border color
- [ ] Works with readOnly property
- [ ] Tested in Light Mode
- [ ] Tested in Dark Mode

### ThemedComboBox.qml
- [ ] Inherits from ComboBox
- [ ] Auto-applies palette bindings
- [ ] Auto-applies background Rectangle
- [ ] Auto-applies contentItem text styling
- [ ] Shows theme.mainBg background
- [ ] Shows theme.textPrimary text
- [ ] Has proper dropdown styling
- [ ] Tested in Light Mode
- [ ] Tested in Dark Mode

### ThemedCheckBox.qml
- [ ] Auto-applies palette.text binding
- [ ] Tested with theme.textPrimary color
- [ ] Tested in Light Mode
- [ ] Tested in Dark Mode

### ThemedTextArea.qml
- [ ] Inherits from TextArea
- [ ] Auto-applies palette bindings
- [ ] Auto-applies background Rectangle
- [ ] Shows theme.mainBg background
- [ ] Shows theme.textPrimary text
- [ ] Has border with theme.border color
- [ ] Tested in Light Mode
- [ ] Tested in Dark Mode

---

## View Migration Checklist

For each view, track:
- [ ] Audit complete (identified all controls)
- [ ] Components identified (how many of each type)
- [ ] Replacements implemented (old → new)
- [ ] Light Mode tested
- [ ] Dark Mode tested
- [ ] Interactions still work
- [ ] No regression

### SettingsView.qml
- Components to replace: 3x ComboBox, 5x TextField
- Status: [ ] Audit [ ] Replace [ ] Test

### LibraryView.qml
- Components to replace: 1x ComboBox
- Status: [ ] Audit [ ] Replace [ ] Test

### MatchReviewView.qml
- Components to replace: 1x ComboBox
- Status: [ ] Audit [ ] Replace [ ] Test

### ConversionView.qml
- Components to replace: 1x ComboBox, 3x TextField
- Status: [ ] Audit [ ] Replace [ ] Test

### GameDetailsView.qml
- Components to replace: 8x TextField, 1x TextArea
- Status: [ ] Audit [ ] Replace [ ] Test

### VerificationView.qml
- Components to replace: 2x ComboBox, 1x TextField
- Status: [ ] Audit [ ] Replace [ ] Test

### ExportView.qml
- Components to replace: 1x ComboBox
- Status: [ ] Audit [ ] Replace [ ] Test

### PatchView.qml
- Components to replace: [ TBD ]
- Status: [ ] Audit [ ] Replace [ ] Test

### ArtworkView.qml
- Components to replace: [ TBD ]
- Status: [ ] Audit [ ] Replace [ ] Test

---

## Documentation Checklist

- [ ] THEMING-STRATEGY.md (✓ Done)
- [ ] COLOR-USAGE.md (semantics for each color)
- [ ] TYPOGRAPHY.md (font sizes, weights, line heights)
- [ ] COMPONENT-USAGE.md (how to use ThemedButton, etc.)
- [ ] THEMING-ARCHITECTURE.md (how theme system works end-to-end)
- [ ] Update README.md with theme info
- [ ] Add inline code comments for theme properties

---

## Testing Checklist

### Manual Visual Tests
- [ ] Light Mode: All text readable
- [ ] Light Mode: All input fields visible
- [ ] Light Mode: All buttons distinct from background
- [ ] Light Mode: Proper color hierarchy
- [ ] Dark Mode: All text visible
- [ ] Dark Mode: Proper contrast
- [ ] Dark Mode: Accent colors stand out

### Functional Tests
- [ ] Theme toggle works (Settings)
- [ ] Theme persists on restart
- [ ] All controls respond to theme change
- [ ] No lag when switching themes
- [ ] Hover/active states work

### Cross-Platform Tests
- [ ] KDE Breeze desktop
- [ ] GNOME desktop (if available)
- [ ] Custom theme environment
- [ ] Default Qt style (Fusion, Windows, etc.)

---

## Performance Considerations

- [ ] Count Q_PROPERTY getters (40+?)
- [ ] Measure themeModeChanged signal emissions
- [ ] Check for excessive binding re-evaluations
- [ ] Profile QML rendering with new components
- [ ] Optimize theme property access (memoization?)

---

## Decision Matrix

| Approach         | Timeline | Complexity | Quality | Maintainability |
|-----------------|----------|-----------|---------|-----------------|
| Tier 1 (Full Components) | 4 weeks | High | Excellent | Excellent |
| Tier 2 (Quick Win) | 2-3 hrs | Low | Good | Fair |
| Tier 3 (Material Style) | 3 weeks | Medium | Good | Good |

**Recommendation**: Start with Tier 2 for immediate visual fix, plan Tier 1 for refactor.

---

## Next Steps (Your Input Needed)

1. Which approach do you want to implement first?
   - [ ] Tier 1 (Component Library - best long-term)
   - [ ] Tier 2 (Quick Win - fixes Light Mode immediately)
   - [ ] Hybrid (Quick Win first, then refactor to Tier 1)

2. Should I audit all views first, or start fixing based on screenshots?

3. Do you want to keep Breeze or switch to Material style?

4. Any specific areas to prioritize?
