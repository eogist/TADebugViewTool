# Changelog

## 1.4.0 - 2026-08-15

### Fixed

- Made workflow JSON loading atomic so an invalid document never exposes a valid prefix.
- Write-protected invalid override files and prevented stale editor caches from overwriting external changes.
- Made override saves transactional with unique staging files and required backups.
- Replaced silent legacy workflow migration with an explicit project import that preserves legacy commands.
- Unified workflow action validation and case-insensitive workflow identity handling.
- Made Reset Debug reliably restore Lit mode and clear visualization, stat, Bounds, Navigation, and VSM cached-page state.
- Corrected Semantic Version precedence for prerelease releases and restricted release links to the official repository.

### Changed

- Renamed user-created shared workflows to Project Created.
- Cached update checks at module scope so automatic checks run at most once per editor session.
- Added a personal preference for automatic update checks.
- Added Unreal Automation coverage for JSON parsing, schema versions, legacy command preservation, Reset coverage, Semantic Version comparison, and trusted release URLs.

### Added

- Added project workflow recovery guidance, publisher metadata, and a plugin browser icon.
