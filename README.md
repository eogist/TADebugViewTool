# TA Debug View Tool

TA Debug View Tool is an editor-only UE5 plugin for technical artists. It provides a compact panel for quickly switching common viewport debug views, running multi-step rendering inspection workflows, and keeping high-frequency actions close at hand.

## Goals

- Reduce the cost of repeatedly opening UE viewport debug menus.
- Put Nanite, Lumen, Virtual Shadow Map, material, lighting, geometry, and performance checks in one panel.
- Support repeatable TA workflows instead of one-off console command typing.
- Remember the user's last panel state, favorites, recent actions, and viewport target.

## Main Features

- **Workflows**: a unified workflow grid with Default, Modified, and User Created states for Material Cost, Nanite Audit, Lumen Check, VSM Cache, Collision QA, Performance HUD, Reset Debug, and custom checks.
- **Debug Views**: grouped viewport debug presets for ViewMode, Nanite, Lumen, VSM, lighting, materials, geometry, and performance.
- **Context Inspector**: inspect, run, edit, reset, or delete the selected workflow without leaving the main panel.
- **Quick Access**: Favorites above the page navigation, displayed five per page with mouse-wheel paging, clickable page dots, and an all-favorites dropdown.
- **Viewport Target**: Active, Perspective, and All target modes for applying debug views across editor viewports.
- **Keyboard Shortcuts**: fixed shortcuts for opening the panel, resetting debug state, and executing the first five favorites.
- **Help UI**: in-panel reference for shortcuts, quick access behavior, pages, and viewport targets.
- **Action Launcher**: search built-in views and effective workflows from one field; press Enter to execute the first result.
- **Preset Diagnostics**: validate the default/override workflow registry and clean stale Favorites / Recent references.

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

Project workflow overrides and user-created workflows are stored in:

`Project/Config/TADebugViewTool/WorkflowOverrides.json`

The runtime registry merges both files by stable workflow Id:

- A default workflow with no override is reported as **Default**.
- A default workflow with a matching override is reported as **Modified**.
- An override with a new Id is reported as **User Created**.
- Resetting a modified default deletes only its override and exposes the plugin default again.
- Deleting a user-created workflow removes only that override entry.

Override saves use a validated temporary file and preserve the previous file as `.bak` before replacement. Existing legacy workflows from editor per-project user settings are migrated once into the override file.

Personal panel preferences remain in `UTADebugViewCustomPresetSettings` as editor per-project user settings.

The plugin remembers:

- Favorite actions
- Recent actions, capped at five
- Last viewport target
- Last panel page
- Last Debug Views category
- Whether default quick access favorites have already been initialized
- Whether legacy workflow overrides have already been migrated

## Code Structure

- `TADebugViewToolModule.cpp`: module startup, tab registration, menu and toolbar entries, command binding, shortcut execution.
- `TADebugViewToolCommands.*`: fixed UE editor commands and default keyboard chords.
- `STADebugViewPanel.*`: three-column Slate panel, workflow grid, inline context editor, quick access, diagnostics, and help UI.
- `TADebugViewExecutor.*`: applies view modes, visualization modes, workflow state capture, workflow restore, and console commands.
- `TADebugViewPresetRegistry.*`: built-in debug view definitions and compiled workflow fallback data.
- `TADebugViewWorkflowRegistry.*`: loads, validates, merges, migrates, saves, resets, and deletes workflow overrides.
- `TADebugViewCustomPresetSettings.*`: personal panel state, legacy migration data, and quick access references.
- `TADebugViewQuickActionRuntime.*`: shared runtime helper for resolving quick actions, executing favorites/workflows/debug presets, writing Recent, and cleaning stale references.
- `TADebugViewPresetDiagnostics.*`: effective-registry validation and stale-reference reporting.

## Current Scope

This plugin currently focuses on editor workflow speed. It does not add runtime game UI, dynamic shortcut generation for every preset, or automated screenshot/capture workflows yet.

## Suggested Next Phases

- Optional capture workflows for turning on a debug state and taking viewport screenshots.
- Optional automation tests for override migration, JSON round trips, diagnostics, and quick-action resolution.
