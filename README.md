# Discord Rich Presence for Resident Evil 2 Remake

A simple REFramework mod that shows what you're doing in Resident Evil 2 Remake on your Discord profile!

## What it shows
- **Character:** Leon or Claire
- **Scenario:** Leon A / Leon B / Claire A / Claire B
- **Difficulty:** Assisted, Standard, Hardcore
- **HP Status:** Fine, Caution, Danger, Poison

Everything is fully customizable through `config.ini`, and you can translate text using `Discord_Presence_RE2R_Translation.ini`.

## How to install
1. Make sure you have **REFramework** installed.
2. Download the mod.
3. Drop `DiscordPresenceRE2R.dll` right into your `reframework/plugins/` folder.

## Building from source
If you want to compile mod yourself, just use `build.bat` script or use `CMakeLists.txt` for your favorite IDE.

## For Developers
File `reframework/autorun/discord_scout.lua` is included just to help find game values and testing things out. You don't need it for normal use.

## Credits
Inspiration: [Discord Rich Presence (RE4 Remake)](https://www.nexusmods.com/residentevil42023/mods/1449) by TommInfinite

## License
Check the [LICENSE](LICENSE) file.
