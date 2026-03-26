# Research URL Tracker — Remus

> Living document. Append rows as new useful URLs are discovered. All agents may update this file.
> Do not delete rows — mark stale entries with `(stale)` in the Summary column.
>
> **Setup note**: Seed the tables below with links relevant to Remus's stack and domain.

## VS Code Copilot — AI Customisation

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://code.visualstudio.com/docs/copilot/customization/custom-agents | Custom agents documentation | 2026-03-19 | agents, customisation |
| https://code.visualstudio.com/docs/copilot/reference/copilot-vscode-features#_chat-tools | Built-in tool reference list | 2026-03-19 | tools, reference |

## Project-Specific Resources

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://docs.retroachievements.org/developer-docs/game-identification.html | Per-system hashing algorithms used by RetroAchievements (header stripping, partial hashing, custom buffers) | 2026-03-23 | hashing, retroachievements, identification |
| https://wiki.no-intro.org/index.php?title=Naming_Convention | No-Intro naming convention; [b] bad/hacked flag; Status field for dumps | 2026-03-23 | no-intro, naming, dat |
| https://www.tosecdev.org/tosec-naming-convention | TOSEC naming convention v4 — full dump-flag taxonomy: [h] [tr] [f] [cr] [t] [m] [p] [b] [!] | 2026-03-23 | tosec, naming, hacks, translations |
| https://hasheous.org/swagger/index.html | Hasheous API v1 — hash lookup by CRC/MD5/SHA1/SHA256; ArchiveObservation submission; no auth required | 2026-03-23 | hasheous, api, hash-lookup |
| https://github.com/gaseous-project/hasheous | Hasheous source (AGPL-3.0); supports TOSEC/No-Intro/Redump/MAME; IGDB proxy | 2026-03-23 | hasheous, open-source, dat |
| https://emulation.gametechwiki.com/index.php/File_hashes | Overview of hash databases for emulation verification; multi-track disc guide | 2026-03-23 | hashing, reference, verification |
| https://github.com/Alcaro/Flips | Flips — BPS/IPS patch tool (GPL); BPS format specification links | 2026-03-23 | patching, bps, ips, flips |

## Frontend ROM Folder Naming Conventions (2026-05-24)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://emudeck.github.io/cheat-sheet/ | EmuDeck official cheat sheet — complete ROM folder names, emulators, extensions, and BIOS for every supported platform | 2026-05-24 | emudeck, folder-names, roms, reference |
| https://gitlab.com/es-de/emulationstation-de/-/raw/master/resources/systems/linux/es_systems.xml | ES-DE es_systems.xml — authoritative `<name>` tags = folder names for every supported system | 2026-05-24 | es-de, emulationstation, folder-names, xml |
| https://wiki.batocera.org/systems | Batocera system list wiki — folder names for all supported platforms | 2026-05-24 | batocera, folder-names, wiki |
| https://raw.githubusercontent.com/rommapp/romm/master/backend/handler/metadata/base_handler.py | RomM base_handler.py — defines `UniversalPlatformSlug` StrEnum; enum values ARE the folder names RomM expects | 2026-05-24 | romm, folder-names, python, source |
| https://raw.githubusercontent.com/rommapp/romm/master/backend/handler/metadata/igdb_handler.py | RomM igdb_handler.py — `IGDB_PLATFORM_LIST` dict mapping UPS → IGDB IDs/slugs/categories | 2026-05-24 | romm, igdb, platforms, python, source |
| https://docs.romm.app/latest/Platforms-and-Players/Supported-Platforms/ | RomM official supported platforms list — platform display name to folder-name mapping (authoritative human-readable list) | 2026-05-24 | romm, folder-names, docs, reference |
| https://docs.romm.app/latest/Getting-Started/Folder-Structure/ | RomM folder structure docs — Structure A vs B, naming conventions, tag support | 2026-05-24 | romm, folder-structure, docs |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-genesis-plus-gx.sh | RetroPie lr-genesis-plus-gx — confirms: megadrive, mastersystem, gamegear, sg-1000 (hyphen), segacd | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-fbneo.sh | RetroPie lr-fbneo — confirms: arcade, neogeo, fba, pcengine, coleco (not colecovision), ngp, ngpc, zxspectrum | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-virtualjaguar.sh | RetroPie lr-virtualjaguar — confirms: atarijaguar (with atari prefix) | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-beetle-lynx.sh | RetroPie lr-beetle-lynx — confirms: atarilynx (with atari prefix) | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-beetle-wswan.sh | RetroPie lr-beetle-wswan — confirms: wonderswan, wonderswancolor | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-flycast.sh | RetroPie lr-flycast — confirms: dreamcast (not dc) | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-opera.sh | RetroPie lr-opera — confirms: 3do | 2026-05-24 | retropie, folder-names, source |
| https://raw.githubusercontent.com/RetroPie/RetroPie-Setup/master/scriptmodules/libretrocores/lr-bluemsx.sh | RetroPie lr-bluemsx — confirms: coleco (abbreviated, not colecovision) | 2026-05-24 | retropie, folder-names, source |
| https://gamesdb.launchbox-app.com/ | LaunchBox Games Database — Platform display names (not folder names; LaunchBox is folder-agnostic) | 2026-05-24 | launchbox, platform-names, reference |

