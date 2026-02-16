#pragma once

/**
 * @file constants.h
 * @brief Central constants library for Remus
 * 
 * This header provides a single location for all application constants, enums,
 * and configuration values. Eliminates scattered hardcoded strings and enables
 * type-safe constant access throughout the application.
 *
 * Usage:
 *   #include "constants/constants.h"
 *   using namespace Remus::Constants;
 *   
 *   auto system = Systems::getSystem(Systems::ID_NES);
 *   QString provider = Providers::SCREENSCRAPER;
 *   auto info = Providers::getProviderInfo(provider);
 *
 * @note This is a header-only library. No compilation unit required.
 */

// Include all constant modules
#include "providers.h"
#include "systems.h"
#include "templates.h"
#include "settings.h"
#include "ui_theme.h"
#include "confidence.h"
#include "hash_algorithms.h"
#include "match_methods.h"
#include "api.h"
#include "database_schema.h"
#include "network.h"
#include "engines.h"
#include "errors.h"

namespace Remus {
namespace Constants {

/**
 * @brief Version of the constants library
 * 
 * Increment when adding new constants or modifying enum values.
 * Used for cache invalidation and migration detection.
 * 
 * Version history:
 *   - 1: Initial implementation (Phase 1)
 *   - 2: Added templates and settings constants (Phase 3)
 *   - 3: Added API, database schema, network, engines, and error constants
 */
inline constexpr int CONSTANTS_VERSION = 3;

/**
 * @brief Application organization name for QSettings
 */
inline constexpr const char* SETTINGS_ORGANIZATION = "Remus";

/**
 * @brief Application name for QSettings
 */
inline constexpr const char* SETTINGS_APPLICATION = "Remus";

/**
 * @brief Default database filename
 */
inline constexpr const char* DATABASE_FILENAME = "remus.db";

/**
 * @brief Application version string
 * 
 * Format: MAJOR.MINOR.PATCH
 * Updated per milestone completions
 */
inline constexpr const char* APP_VERSION = "0.9.0";  // M9 Complete

/**
 * @brief Milestone tracking
 */
inline constexpr const char* CURRENT_MILESTONE = "M9";

} // Constants
} // Remus
