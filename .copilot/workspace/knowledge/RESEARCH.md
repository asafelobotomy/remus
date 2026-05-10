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

## libretro-database metadat/ structure (2026-03-31)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://github.com/libretro/libretro-database/tree/master/metadat | Root of metadat/ — subdirs: developer, publisher, genre, maxusers, releaseyear, redump, no-intro, libretro-dats, etc. | 2026-03-31 | libretro, metadat, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/developer | developer/ metadata DAT files — cartridge systems + PS1, PS2, Dreamcast; no GameCube/Wii/Saturn/Mega-CD/PC-Engine-CD/3DO | 2026-03-31 | libretro, developer, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/publisher | publisher/ metadata DAT files — same scope as developer minus PS1/PS2/Dreamcast (only PSP for Sony, no Dreamcast) | 2026-03-31 | libretro, publisher, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/genre | genre/ metadata DAT files — similar scope to developer; no PS1/PS2/Dreamcast/disc systems | 2026-03-31 | libretro, genre, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/maxusers | maxusers/ — cartridge systems only; no Sony/disc-based systems at all | 2026-03-31 | libretro, maxusers, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/releaseyear | releaseyear/ — cartridge systems + PSP only; no PS1/PS2/Dreamcast/other disc systems | 2026-03-31 | libretro, releaseyear, dat |
| https://github.com/libretro/libretro-database/tree/master/metadat/redump | redump/ — disc-based game identity DAT files (CRC of .bin tracks); covers all 9 target systems | 2026-03-31 | libretro, redump, dat, disc-based |

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

## ROM Compression & Conversion Formats Research (2026-05-25)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://docs.mamedev.org/tools/chdman.html | chdman official docs — createcd/createdvd/createdvd/extractcd commands; codec options (cdlz, cdzl, cdfl, cdzs); Linux ✅ | 2026-05-25 | chdman, chd, compression, disc, tool |
| https://github.com/unknownbrackets/maxcso | maxcso — fast ISO→CSO/ZSO/DAX converter for PSP + PS2; supports cso1, cso2, zso, dax formats; Linux ✅ (needs liblz4-dev, libdeflate-dev, libuv1-dev) | 2026-05-25 | maxcso, cso, zso, psp, ps2, tool |
| https://github.com/Exzap/ZArchive | ZArchive — Cemu's Wii U archive tool; WUA format = lossless, combines game+update+DLC; Linux ✅ | 2026-05-25 | zarchive, wua, wii-u, cemu, tool |
| https://docs.libretro.com/library/beetle_psx_hw/ | Beetle PSX HW — PS1 core; confirmed extensions: .cue .toc .m3u .ccd .exe .pbp .chd | 2026-05-25 | ps1, beetle, chd, libretro |
| https://docs.libretro.com/library/lrps2/ | LRPS2 (libretro PCSX2) — PS2 core; extensions: .elf .iso .ciso .chd .cso .bin .mdf .nrg .dump .gz .img .m3u; x86_64 only | 2026-05-25 | ps2, lrps2, pcsx2, chd, cso, libretro |
| https://docs.libretro.com/library/beetle_saturn/ | Beetle Saturn — Saturn core; confirmed CHD support; extensions: .cue .toc .m3u .ccd .chd | 2026-05-25 | saturn, beetle, chd, libretro |
| https://docs.libretro.com/library/flycast/ | Flycast — Dreamcast core; confirmed CHD + GDI support; extensions: .cdi .gdi .chd .cue .bin .elf .zip .7z .lst .dat .m3u | 2026-05-25 | dreamcast, flycast, chd, libretro |
| https://docs.libretro.com/library/genesis_plus_gx/ | Genesis Plus GX — Sega CD core; CHD confirmed with chdman examples; ISO+MP3 NOT supported | 2026-05-25 | sega-cd, genesis, chd, libretro |
| https://docs.libretro.com/library/picodrive/ | PicoDrive — Sega CD/32X core; CHD confirmed; extensions include .chd; no chdman example in docs | 2026-05-25 | sega-cd, 32x, picodrive, chd, libretro |
| https://docs.libretro.com/library/beetle_pce_fast/ | Beetle PCE FAST — TurboGrafx-CD core; CHD confirmed; extensions: .pce .cue .ccd .iso .img .bin .chd | 2026-05-25 | turbografx, pce, chd, libretro |
| https://docs.libretro.com/library/opera/ | Opera — 3DO core; CHD confirmed; extensions: .iso .bin .chd .cue | 2026-05-25 | 3do, opera, chd, libretro |
| https://docs.libretro.com/library/ppsspp/ | PPSSPP RetroArch core — official extension list does NOT include .chd; only .elf .iso .cso .prx .pbp | 2026-05-25 | psp, ppsspp, cso, libretro |
| https://docs.libretro.com/library/dolphin/ | Dolphin libretro — GameCube/Wii; extensions: .elf .iso .gcm .dol .tgc .wbfs .ciso .gcz .wad .rvz; RVZ confirmed | 2026-05-25 | gamecube, wii, dolphin, rvz, gcz, libretro |
| https://docs.libretro.com/library/citra/ | Citra — 3DS core; extensions: .3ds .3dsx .elf .axf .cci .cxi .app; requires AES keys; no compressed format equivalent | 2026-05-25 | 3ds, citra, libretro |
| https://xemu.app/docs/disc-images/ | xemu Original Xbox — uses XISO format (.iso ext); redump ISOs must be repacked with xdvdfs; no compression available | 2026-05-25 | xbox, xemu, xiso, xdvdfs |
| https://github.com/cemu-project/Cemu | Cemu — Wii U emulator; WUA introduced v1.27.0b (Apr 2022); Linux support since v2.0 (Aug 2022); available on Flathub | 2026-05-25 | cemu, wii-u, wua, linux |
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

