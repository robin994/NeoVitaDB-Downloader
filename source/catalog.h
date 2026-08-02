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

/*
 * Every remote address the app uses. The original client talked to a PHP
 * backend that resolved ids into download URLs; that backend is gone, so the
 * catalog is now a set of static files generated from NeoVitaDB-Catalog and
 * served over GitHub Pages. Download URLs come from the catalog itself, in the
 * "url" field of each entry, rather than being derived from the id.
 */
#define CATALOG_BASE "https://robin994.github.io/NeoVitaDB-Catalog"

// Application lists.
#define CATALOG_VITA_LIST CATALOG_BASE "/vita.json"
#define CATALOG_PSP_LIST  CATALOG_BASE "/psp.json"

// Assets, addressed by the paths stored in each entry.
#define CATALOG_ICONS_DB  CATALOG_BASE "/icons.db"
#define CATALOG_ICON_FMT  CATALOG_BASE "/icons/%s"
#define CATALOG_SHOT_FMT  CATALOG_BASE "/%s"
#define CATALOG_VIDEO_FMT CATALOG_BASE "/videos/%s.mp4"

// Trophy definitions, one JSON per title id, plus the icons they reference.
#define CATALOG_TROPHIES_FMT      CATALOG_BASE "/trophies/%s.json"
#define CATALOG_TROPHY_ICON_FMT   CATALOG_BASE "/trophies/icons/%s"

/*
 * Bootstrap payloads. These are fetched before the catalog is parsed — during
 * the libshacccg.suprx extraction chain, and when an homebrew declares
 * kubridge.skprx as a requirement — so they cannot be looked up by id and are
 * mirrored in the catalog repository instead.
 */
#define CATALOG_SHARKFOOD_VPK     CATALOG_BASE "/bootstrap/sharkf00d.vpk"
#define CATALOG_KUBRIDGE_SKPRX    CATALOG_BASE "/bootstrap/kubridge.skprx"
#define CATALOG_KUBRIDGE_MD5      CATALOG_BASE "/bootstrap/kubridge.md5"

// PSM Runtime packages, needed to extract the runtime shader compiler.
#define RUNTIME_PKG_100 "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000.pkg"
#define RUNTIME_PKG_201 "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000-A0201-V0100-e4708b1c1c71116c29632c23df590f68edbfc341-PE.pkg"

/*
 * Catalog id of this application. The app recognises its own entry by this id
 * to offer self-updates, so it must match apps/0001-neovitadb-downloader.json
 * and never change.
 */
#define SELF_CATALOG_ID "1"

#endif