## Retro Game Metadata Sources Research (2026-03-26)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://datomatic.no-intro.org/ | No-Intro DAT-o-MATIC — web search + download portal for verified ROM hash DATs; no API; CRC/MD5/SHA1; non-disc focus | 2026-03-26 | no-intro, dat, hash, roms |
| https://www.tosecdev.org/downloads | TOSEC full DAT pack download; 4,245 DATs, 1.29M ROMs; last release 2025-03-13; XML format | 2026-03-26 | tosec, dat, hash, download |
| http://redump.org/ | Redump disc preservation project; actively updated; CRC/MD5/SHA1; disc-based platforms; no API; DAT downloads require registration | 2026-03-26 | redump, dat, hash, disc, preservation |
| https://github.com/libretro/libretro-database | libretro-database CC-BY-SA-4.0; DAT+RDB format; 80+ systems; sources No-Intro/Redump/TOSEC/MAME/GameTDB; includes metadata DATs with developer/genre/rating | 2026-03-26 | libretro, dat, hash, metadata, open-source |
| https://github.com/libretro/libretro-thumbnails | libretro-thumbnails; PNG boxart/screenshots/title screens; 80+ systems; no auth; updated every ~2 days; URL: thumbnails.libretro.com | 2026-03-26 | libretro, artwork, thumbnails, boxart |
| https://github.com/OpenVGDB/OpenVGDB | OpenVGDB SQLite database; ROM hashes + metadata (title/desc/genre/publisher/developer); last release v29.0 Nov 2021; effectively abandoned | 2026-03-26 | openvgdb, sqlite, metadata, stale |
| https://www.mobygames.com/api/ | MobyGames REST API v1; requires paid API key (hobbyist tier available); 720 req/hr; titles/descriptions/genres/art; comprehensive retro coverage | 2026-03-26 | mobygames, api, metadata, artwork |
| https://api.thegamesdb.net/ | TheGamesDB Swagger; GPLv3; free API key; 3000 req/month/IP; titles/genres/players/publishers/boxart; good retro coverage | 2026-03-26 | thegamesdb, api, metadata, artwork |
| https://api-docs.igdb.com/ | IGDB REST API v4; free Twitch OAuth required; 4 req/sec; full game metadata, multiplayer modes (players), covers; Apicalypse query language | 2026-03-26 | igdb, api, metadata, artwork, twitch |
| https://www.screenscraper.fr/ | ScreenScraper CC-BY-NC-SA 4.0; free registration; 20k req/day; hash-based + filename; full metadata + artwork (covers/screenshots/wheel/marquee/video) | 2026-03-26 | screenscraper, api, metadata, artwork, hash |
| https://www.gametdb.com/ | GameTDB free XML database downloads + art packs; covers Wii/GC/WiiU/3DS/DS/Switch/PS3; game ID-based; no signup for downloads | 2026-03-26 | gametdb, xml, artwork, nintendo, free |
| https://art.gametdb.com | GameTDB artwork CDN; URL: art.gametdb.com/{platform}/{type}/{region}/{id}.{ext}; free, no auth; .png for Wii/GC, .jpg for Switch/PS3/3DS | 2026-03-26 | gametdb, artwork, cdn |
| https://www.gametdb.com/Wii/Downloads | Wii/GC download page — wiitdb.zip, cover packs by region, LANG parameter docs | 2026-03-26 | gametdb, wii, gamecube, download |
| https://www.gametdb.com/Main/FAQ | GameTDB FAQ — ID format, XML structure, license (free for software, contact for websites), art guidelines | 2026-03-26 | gametdb, faq, license, xml |
| https://www.gametdb.com/Main/Legal | GameTDB Legal page — not affiliated with Nintendo/Sony; community-contributed; no explicit CC license | 2026-03-26 | gametdb, legal, license |
| thumbnails.libretro.com | libretro thumbnail server; URL pattern: thumbnails.libretro.com/{System}/Named_Boxarts/{GameName}.png | 2026-03-26 | libretro, thumbnails, cdn |
| https://github.com/Gemba/skyscraper | Gemba/skyscraper — active C++/Qt5 fork of archived muldjord/skyscraper; GPL-3.0; supports ScreenScraper/TheGamesDB/ArcadeDB/MobyGames/IGDB and more | 2026-03-26 | skyscraper, scraper, open-source, cpp |
| https://github.com/muldjord/skyscraper | muldjord/skyscraper — ARCHIVED June 2022; C++/Qt5 scraper; active fork at Gemba/skyscraper | 2026-03-26 | skyscraper, archived, scraper |
| http://adb.arcadeitalia.net/ | ArcadeDB — Italian MAME arcade database; free REST API; no auth; shortplay videos; MAME filename-based | 2026-03-26 | arcadedb, arcade, mame, api, free |
| https://www.wikidata.org/wiki/Wikidata:WikiProject_Video_games | Wikidata WikiProject Video games — CC0 licensed; SPARQL API; links 100+ game databases; publisher/developer/genre data; variable quality | 2026-03-26 | wikidata, sparql, metadata, cc0, free |
| https://www.wikidata.org/wiki/Wikidata:Data_access | Wikidata data access methods — SPARQL endpoint, REST API, bulk dumps; all CC0 | 2026-03-26 | wikidata, api, sparql, bulk |
| https://gamesdb.launchbox-app.com/ | LaunchBox Games Database — web UI only; no public API; no bulk download; not usable for Remus integration | 2026-03-26 | launchbox, metadata, no-api |