## Disc Magic Byte Detection & System Identification (2026-06-05)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://github.com/libretro/RetroArch/blob/master/tasks/task_database_cue.c | **PRIMARY** — complete `MAGIC_NUMBERS[]` table covering GC/Wii/Dreamcast/Saturn/Mega-CD/PS1/PS2/PSP/CDi; `detect_system()` algorithm; per-platform serial extractors | 2026-06-05 | retroarch, magic-bytes, disc, detection, system-identification |
| https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/DiscIO/Volume.cpp | Dolphin `TryCreateDisc()` — reads `GAMECUBE_DISC_MAGIC`/`WII_DISC_MAGIC` from disc header to select GC vs Wii disc parser | 2026-06-05 | dolphin, gamecube, wii, magic-bytes, detection |
| https://raw.githubusercontent.com/dolphin-emu/dolphin/master/Source/Core/DiscIO/DiscUtils.h | Dolphin `DiscUtils.h` — defines `GAMECUBE_DISC_MAGIC = 0xC2339F3D` at 0x1C; `WII_DISC_MAGIC = 0x5D1C9EA3` at 0x18 | 2026-06-05 | dolphin, gamecube, wii, constants, magic-bytes |
| https://github.com/hrydgard/ppsspp/blob/master/Core/Loaders.cpp | PPSSPP `Loaders.cpp` — multi-system ISO 9660 PVD detection; LBA 16 (offset 0x8000); systemId field: "PSP GAME"/"PS3"/"PLAYSTATION"; 800 MB PS1/PS2 heuristic | 2026-06-05 | ppsspp, psp, ps1, ps2, iso9660, pvd, detection |
| https://dreamcast.wiki/IP.BIN | Dreamcast IP.BIN header spec — field offsets: hardware ID 0x00, product number 0x40, release date 0x50, game title 0x80; "SEGA SEGAKATANA" disc magic | 2026-06-05 | dreamcast, ipbin, serial, header, detection |
| https://doc.qt.io/qt-6/qimagereader.html | Qt 6 `QImageReader` — static `imageFormat(QIODevice*)` probes magic bytes to detect PNG/JPEG/BMP/WebP independent of file extension; `setDecideFormatFromContent(true)` | 2026-06-05 | qt6, image, format-detection, artwork, qimagereader |
| https://github.com/zxdb/ZXDB | ZXDB ODbL 1.0: MySQL+SQLite dump of complete ZX Spectrum metadata (title/genre/developer/publisher/year/players); version 1.0.234 (active); Python→SQLite converter included | 2026-04-01 | zx-spectrum, offline, database, metadata |
| https://api.zxinfo.dk/v3/ | ZXInfo REST API v3 backed by ZXDB; free, no auth; search/lookup endpoints for ZX Spectrum titles by title, publisher, author | 2026-04-01 | zx-spectrum, api, metadata |
| https://nopaystation.com/tsv/PSV_GAMES.tsv | NoPayStation PS Vita TSV: Content ID serial + title + region; NO genre/developer/year; community contribution | 2026-04-01 | ps-vita, serial, identification |
| https://github.com/libretro/libretro-database/tree/master/metadat/developer | libretro-database developer/ sub-directory: confirmed Lynx, Jaguar, SuperGrafx, Dreamcast, PS1, PS2 DATs; NOT C64/Amiga/Xbox/Vita | 2026-04-01 | libretro, developer, metadata |
| https://github.com/libretro/libretro-database/tree/master/metadat/genre | libretro-database genre/ sub-directory: confirmed Lynx, Jaguar, SuperGrafx; no other gap-system DATs | 2026-04-01 | libretro, genre, metadata |
| https://github.com/libretro/libretro-database/tree/master/metadat/publisher | libretro-database publisher/ sub-directory: confirmed Lynx, Jaguar, SuperGrafx; no Dreamcast/PS1/PS2/Xbox | 2026-04-01 | libretro, publisher, metadata |
| https://github.com/libretro/libretro-database/tree/master/metadat/releaseyear | libretro-database releaseyear/ sub-directory: confirmed Lynx, Jaguar, SuperGrafx; no disc-system DATs | 2026-04-01 | libretro, releaseyear, metadata |
| https://github.com/libretro/libretro-database/tree/master/metadat/redump | libretro-database redump/ sub-directory: identity-only DATs for Xbox, Xbox360, Dreamcast, Saturn, Mega-CD, PCE-CD, 3DO, PS1, PS2; NOT descriptive metadata | 2026-04-01 | libretro, redump, identification |
| https://query.wikidata.org/sparql | Wikidata SPARQL endpoint; CC0 data; P400=platform, P136=genre, P178=developer, P123=publisher, P577=publication date; useful CC0 fallback for sparse platforms | 2026-04-01 | wikidata, cc0, metadata, sparql |

