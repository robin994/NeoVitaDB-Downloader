/*
 * This file is part of NeoVitaDB Downloader
 * Copyright 2026 robin994
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

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>
#include <vitaGL.h>
#include "catalog.h"
#include "utils.h"

char catalog_base[CATALOG_BASE_SIZE];
char catalog_dir[256];
std::vector<CustomCatalog> custom_catalogs;
bool using_vitadb_legacy;

char CATALOG_VITA_LIST[CATALOG_BASE_SIZE + 16];
char CATALOG_PSP_LIST[CATALOG_BASE_SIZE + 16];
char CATALOG_ICONS_DB[CATALOG_BASE_SIZE + 16];
char CATALOG_ICONS_VITA_ZIP[CATALOG_BASE_SIZE + 24];
char CATALOG_ICONS_PSP_ZIP[CATALOG_BASE_SIZE + 24];
char CATALOG_ICON_FMT[CATALOG_BASE_SIZE + 16];
char CATALOG_PSP_ICON_FMT[CATALOG_BASE_SIZE + 24];
char CATALOG_SHOT_FMT[CATALOG_BASE_SIZE + 16];
char CATALOG_VIDEO_FMT[CATALOG_BASE_SIZE + 16];
char CATALOG_TROPHIES_FMT[CATALOG_BASE_SIZE + 24];
char CATALOG_TROPHY_ICON_FMT[CATALOG_BASE_SIZE + 24];

#define CATALOG_CONFIG_FILE "ux0:data/NeoVitaDB/catalog.cfg"
#define LEGACY_DIR "ux0:data/NeoVitaDB"

static void catalog_slug(char *dst, const char *base) {
	if (!strncmp(base, "https://", 8))
		base += 8;
	else if (!strncmp(base, "http://", 7))
		base += 7;
	size_t i = 0;
	for (; base[i] && i < 48; i++)
		dst[i] = (base[i] == '/' || base[i] == ':') ? '_' : base[i];
	dst[i] = 0;
}

static void load_custom_catalogs() {
	custom_catalogs.clear();
	SceUID fd = sceIoOpen(LEGACY_DIR "/catalogs.cfg", SCE_O_RDONLY, 0777);
	if (fd < 0)
		return;
	uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
	sceIoLseek(fd, 0, SCE_SEEK_SET);
	char *buffer = (char *)malloc(len + 1);
	sceIoRead(fd, buffer, len);
	buffer[len] = 0;
	sceIoClose(fd);

	char *line = buffer;
	while (line < buffer + len) {
		char *end = strchr(line, '\n');
		if (end)
			*end = 0;
		int line_len = strlen(line);
		while (line_len > 0 && (line[line_len - 1] == '\r' || line[line_len - 1] == ' '))
			line[--line_len] = 0;
		if (line_len > 0) {
			CustomCatalog entry;
			char *sep = strchr(line, '|');
			if (sep) {
				*sep = 0;
				char *alias_end = sep - 1;
				while (alias_end >= line && *alias_end == ' ')
					*alias_end-- = 0;
				char *url_start = sep + 1;
				while (*url_start == ' ')
					url_start++;
				entry.alias = line;
				entry.url = url_start;
			} else {
				entry.url = line;
			}
			custom_catalogs.push_back(entry);
		}
		if (!end)
			break;
		line = end + 1;
	}
	free(buffer);
}

static void migrate_legacy_layout() {
	static const char *legacy_files[] = {
		"apps.json", "apps.stamp", "psp_apps.json", "psp_apps.stamp",
		"icons.db", "favorites.txt", "daemon_blacklist.txt"
	};
	char src[300], dst[300];
	for (size_t i = 0; i < sizeof(legacy_files) / sizeof(*legacy_files); i++) {
		sprintf(src, LEGACY_DIR "/%s", legacy_files[i]);
		sprintf(dst, "%s%s", catalog_dir, legacy_files[i]);
		move_path(src, dst);
	}
	sprintf(src, LEGACY_DIR "/icons");
	sprintf(dst, "%sicons", catalog_dir);
	move_path(src, dst);
	sprintf(src, LEGACY_DIR "/trophies");
	sprintf(dst, "%strophies", catalog_dir);
	move_path(src, dst);
	sprintf(src, "ux0:data/NeoVitaDB.json");
	sprintf(dst, "%sNeoVitaDB.json", catalog_dir);
	move_path(src, dst);
}

void init_catalog() {
	load_custom_catalogs();

	catalog_base[0] = 0;
	SceUID fd = sceIoOpen(CATALOG_CONFIG_FILE, SCE_O_RDONLY, 0777);
	if (fd >= 0) {
		int len = sceIoRead(fd, catalog_base, sizeof(catalog_base) - 1);
		sceIoClose(fd);
		if (len < 0)
			len = 0;
		catalog_base[len] = 0;
		char *newline = strchr(catalog_base, '\n');
		if (newline) {
			*newline = 0;
			len = newline - catalog_base;
		}
		while (len > 0 && (catalog_base[len - 1] == '\r' || catalog_base[len - 1] == ' '))
			catalog_base[--len] = 0;
	}
	if (catalog_base[0] == 0)
		strcpy(catalog_base, OFFICIAL_CATALOG_BASE);

	using_vitadb_legacy = !strcmp(catalog_base, VITADB_LEGACY_BASE);
	if (using_vitadb_legacy) {
		sprintf(CATALOG_VITA_LIST, "%s/list_hbs_json.php", catalog_base);
		sprintf(CATALOG_PSP_LIST, "%s/list_psp_hbs_json.php", catalog_base);
		CATALOG_ICONS_DB[0] = 0;
		CATALOG_ICONS_VITA_ZIP[0] = 0;
		CATALOG_ICONS_PSP_ZIP[0] = 0;
		sprintf(CATALOG_ICON_FMT, "%s/icons/%%s", catalog_base);
		sprintf(CATALOG_PSP_ICON_FMT, "%s/icons/%%s", catalog_base);
		sprintf(CATALOG_SHOT_FMT, "%s/%%s", catalog_base);
		CATALOG_VIDEO_FMT[0] = 0;
		sprintf(CATALOG_TROPHIES_FMT, "%s/get_trophies_for_app.php?id=%%s", catalog_base);
		CATALOG_TROPHY_ICON_FMT[0] = 0;
	} else {
		sprintf(CATALOG_VITA_LIST, "%s/vita.json", catalog_base);
		sprintf(CATALOG_PSP_LIST, "%s/psp.json", catalog_base);
		sprintf(CATALOG_ICONS_DB, "%s/icons.db", catalog_base);
		sprintf(CATALOG_ICONS_VITA_ZIP, "%s/icons_vita.zip", catalog_base);
		sprintf(CATALOG_ICONS_PSP_ZIP, "%s/icons_psp.zip", catalog_base);
		sprintf(CATALOG_ICON_FMT, "%s/icons/%%s", catalog_base);
		sprintf(CATALOG_PSP_ICON_FMT, "%s/icons_psp/%%s", catalog_base);
		sprintf(CATALOG_SHOT_FMT, "%s/%%s", catalog_base);
		sprintf(CATALOG_VIDEO_FMT, "%s/videos/%%s.mp4", catalog_base);
		sprintf(CATALOG_TROPHIES_FMT, "%s/trophies/%%s.json", catalog_base);
		sprintf(CATALOG_TROPHY_ICON_FMT, "%s/trophies/icons/%%s", catalog_base);
	}

	char slug[64];
	catalog_slug(slug, catalog_base);
	sceIoMkdir(LEGACY_DIR "/catalogs", 0777);
	sprintf(catalog_dir, LEGACY_DIR "/catalogs/%s/", slug);
	sceIoMkdir(catalog_dir, 0777);

	char check[300];
	sprintf(check, "%sapps.json", catalog_dir);
	SceIoStat st;
	bool already_populated = sceIoGetstat(check, &st) >= 0;
	bool is_official = !strcmp(catalog_base, OFFICIAL_CATALOG_BASE);
	if (!already_populated && is_official && sceIoGetstat(LEGACY_DIR "/apps.json", &st) >= 0)
		migrate_legacy_layout();

	char subdir[300];
	sprintf(subdir, "%sicons", catalog_dir);
	sceIoMkdir(subdir, 0777);
	sprintf(subdir, "%strophies", catalog_dir);
	sceIoMkdir(subdir, 0777);
}
