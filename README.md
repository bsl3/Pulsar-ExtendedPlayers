# Extended Players for Pulsar
Developed with supplemental AI assistance 

Adds offline 16-, 18- and 24-player GP/VS races, including local multiplayer. You can also choose 12 players.

## Adding to your own pulsar based project

### 1. Copy the source folders

Copy the contents of `PulsarEngine/` and `GameSource/` 

On a clean Pulsar project, replace matching files. If you have already edited them, compare the files and keep your own changes if needed

### 2. Add the symbols

Add the entries from this folder's `symbols.txt` to your project's `GameSource/symbols.txt`.

### 3. Update the version entries

add the game versions to your game versions list

Open this folder's `gameversions.txt` beside your project's `GameSource/versions.txt`. 

For each section shown (`[P]`, `[E]`, etc.), remove the exact lines marked `# REMOVE:` from that existing section, then add the entries under `# ADD:`. Keep all other lines. Use the section headings to find the right place; do not create duplicate sections.

Some existing ranges must be replaced, not just appended. Removing the marked lines first prevents overlapping mappings. If you previously changed one of those ranges yourself, compare it before replacing it.


### 4. Configure

In RaceAssets.szs\game_image\timg add files for player position icons
tt_position_no_st_64x64_XX.tpl (single player)
tt_multi_position_no_st_64x64_XX.tpl (multiplayer)
XX represents 13-24


Open `PulsarEngine/ExtendedPlayers/Config.hpp`. Set `PlayerCount` to `12`, `16`, `18` or `24`. `LargeResults` chooses the alternate results layout.


## ItemSlot24 probabilities

Put the editor's exported **`ItemSlot24.bin` at the root of `CommonAssets.szs`**.

- 1–12 racers: normal item tables and track `.slt` behavior are unchanged.
- 13–24 racers: it will then use itemSlot24.bin

Each Player/CPU column must total 100%.


## Compatibility and testing

Offline GP/VS only. Other modes keep their normal player counts. Offline KO is not included. Missing extra rank textures use generated numbers. No online