## Offline Metadata Sources for Compendium Enrichment (2026-05-09)

| URL | Summary | Date | Tags |
|-----|---------|------|------|
| https://github.com/OpenVGDB/OpenVGDB | OpenVGDB — SQLite ROM hash→metadata DB; ~65 MB uncompressed; MD5 join; covers NES/SNES/GBA/PS1/PS2/Xbox/GCN; last updated Nov 2021 | 2026-05-09 | openvgdb, sqlite, offline, metadata, hash |
| https://github.com/OpenVGDB/OpenVGDB/releases/download/v29.0/openvgdb.zip | OpenVGDB v29.0 direct download — ~8.7 MB zip; schema: ROMS(romMD5), RELEASES(releaseDescription, releaseDeveloper, releaseGenre, releaseDate) | 2026-05-09 | openvgdb, download, sqlite |
| https://www.tosecdev.org/downloads | TOSEC DAT downloads page — complete pack 4743 DATs (2025-03-13); strong Amiga/C64/Amstrad CPC coverage; ClrMAMePro format; CRC32+MD5+SHA1 | 2026-05-09 | tosec, dat, amiga, c64, amstrad, offline |
| https://www.tosecdev.org/downloads/category/59-2025-03-13 | TOSEC 2025-03-13 release notes — 3111 main + 283 ISO + 1132 PIX DATs; PS2 WIP ISO; new Amstrad CPC CDT/DSK DATs | 2026-05-09 | tosec, release, amiga, c64, amstrad |
| https://datomatic.no-intro.org/index.php?page=download | No-Intro DAT-o-MATIC download page — Standard DAT/DB XML/daily bundle; free account required; covers Xbox 360 (17k entries), DS, C64, Amiga | 2026-05-09 | no-intro, dat, download, xbox360, ds |
| https://api-docs.retroachievements.org/v1/get-game-list.html | RetroAchievements API: GetGameList with h=1 returns MD5 hashes per console; custom RA hashing algorithm (not standard MD5 of whole file) | 2026-05-09 | retroachievements, api, hash, md5 |
| https://www.mobygames.com/info/api/ | MobyGames REST API — 720 req/hr non-commercial; platforms: DOS(2), Amiga(19), C64(4), Amstrad CPC(60), Xbox 360(69), PS2(7); best legacy PC metadata | 2026-05-09 | mobygames, api, dos, amiga, c64, amstrad, xbox360, ps2 |
| https://shiraga.me/ | shiragame — SQLite compiling No-Intro+Redump+TOSEC; MIT license; ~87 MB zip; hash→gamename only; last release 2022-08-13 (stale) | 2026-05-09 | shiragame, sqlite, offline, hash |
| https://github.com/SnowflakePowered/shiragame | shiragame GitHub — download: shiragame.db.zip (~87 MB); Stone platform IDs; auto-compiled from cataloguing orgs | 2026-05-09 | shiragame, github, download |
| https://docs/research/offline-metadata-sources-2026-05-09.md | Full research report: 10 sources compared; priority ranking; implementation notes for TOSEC/OpenVGDB/libretro metadat | 2026-05-09 | research, metadata, compendium, offline |
