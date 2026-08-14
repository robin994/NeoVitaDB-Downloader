/*
 * This file is part of NeoVitaDB Downloader
 * Copyright 2026 robin994
 * Copyright 2025 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CATALOG_H
#define _CATALOG_H

#include <string>
#include <vector>

/*
 * The original client talked to a PHP backend that resolved ids into download
 * URLs; that backend is gone, so the catalog is now a set of static files
 * generated from a NeoVitaDB-Catalog-shaped repository and served over GitHub
 * Pages (or any static host). Download URLs come from the catalog itself, in
 * the "url" field of each entry, rather than being derived from the id.
 *
 * Which catalog to use is user-configurable (ux0:data/NeoVitaDB/catalog.cfg,
 * see init_catalog() in catalog.cpp), so most of what used to be compile-time
 * CATALOG_* macros built from a single fixed base are now runtime buffers
 * populated once at boot. Every existing call site that used to do
 * sprintf(dst, CATALOG_ICON_FMT, icon) keeps working unchanged: these are
 * still printf-style format strings, just variables instead of macros now.
 */

#define OFFICIAL_CATALOG_BASE "https://robin994.github.io/NeoVitaDB-Catalog"
#define OFFICIAL_CATALOG_NAME "NeoVitaDB (Official)"

/*
 * The original VitaDB backend this project forked away from is back online,
 * serving its own native (non-NeoVitaDB-Catalog-shaped) PHP API. It's offered
 * as a second always-available catalog choice, alongside the official one and
 * any custom entries - but its JSON shape and download mechanics are enough of
 * a mismatch from our own that it's parsed by a dedicated function
 * (populate_apps_database_vitadb_legacy() in database.cpp) rather than the
 * regular FIELD_ORDER-sequential one. using_vitadb_legacy is what every other
 * VitaDB-legacy-specific branch in the codebase gates on.
 */
#define VITADB_LEGACY_BASE "https://www.rinnegatamante.eu/vitadb"
#define VITADB_LEGACY_NAME "VitaDB (Official)"
extern bool using_vitadb_legacy;

// Max length of a catalog base URL as read from catalog.cfg.
#define CATALOG_BASE_SIZE 192

extern char catalog_base[CATALOG_BASE_SIZE];
extern char catalog_dir[256];

struct CustomCatalog {
	std::string alias;
	std::string url;
};

extern std::vector<CustomCatalog> custom_catalogs;

// Application lists.
extern char CATALOG_VITA_LIST[CATALOG_BASE_SIZE + 16];
extern char CATALOG_PSP_LIST[CATALOG_BASE_SIZE + 16];

// Assets, addressed by the paths stored in each entry.
extern char CATALOG_ICONS_DB[CATALOG_BASE_SIZE + 16];
extern char CATALOG_ICONS_VITA_ZIP[CATALOG_BASE_SIZE + 24];
extern char CATALOG_ICONS_PSP_ZIP[CATALOG_BASE_SIZE + 24];
extern char CATALOG_ICON_FMT[CATALOG_BASE_SIZE + 16];
extern char CATALOG_PSP_ICON_FMT[CATALOG_BASE_SIZE + 24];
extern char CATALOG_SHOT_FMT[CATALOG_BASE_SIZE + 16];
extern char CATALOG_VIDEO_FMT[CATALOG_BASE_SIZE + 16];

// Trophy definitions, one JSON per title id, plus the icons they reference.
extern char CATALOG_TROPHIES_FMT[CATALOG_BASE_SIZE + 24];
extern char CATALOG_TROPHY_ICON_FMT[CATALOG_BASE_SIZE + 24];

/*
 * Bootstrap payloads. These are fetched before the catalog is parsed — during
 * the libshacccg.suprx extraction chain, and when an homebrew declares
 * kubridge.skprx as a requirement — so they cannot be looked up by id. They
 * are mirrored in the official catalog repository purely because it needed a
 * stable host; a user-selected third-party catalog is not expected to mirror
 * them, so these stay pinned to OFFICIAL_CATALOG_BASE regardless of
 * catalog.cfg.
 */
#define CATALOG_SHARKFOOD_VPK     OFFICIAL_CATALOG_BASE "/bootstrap/sharkf00d.vpk"
#define CATALOG_KUBRIDGE_SKPRX    OFFICIAL_CATALOG_BASE "/bootstrap/kubridge.skprx"
#define CATALOG_KUBRIDGE_MD5      OFFICIAL_CATALOG_BASE "/bootstrap/kubridge.md5"

// PSM Runtime packages, needed to extract the runtime shader compiler.
#define RUNTIME_PKG_100 "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000.pkg"
#define RUNTIME_PKG_201 "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000-A0201-V0100-e4708b1c1c71116c29632c23df590f68edbfc341-PE.pkg"

/*
 * Catalog id of this application. The app recognises its own entry by this id
 * to offer self-updates, so it must match apps/0001-neovitadb-downloader.json
 * and never change.
 */
#define SELF_CATALOG_ID "1"

void init_catalog();

#endif
