# Riftwalker Installer (Windows)

This folder contains an [Inno Setup](https://jrsoftware.org/isdl.php) script that
packages the Release build into a standard Windows installer.

## Build the installer

1. Install Inno Setup 6 (free): https://jrsoftware.org/isdl.php
2. Build the Release binary first:
   ```bash
   cd ../build
   cmake --build . --config Release
   ```
3. Compile the installer:
   ```bash
   iscc Riftwalker.iss
   ```
   Or open `Riftwalker.iss` in the Inno Setup IDE and press `F9`.

The output file appears at `installer\Output\Riftwalker-v1.0.0-Setup.exe` (~75–80 MB).

## What the installer does

- Default location: `Program Files\Riftwalker` (per-machine) or
  `%LocalAppData%\Programs\Riftwalker` (per-user, no admin required).
- Creates a Start Menu group with shortcuts for: game, README, Changelog, Uninstaller.
- Optional desktop shortcut (checkbox during install).
- Bundled languages: English + German (matches the game).
- 64-bit only (the game requires x64).
- Uninstaller removes the install directory **and** save files in the app dir.

## What the installer does NOT do

- It does not install the Visual C++ Redistributable. Players running on Windows
  10/11 already have everything needed for an MSVC-built x64 binary.
- It does not download Steamworks. The Steamworks SDK is opt-in via
  `cmake -DENABLE_STEAMWORKS=ON ..` and requires a Steam partner account to
  set up correctly.
