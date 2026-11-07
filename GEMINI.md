---
trigger: always_on
---

# DriverNest Project Directives & Design Standards

## UI/UX & Experience Standards
For every UI, visual component, interaction, layout, or design change in DriverNest:
1. **Always use `ui-ux-pro-max`**: Design system tokens, telemetry data matrices, intentional color theory (Aerospace Deep Chassis), high contrast accessibility, and responsive density.
2. **Always use `frontend-design`**: Bespoke industrial instrumentation styling, precision GDI rendering, micro-interactions (hover grips, shimmer skeletons, custom checkbox ticks, splitter drags).
3. **Always use `scroll-experience`**: Fluid list-view physics, momentum and wheel scrolling ergonomics, smooth sticky telemetry decks, and visual feedback during interaction.

## Release & Deployment Workflow
- The application version must be updated consistently across:
  - `DriverNest/CMakeLists.txt`
  - `DriverNest/src/ui/Strings.h` (`AppVersion`)
  - `DriverNest/src/app/App.cpp`
  - `README.md`
  - `CHANGELOG.md`
- Binaries are built and released automatically via GitHub Actions (`.github/workflows/build-and-release.yml`).
- Every push to `main` and release tag triggers automated CI builds, unit tests, and GitHub Releases with downloadable assets.
