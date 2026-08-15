# TA Debug View Tool

TA Debug View Tool is an editor-only UE5 plugin for technical artists. It provides a compact panel for quickly switching common viewport debug views, running multi-step rendering inspection workflows, and keeping high-frequency actions close at hand.

## Goals

- Reduce the cost of repeatedly opening UE viewport debug menus.
- Put Nanite, Lumen, Virtual Shadow Map, material, lighting, geometry, and performance checks in one panel.
- Support repeatable TA workflows instead of one-off console command typing.
- Remember the user's last panel state, favorites, viewport target, and update-check preference.

## Main Features

- **Workflows**: a unified workflow grid with Default, Modified, and Project Created states for Material Cost, Nanite Audit, Lumen Check, VSM Cache, Collision QA, Performance HUD, Reset Debug, and custom checks. Custom workflows can choose their own persisted card icon.
- **Debug Views**: grouped viewport visualization presets for ViewMode, Nanite, Lumen, VSM, lighting, materials, and geometry.
- **Commands**: a small curated set of VSM, geometry, and performance commands, grouped for quick access without duplicating the full UE console.
- **Context Inspector**: inspect, run, edit, reset, or delete the selected workflow without leaving the main panel.
- **Quick Access**: Favorites above the page navigation, displayed in responsive pages of five, three, or two items with mouse-wheel paging, clickable page dots, and an all-favorites dropdown.
- **Responsive UI**: Wide, medium, and compact layouts adapt the navigation rail, Workflow grid, Context Inspector, and vertical density to the available dock size. Per-window DPI compensation follows the panel when it moves between monitors.
- **Viewport Target**: Active, Perspective, and All target modes for applying debug views across editor viewports.
- **Keyboard Shortcuts**: fixed shortcuts for opening the panel, resetting debug state, and executing the first five favorites.
- **Help UI**: in-panel reference for shortcuts, quick access behavior, pages, and viewport targets.
- **Update Check**: optionally checks the latest public GitHub Release once per editor session, correctly compares stable and prerelease versions, and provides manual retry/open-release controls in Help. It never downloads or installs files automatically.
- **Action Launcher**: search built-in views, effective workflows, curated commands, and every console command or CVar registered in the current UE session; press Enter to execute the first result.
- **Preset Diagnostics**: validate the default/override workflow registry, import legacy personal workflows into project-shared storage, and clean stale Favorites references.

## Default Shortcuts

| Shortcut | Action |
| --- | --- |
| `Alt + Shift + D` | Open or focus the TA Debug Views panel |
| `Alt + Shift + 0` | Execute Reset Debug |
| `Alt + Shift + 1` | Execute Favorites item 1 |
| `Alt + Shift + 2` | Execute Favorites item 2 |
| `Alt + Shift + 3` | Execute Favorites item 3 |
| `Alt + Shift + 4` | Execute Favorites item 4 |
| `Alt + Shift + 5` | Execute Favorites item 5 |

These commands appear in Editor Preferences > Keyboard Shortcuts under the TA Debug Views command context.

The panel opens only from its toolbar/menu command or shortcut. Leaving it open when the editor exits does not make it reopen automatically on the next editor launch.

The panel has no fixed minimum desired size. It switches between wide, medium, and compact layouts using the effective Slate space after local DPI compensation. On compact layouts, Context Details moves into a popup instead of consuming a fixed right column.

## Default Favorites

On first use, Favorites are initialized to:

1. Material Cost
2. Nanite Audit
3. Lumen Check
4. VSM Cache

The default set is only created once. If the user clears or edits Favorites, the plugin will not automatically fill them again.

## Persistence

Built-in workflow definitions are stored in:

`Plugins/TADebugViewTool/Resources/DefaultWorkflows.json`

Project workflow overrides and project-created workflows are stored in:

`Project/Config/TADebugViewTool/WorkflowOverrides.json`

This is team-visible project data and may be committed to source control. The runtime registry merges both files by stable workflow Id:

- A default workflow with no override is reported as **Default**.
- A default workflow with a matching override is reported as **Modified**.
- An override with a new Id is reported as **Project Created**.
- Resetting a modified default deletes only its override and exposes the plugin default again.
- Deleting a project-created workflow removes only that override entry.

Override loading is all-or-nothing. If the file is unreadable or any entry is invalid, the plugin uses its defaults and write-protects the override file instead of applying or overwriting a valid prefix. Repair the JSON, restore `WorkflowOverrides.json.bak`, or revert the file from source control before saving again.

Saves use a uniquely named validated temporary file, require a successful `.bak` backup, and compare the current file fingerprint before replacement so another editor or external edit is not overwritten. Legacy personal workflows are shown in Diagnostics and are imported only after explicit confirmation into project-shared storage.

Personal panel preferences remain in `UTADebugViewCustomPresetSettings` as editor per-project user settings.

The plugin remembers:

- Favorite actions
- Last viewport target
- Last panel page
- Last Debug Views category
- Whether default quick access favorites have already been initialized
- Whether automatic update checks are enabled
- Whether legacy workflows have been explicitly imported into project storage

## Code Structure

- `TADebugViewToolModule.cpp`: module startup, tab registration, menu and toolbar entries, command binding, shortcut execution.
- `TADebugViewToolCommands.*`: fixed UE editor commands and default keyboard chords.
- `STADebugViewPanel.*`: responsive Slate panel, DPI compensation, workflow grid, Commands browser, runtime console search, inline context editor, quick access, diagnostics, and help UI.
- `TADebugViewExecutor.*`: applies view modes, visualization modes, workflow state capture, workflow restore, and console commands.
- `TADebugViewPresetRegistry.*`: built-in debug view definitions and compiled workflow fallback data.
- `TADebugViewWorkflowRegistry.*`: loads, validates, merges, migrates, saves, resets, and deletes workflow overrides.
- `TADebugViewCustomPresetSettings.*`: personal panel state, legacy migration data, and quick access references.
- `TADebugViewQuickActionRuntime.*`: shared runtime helper for resolving and executing favorites, workflows, debug presets, and runtime console commands, and cleaning stale references.
- `TADebugViewUpdateService.*`: module-lifetime GitHub release check, SemVer comparison, trusted release links, and per-session request caching.
- `TADebugViewPresetDiagnostics.*`: effective-registry validation and stale-reference reporting.

## Current Scope

This plugin currently focuses on editor workflow speed. It does not add runtime game UI, dynamic shortcut generation for every preset, or automated screenshot/capture workflows yet.

## Suggested Next Phases

- Optional capture workflows for turning on a debug state and taking viewport screenshots.
- Extend the existing Automation Tests with viewport-driven Reset and multi-editor conflict smoke coverage.
