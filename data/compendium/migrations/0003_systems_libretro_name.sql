PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- Add the libretro thumbnail CDN system directory name.
-- This matches the directory names used by https://thumbnails.libretro.com/
-- and the filenames in the libretro-database DAT repository.
ALTER TABLE systems ADD COLUMN libretro_name TEXT NOT NULL DEFAULT '';

-- Populate for all known systems.
UPDATE systems SET libretro_name = 'Nintendo - Nintendo Entertainment System'    WHERE system_id = 1;
UPDATE systems SET libretro_name = 'Nintendo - Super Nintendo Entertainment System' WHERE system_id = 2;
UPDATE systems SET libretro_name = 'Nintendo - Nintendo 64'                       WHERE system_id = 3;
UPDATE systems SET libretro_name = 'Nintendo - GameCube'                           WHERE system_id = 4;
UPDATE systems SET libretro_name = 'Nintendo - Wii'                                WHERE system_id = 5;
UPDATE systems SET libretro_name = 'Nintendo - Game Boy'                           WHERE system_id = 6;
UPDATE systems SET libretro_name = 'Nintendo - Game Boy Color'                     WHERE system_id = 7;
UPDATE systems SET libretro_name = 'Nintendo - Game Boy Advance'                   WHERE system_id = 8;
UPDATE systems SET libretro_name = 'Nintendo - Nintendo DS'                        WHERE system_id = 9;
UPDATE systems SET libretro_name = 'Sega - Mega Drive - Genesis'                   WHERE system_id = 10;
UPDATE systems SET libretro_name = 'Sega - Master System - Mark III'               WHERE system_id = 11;
UPDATE systems SET libretro_name = 'Sega - Saturn'                                 WHERE system_id = 12;
UPDATE systems SET libretro_name = 'Sega - Dreamcast'                              WHERE system_id = 13;
UPDATE systems SET libretro_name = 'Sony - PlayStation'                            WHERE system_id = 14;
UPDATE systems SET libretro_name = 'Sony - PlayStation 2'                          WHERE system_id = 15;
UPDATE systems SET libretro_name = 'Sony - PlayStation Portable'                   WHERE system_id = 16;
UPDATE systems SET libretro_name = 'Atari - 2600'                                  WHERE system_id = 17;
UPDATE systems SET libretro_name = 'Atari - 7800'                                  WHERE system_id = 18;
UPDATE systems SET libretro_name = 'Atari - Lynx'                                  WHERE system_id = 19;
UPDATE systems SET libretro_name = 'NEC - PC Engine - TurboGrafx 16'              WHERE system_id = 20;
UPDATE systems SET libretro_name = 'NEC - PC Engine CD - TurboGrafx-CD'           WHERE system_id = 21;
UPDATE systems SET libretro_name = 'SNK - Neo Geo'                                 WHERE system_id = 22;
UPDATE systems SET libretro_name = 'Sega - Mega-CD - Sega CD'                     WHERE system_id = 23;
UPDATE systems SET libretro_name = 'Sega - Game Gear'                              WHERE system_id = 24;
UPDATE systems SET libretro_name = 'Sega - 32X'                                    WHERE system_id = 25;
UPDATE systems SET libretro_name = 'Atari - Jaguar'                                WHERE system_id = 26;
UPDATE systems SET libretro_name = 'SNK - Neo Geo Pocket Color'                   WHERE system_id = 27;
UPDATE systems SET libretro_name = 'Bandai - WonderSwan Color'                    WHERE system_id = 28;
UPDATE systems SET libretro_name = 'Nintendo - Virtual Boy'                        WHERE system_id = 29;
UPDATE systems SET libretro_name = 'Nintendo - Nintendo 3DS'                       WHERE system_id = 30;
UPDATE systems SET libretro_name = 'Nintendo - Switch'                             WHERE system_id = 31;
UPDATE systems SET libretro_name = 'Sony - PlayStation Vita'                       WHERE system_id = 32;
UPDATE systems SET libretro_name = 'Commodore - 64'                                WHERE system_id = 33;
UPDATE systems SET libretro_name = 'Commodore - Amiga'                             WHERE system_id = 34;
UPDATE systems SET libretro_name = 'Sinclair - ZX Spectrum +'                      WHERE system_id = 35;
UPDATE systems SET libretro_name = 'NEC - PC Engine SuperGrafx'                   WHERE system_id = 36;
UPDATE systems SET libretro_name = 'Microsoft - Xbox'                              WHERE system_id = 37;
UPDATE systems SET libretro_name = 'Microsoft - Xbox 360'                          WHERE system_id = 38;
UPDATE systems SET libretro_name = 'MAME'                                          WHERE system_id = 39;
UPDATE systems SET libretro_name = 'The 3DO Company - 3DO'                        WHERE system_id = 40;
UPDATE systems SET libretro_name = 'SNK - Neo Geo CD'                              WHERE system_id = 41;
UPDATE systems SET libretro_name = 'Nintendo - Family Computer Disk System'        WHERE system_id = 42;

COMMIT;
