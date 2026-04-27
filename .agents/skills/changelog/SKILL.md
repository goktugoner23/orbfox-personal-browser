---
name: changelog
description: Update or create README.md and CHANGELOG.md files for the project.
---

# Changelog Skill

Update or create README.md and CHANGELOG.md files for the project.

## Trigger

User executes `/changelog` command.

## Behavior

1. **Locate files** - Check for README.md and CHANGELOG.md at the root of the codebase
2. **Read current state** - If files exist, read their contents to understand the current structure
3. **Gather changes** - Review recent work in the conversation to identify:
   - New features added
   - Changes to existing features
   - Bug fixes
   - Technical improvements
   - Breaking changes
4. **Update CHANGELOG.md**:
   - Follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format
   - Add entries under `[Unreleased]` or create a new version section
   - Categorize changes: Added, Changed, Fixed, Removed, Deprecated, Security
   - Use clear, concise descriptions
5. **Update README.md**:
   - Update Features section if new features were added
   - Update any outdated information
   - Keep the existing structure and style
6. **Create if missing**:
   - If README.md doesn't exist, create a basic one with project name, description, features, and build instructions
   - If CHANGELOG.md doesn't exist, create one with the Keep a Changelog format

## CHANGELOG Format

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes to existing features

### Fixed
- Bug fixes

### Removed
- Removed features

### Deprecated
- Soon-to-be removed features

### Security
- Security fixes
```

## README Structure

If creating a new README, include:
- Project name and description
- Features list
- Requirements
- Build/installation instructions
- Usage examples (if applicable)
- License

## Notes

- Always read existing files before modifying to preserve style
- Use semantic versioning for version numbers
- Date format: YYYY-MM-DD
- Be concise but descriptive in changelog entries
