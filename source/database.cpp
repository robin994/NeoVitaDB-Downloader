/*
 * This file is part of VitaDB Downloader
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
 
#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <vitasdk.h>
#include <vitaGL.h>
#include "catalog.h"
#include "database.h"
#include "dialogs.h"
#include "extractor.h"
#include "network.h"
#include "utils.h"

ThemeSelection *themes = nullptr;
AppSelection *apps = nullptr;
AppSelection *psp_apps = nullptr;
TrophySelection *trophies = nullptr;
std::vector<std::string> daemon_blacklist;
std::vector<std::string> favorites;
bool favorites_old_format = false;

static bool is_favorite_record(const std::string &s, bool is_psp, const char *id) {
	return s.size() == 5 && s[0] == (is_psp ? 'P' : 'V') && atoi(s.c_str() + 1) == atoi(id);
}

char *hardcoded_daemon_blacklist[] = {
	"ABCD12345",
	"DEDALOX64",
	"RETROVITA",
	"JULIUS001",
	"DVLX00001",
	"GMSV00001",
	"MAIM00001",
	"MLCL00003",
	"OPENTITUS",
	"REGEDIT01",
	"SVMP00001",
	"SWKK00001",
	"VID000016",
	"VITAPONG0",
	"VSCU00001",
	"XASH00001"
};

static SceUID clash_thd;

extern char boot_params[1024];
extern AppSelection *to_download;

const char *sort_modes_apps_str[11] = {
	"Recently Added",
	"Recently Updated",
	"Oldest",
	"Most Downloaded",
	"Least Downloaded",
	"Alphabetical (A-Z)",
	"Alphabetical (Z-A)",
	"Smallest",
	"Largest",
	"Highest Game Score",
	"Lowest Game Score"
};

const char *sort_modes_themes_str[2] = {
	"Alphabetical (A-Z)",
	"Alphabetical (Z-A)"
};

static const char *aux_main_files[6] = {
	"Media/sharedassets0.assets.resS", // Unity
	"games/game.win", // GameMaker Studio
	"index.lua", // LuaPlayer Plus Vita
    "main.lua", // LifeLua
	"game.apk", // YoYo Loader
	"game_data/game.pck" // Godot
};

static int clashThread(unsigned int args, void *arg) {
	AppSelection *app = apps;
	while (app) {
		AppSelection *chk = app->next;
		while (chk) {
			if (!strcmp(chk->titleid, app->titleid)) {
				app->next_clash = chk;
				chk->prev_clash = app;
				break;
			}
			chk = chk->next;
		}
		app = app->next;
	}
	//printf("clash thread ended\n");
	return sceKernelExitDeleteThread(0);
}

// Grabs every icon for a platform in one request/extract instead of one
// HTTP round-trip per missing icon - the difference between a handful of
// requests and thousands on a fresh install or catalog switch, where
// virtually every icon is "missing". indexing=true on extract_zip_file
// shards into <shard>/<file> the same way the per-icon path does and
// rewrites icons.db to match, so this is only meant for populating from
// (near-)empty, not for topping up a handful of new icons - the per-icon
// loop stays for that. Returns false (falls back to the per-icon loop)
// if the catalog doesn't publish this zip, or the download/extract fails.
static bool download_icons_bulk(bool is_psp) {
	char *zip_url = is_psp ? CATALOG_ICONS_PSP_ZIP : CATALOG_ICONS_VITA_ZIP;
	if (!zip_url[0])
		return false;
	download_file(zip_url, "Downloading icons");
	SceIoStat st;
	if (sceIoGetstat(TEMP_DOWNLOAD_NAME, &st) < 0 || st.st_size == 0) {
		sceIoRemove(TEMP_DOWNLOAD_NAME);
		return false;
	}
	// Both platforms' icons live under the same local "icons/" folder -
	// icons_psp/ is a server-side publish path (and the CATALOG_PSP_ICON_FMT
	// download URL) only, never a local one; main.cpp's texture loader and
	// the per-icon download loop both always read/write "icons/" regardless
	// of platform, so this has to match or bulk-downloaded PSP icons end up
	// somewhere nothing ever looks.
	char icons_dir[288];
	sprintf(icons_dir, "%sicons/", catalog_dir);
	bool ok = extract_zip_file(TEMP_DOWNLOAD_NAME, icons_dir, true, false);
	sceIoRemove(TEMP_DOWNLOAD_NAME);
	return ok;
}

static char *get_value_from_json(char *dst, char *src, char *val, char **new_ptr) {
	char label[32];
	sprintf(label, "\"%s\": \"", val);
	//printf("label: %s\n", label);
	char *ptr = strstr(src, label) + strlen(label);
	//printf("ptr is: %X\n", ptr);
	if ((uintptr_t)ptr == strlen(label))
		return nullptr;
	char *end2 = strstr(ptr, (!strcmp(val, "long_description") || !strcmp(val, "changelog")) ? "\"," : "\"");
	if (dst == nullptr) {
		if (end2 - ptr > 0) {
			dst = (char *)malloc(end2 - ptr + 1);
			*new_ptr = dst;
		} else {
			*new_ptr = nullptr;
			return end2 + 1;
		}
	}
	//printf("size: %d\n", end2 - ptr);
	memcpy(dst, ptr, end2 - ptr);
	dst[end2 - ptr] = 0;
	return end2 + 1;
}

static bool checksum_match(char *hash_fname, char *fname, AppSelection *node, uint8_t type) {
	char cur_hash[40], aux_fname[256];
	SceUID f = sceIoOpen(hash_fname, SCE_O_RDONLY, 0777);
	if (f >= 0) {
		sceIoRead(f, cur_hash, 32);
		cur_hash[32] = 0;
		sceIoClose(f);
		if (strncmp(cur_hash, type != AUXILIARY_FILE ? node->hash : node->aux_hash, 32))
			node->state = APP_OUTDATED;
		else
			node->state = APP_UPDATED;
		return true;
	} else {
		if (type != AUXILIARY_FILE)
			f = sceIoOpen(fname, SCE_O_RDONLY, 0777);
		else {
			for (int i = 0; i < sizeof(aux_main_files) / sizeof(*aux_main_files); i++) {
				sprintf(aux_fname, "ux0:app/%s/%s", node->titleid, aux_main_files[i]);
				//printf("attempting with %s\n", aux_fname);
				f = sceIoOpen(aux_fname, SCE_O_RDONLY, 0777);
				if (f >= 0)
					break;
			}
		}
		if (f >= 0) {
			calculate_md5(f, cur_hash);
			if (strncmp(cur_hash, type != AUXILIARY_FILE ? node->hash : node->aux_hash, 32))
				node->state = APP_OUTDATED;
			else
				node->state = APP_UPDATED;
			switch (type) {
			case VITA_EXECUTABLE:
				sprintf(aux_fname, "ux0:app/%s/hash.vdb", node->titleid);
				break;
			case PSP_EXECUTABLE:
				sprintf(aux_fname, "%spspemu/PSP/GAME/%s/hash.vdb", pspemu_dev, node->folder);
				break;
			case AUXILIARY_FILE:
				sprintf(aux_fname, "ux0:app/%s/aux_hash.vdb", node->titleid);
				break;
			default:
				printf("Fatal Error!!!!\n");
				break;
			}
			f = sceIoOpen(aux_fname, SCE_O_WRONLY | SCE_O_TRUNC | SCE_O_CREAT, 0777);
			sceIoWrite(f, cur_hash, 32);
			sceIoClose(f);
			return true;
		} else
			node->state = APP_UNTRACKED;
		return false;
	}
}

char *get_changelog(const char *file, char *id) {
	char *res = nullptr;
	SceUID f = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (f >= 0) {
		size_t len = sceIoLseek(f, 0, SCE_SEEK_END);
		sceIoLseek(f, 0, SCE_SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		sceIoRead(f, buffer, len);
		buffer[len] = 0;
		char *ptr = buffer;
		char *end, *end2;
		char cur_id[8];
		do {
			ptr = get_value_from_json(cur_id, ptr, "id", nullptr);
			if (!strncmp(cur_id, id, 3)) {
				ptr = get_value_from_json(res, ptr, "changelog", &res);
				if (res)
					res = unescape(res);
				break;
			}
		} while (ptr);
		sceIoClose(f);
		free(buffer);
	}
	return res;
}

bool populate_apps_database(const char *file, bool is_psp) {
	// Read icons database
	char icons_db_path[288];
	sprintf(icons_db_path, "%sicons.db", catalog_dir);
	SceUID f = sceIoOpen(icons_db_path, SCE_O_RDONLY, 0777);
	//printf("f is %x\n", f);
	int icons_db_read = sceIoRead(f, generic_mem_buffer, MEM_BUFFER_SIZE);
	size_t icons_db_size = icons_db_read < 0 ? 0 : (size_t)icons_db_read;
	//printf("icons_db_size is %x\n", icons_db_size);
	char *icons_db = (char *)vglMalloc(icons_db_size + 1);
	sceClibMemcpy(icons_db, generic_mem_buffer, icons_db_size);
	icons_db[icons_db_size] = 0;
	sceIoClose(f);
	
	uint32_t missing_icons_num = 0;
	AppSelection *missing_icons[2048];

	// Burning on screen the parsing text dialog
	for (int i = 0; i < 3; i++) {
		draw_text_dialog("Parsing apps list", true, !is_psp);
	}
	f = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (f >= 0) {
		size_t len = sceIoLseek(f, 0, SCE_SEEK_END);
		sceIoLseek(f, 0, SCE_SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		sceIoRead(f, buffer, len);
		sceIoClose(f);
		buffer[len] = 0;
		if (!strstr(buffer, "\"name\":")) {
			bool is_empty_catalog = strstr(buffer, "[]") != nullptr;
			free(buffer);
			vglFree(icons_db);
			return is_empty_catalog;
		}
		bool has_likes_field = strstr(buffer, "\"likes\":") != nullptr;
		bool has_score_field = strstr(buffer, "\"score\":") != nullptr;
		bool has_ai_assisted_field = strstr(buffer, "\"ai_assisted\":") != nullptr;
		char *ptr = buffer;
		char *end, *end2;
		std::vector<std::string> new_favorites;
		do {
			char name[128], version[64], fname[128], fname2[128], smalldata[4];
			ptr = get_value_from_json(name, ptr, "name", nullptr);
			//printf("parsing %s\n", name);
			if (!ptr)
				break;
			AppSelection *node = (AppSelection*)malloc(sizeof(AppSelection));
			node->search_filtered = false;
			node->filtered = false;
			node->desc = nullptr;
			node->requirements = nullptr;
			node->next_clash = nullptr;
			node->prev_clash = nullptr;
			node->score = 0.0f;
			ptr = get_value_from_json(node->icon, ptr, "icon", nullptr);
			if (!strstr(icons_db, node->icon) && missing_icons_num < 2048) {
				missing_icons[missing_icons_num++] = node;
				//printf("%s is missing [%s]\n", node->icon, name);
			}
			ptr = get_value_from_json(version, ptr, "version", nullptr);
			ptr = get_value_from_json(node->author, ptr, "author", nullptr);
			ptr = get_value_from_json(node->type, ptr, "type", nullptr);
			ptr = get_value_from_json(node->id, ptr, "id", nullptr);
			if (!strcmp(node->id, SELF_CATALOG_ID) && strlen(boot_params) == 0) { // NeoVitaDB Downloader, check if newer than running version
				char *catalog_ver = version;
				while (*catalog_ver && (*catalog_ver < '0' || *catalog_ver > '9')) catalog_ver++; // skip the tag prefix, e.g. "v2.9.0" -> "2.9.0", whatever its length
				char *catalog_dot = strchr(catalog_ver, '.');
				char *local_dot = strchr(VERSION, '.');
				if (catalog_dot && local_dot) {
					int catalog_major = atoi(catalog_ver), catalog_minor = atoi(catalog_dot + 1);
					int local_major = atoi(VERSION), local_minor = atoi(local_dot + 1);
					char *catalog_dot2 = strchr(catalog_dot + 1, '.');
					char *local_dot2 = strchr(local_dot + 1, '.');
					int catalog_patch = catalog_dot2 ? atoi(catalog_dot2 + 1) : 0;
					int local_patch = local_dot2 ? atoi(local_dot2 + 1) : 0;
					if (catalog_major > local_major ||
						(catalog_major == local_major && catalog_minor > local_minor) ||
						(catalog_major == local_major && catalog_minor == local_minor && catalog_patch > local_patch)) {
						update_detected = true;
						to_download = node;
					}
				}
			}
			ptr = get_value_from_json(node->date, ptr, "date", nullptr);
			ptr = get_value_from_json(node->titleid, ptr, "titleid", nullptr);
			ptr = get_value_from_json(node->screenshots, ptr, "screenshots", nullptr);
			ptr = get_value_from_json(node->desc, ptr, "long_description", &node->desc);
			node->desc = unescape(node->desc);
			ptr = get_value_from_json(node->downloads, ptr, "downloads", nullptr);
			ptr = get_value_from_json(node->source_page, ptr, "source", nullptr);
			ptr = get_value_from_json(node->release_page, ptr, "release_page", nullptr);
			ptr = get_value_from_json(node->trailer, ptr, "trailer", nullptr);
			ptr = get_value_from_json(node->size, ptr, "size", nullptr);
			ptr = get_value_from_json(node->data_size, ptr, "data_size", nullptr);
			ptr = get_value_from_json(node->hash, ptr, "hash", nullptr);
			//printf("db hash %s\n", node->hash);
			if (is_psp) {
				node->favorites = false;
				if (!favorites_old_format) {
					for (auto &s : favorites) {
						if (is_favorite_record(s, true, node->id)) {
							node->favorites = true;
							break;
						}
					}
				}
				int type_num;
				sscanf(node->type, "%d", &type_num);
				type_num -= 10;
				sprintf(node->type, "%d", type_num);
			} else {
				ptr = get_value_from_json(node->aux_hash, ptr, "hash2", nullptr);
				//printf("aux db hash %s\n", node->aux_hash);
				sprintf(fname, "ux0:app/%s/hash.vdb", node->titleid);
				sprintf(fname2, "ux0:app/%s/eboot.bin", node->titleid);
				
				node->blacklisted = APP_WHITELISTED;
				for (int i = 0; i < sizeof(hardcoded_daemon_blacklist) / sizeof(*hardcoded_daemon_blacklist); i++) {
					if (!strcmp(hardcoded_daemon_blacklist[i], node->titleid)) {
						node->blacklisted = APP_HARD_BLACKLISTED;
					}
				}
				if (node->blacklisted == APP_WHITELISTED) {
					for (auto &s : daemon_blacklist) {
						if (s == node->titleid) {
							node->blacklisted = APP_BLACKLISTED;
							break;
						}
					}
				}
				
				node->favorites = false;
				for (auto &s : favorites) {
					if (favorites_old_format) {
						if (s == node->titleid) {
							node->favorites = true;
							char padded_id[6];
							sprintf(padded_id, "V%04d", atoi(node->id));
							new_favorites.push_back(padded_id);
							break;
						}
					} else {
						if (is_favorite_record(s, false, node->id)) {
							node->favorites = true;
							break;
						}
					}
				}
			}
			if (!is_psp && checksum_match(fname, fname2, node, VITA_EXECUTABLE)) {
				if (strlen(node->aux_hash) > 0) {
					sprintf(fname, "ux0:app/%s/aux_hash.vdb", node->titleid);
					for (int i = 0; i < sizeof(aux_main_files) / sizeof(*aux_main_files); i++) {
						if (checksum_match(fname, NULL, node, AUXILIARY_FILE))
							break;
					}
				}
			}
			//printf("hash part done\n");
			ptr = get_value_from_json(node->requirements, ptr, "requirements", &node->requirements);
			if (node->requirements)
				node->requirements = unescape(node->requirements);
			ptr = get_value_from_json(smalldata, ptr, "trophies", nullptr);
			node->trophies = atoi(smalldata);
			ptr = get_value_from_json(smalldata, ptr, "ai", nullptr);
			bool is_vibecoded = atoi(smalldata) != 0;
			if (!is_psp && has_score_field) {
				ptr = get_value_from_json(smalldata, ptr, "score", nullptr);
				node->score = atof(smalldata);
			}
			ptr = get_value_from_json(node->data_link, ptr, "data", nullptr);
			ptr = get_value_from_json(node->url, ptr, "url", nullptr);
			ptr = get_value_from_json(smalldata, ptr, "trusted", nullptr);
			node->trusted = atoi(smalldata);
			ptr = get_value_from_json(node->folder, ptr, "folder", nullptr);
			ptr = get_value_from_json(smalldata, ptr, "direct", nullptr);
			node->direct = atoi(smalldata);
			ptr = get_value_from_json(node->added, ptr, "added", nullptr);
			if (has_likes_field) {
				ptr = get_value_from_json(node->likes, ptr, "likes", nullptr);
			} else {
				strcpy(node->likes, "0");
			}
			if (is_vibecoded) {
				node->ai = APP_VIBECODED;
			} else if (has_ai_assisted_field) {
				ptr = get_value_from_json(smalldata, ptr, "ai_assisted", nullptr);
				node->ai = atoi(smalldata) ? APP_AI_ASSISTED : APP_HUMAN_MADE;
			} else {
				node->ai = APP_HUMAN_MADE;
			}
			if (is_psp) {
				if (node->folder[0]) {
					sprintf(fname, "%spspemu/PSP/GAME/%s/hash.vdb", pspemu_dev, node->folder);
					sprintf(fname2, "%spspemu/PSP/GAME/%s/EBOOT.PBP", pspemu_dev, node->folder);
					checksum_match(fname, fname2, node, PSP_EXECUTABLE);
				} else {
					node->state = APP_UNTRACKED;
				}
			}
			sprintf(node->name, "%s %s", name, version);
			if (is_psp) {
				node->next = psp_apps;
				psp_apps = node;
			} else {
				if (node->state == APP_OUTDATED) {
					if (strlen(boot_params) > 0 && !strcmp(boot_params, node->id))
						to_download = node;
				}
				node->next = apps;
				apps = node;
			}
		} while (ptr);
		free(buffer);
		
		if (favorites_old_format) {
			char favorites_path[288];
			sprintf(favorites_path, "%sfavorites.txt", catalog_dir);
			char is_new = '.';
			SceUID fd = sceIoOpen(favorites_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
			sceIoWrite(fd, &is_new, 1);
			is_new = ';';
			int i = 1;
			int sz = new_favorites.size();
			for (auto &s : new_favorites) {
				sceIoWrite(fd, s.c_str(), s.size());
				if (i != sz) {
					sceIoWrite(fd, &is_new, 1);
				}
				i++;
			}
			sceIoClose(fd);
			favorites_old_format = false;
			favorites = new_favorites;
		}
		
		if (!is_psp) {
			// Populate TitleID clashes
			clash_thd = sceKernelCreateThread("Clasher Thread", &clashThread, 0x10000100, 0x100000, 0, 0, NULL);
			sceKernelStartThread(clash_thd, 0, NULL);
		}
		
		if (!update_detected && missing_icons_num > 0) {
			if (missing_icons_num < 20 || !download_icons_bulk(is_psp)) {
				FILE *icons_db_f = fopen(icons_db_path, "a");
				for (int i = 0; i < missing_icons_num; i++) {
					char download_link[512];
					sprintf(download_link, is_psp ? CATALOG_PSP_ICON_FMT : CATALOG_ICON_FMT, missing_icons[i]->icon);
					download_file(download_link, "Downloading missing icons", false, i + 1, missing_icons_num);
					sprintf(download_link, "%sicons/%c%c", catalog_dir, missing_icons[i]->icon[0], missing_icons[i]->icon[1]);
					sceIoMkdir(download_link, 0777);
					sprintf(download_link, "%sicons/%c%c/%s", catalog_dir, missing_icons[i]->icon[0], missing_icons[i]->icon[1], missing_icons[i]->icon);
					if (sceIoRename(TEMP_DOWNLOAD_NAME, download_link) >= 0) {
						fprintf(icons_db_f, "%s\n", download_link);
						fflush(icons_db_f);
					}
				}
				fclose(icons_db_f);
			}
		}
	}
	vglFree(icons_db);
	return f >= 0;
}

bool populate_apps_database_vitadb_legacy(const char *file, bool is_psp) {
	uint32_t missing_icons_num = 0;
	AppSelection *missing_icons[2048];

	for (int i = 0; i < 3; i++) {
		draw_text_dialog("Parsing apps list", true, !is_psp);
	}
	SceUID f = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (f >= 0) {
		size_t len = sceIoLseek(f, 0, SCE_SEEK_END);
		sceIoLseek(f, 0, SCE_SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		sceIoRead(f, buffer, len);
		sceIoClose(f);
		buffer[len] = 0;
		if (!strstr(buffer, "\"name\":")) {
			bool is_empty_catalog = strstr(buffer, "[]") != nullptr;
			free(buffer);
			return is_empty_catalog;
		}
		char *ptr = buffer;
		do {
			char name[128], version[64], fname[128], fname2[128], smalldata[4];
			ptr = get_value_from_json(name, ptr, "name", nullptr);
			if (!ptr)
				break;
			AppSelection *node = (AppSelection*)malloc(sizeof(AppSelection));
			node->search_filtered = false;
			node->desc = nullptr;
			node->requirements = nullptr;
			node->next_clash = nullptr;
			node->prev_clash = nullptr;
			node->score = 0.0f;
			node->trusted = false;
			node->direct = false;
			node->added[0] = 0; // no such concept on this source - see SORT_APPS_RECENTLY_ADDED
			strcpy(node->likes, "0");
			ptr = get_value_from_json(node->icon, ptr, "icon", nullptr);
			char icon_path[300];
			sprintf(icon_path, "%sicons/%c%c/%s", catalog_dir, node->icon[0], node->icon[1], node->icon);
			SceIoStat icon_st;
			if (sceIoGetstat(icon_path, &icon_st) < 0 && missing_icons_num < 2048)
				missing_icons[missing_icons_num++] = node;
			ptr = get_value_from_json(version, ptr, "version", nullptr);
			ptr = get_value_from_json(node->author, ptr, "author", nullptr);
			ptr = get_value_from_json(node->type, ptr, "type", nullptr);
			ptr = get_value_from_json(node->id, ptr, "id", nullptr);
			ptr = get_value_from_json(node->date, ptr, "date", nullptr);
			ptr = get_value_from_json(node->titleid, ptr, "titleid", nullptr);
			ptr = get_value_from_json(node->screenshots, ptr, "screenshots", nullptr);
			ptr = get_value_from_json(node->desc, ptr, "long_description", &node->desc);
			node->desc = unescape(node->desc);
			ptr = get_value_from_json(node->downloads, ptr, "downloads", nullptr);
			// "status" skipped, unused by the client.
			ptr = get_value_from_json(node->source_page, ptr, "source", nullptr);
			ptr = get_value_from_json(node->release_page, ptr, "release_page", nullptr);
			ptr = get_value_from_json(node->trailer, ptr, "trailer", nullptr);
			ptr = get_value_from_json(node->size, ptr, "size", nullptr);
			ptr = get_value_from_json(node->data_size, ptr, "data_size", nullptr);
			ptr = get_value_from_json(node->hash, ptr, "hash", nullptr);
			if (is_psp) {
				node->favorites = false;
				for (auto &s : favorites) {
					if (is_favorite_record(s, true, node->id)) {
						node->favorites = true;
						break;
					}
				}
				int type_num;
				sscanf(node->type, "%d", &type_num);
				type_num -= 10;
				sprintf(node->type, "%d", type_num);
				sprintf(node->folder, "%d", atoi(node->id));
				sprintf(fname, "%spspemu/PSP/GAME/%s/hash.vdb", pspemu_dev, node->folder);
				sprintf(fname2, "%spspemu/PSP/GAME/%s/EBOOT.PBP", pspemu_dev, node->folder);
			} else {
				node->folder[0] = 0;
				ptr = get_value_from_json(node->aux_hash, ptr, "hash2", nullptr);
				sprintf(fname, "ux0:app/%s/hash.vdb", node->titleid);
				sprintf(fname2, "ux0:app/%s/eboot.bin", node->titleid);

				node->blacklisted = APP_WHITELISTED;
				for (int i = 0; i < sizeof(hardcoded_daemon_blacklist) / sizeof(*hardcoded_daemon_blacklist); i++) {
					if (!strcmp(hardcoded_daemon_blacklist[i], node->titleid)) {
						node->blacklisted = APP_HARD_BLACKLISTED;
					}
				}
				if (node->blacklisted == APP_WHITELISTED) {
					for (auto &s : daemon_blacklist) {
						if (s == node->titleid) {
							node->blacklisted = APP_BLACKLISTED;
							break;
						}
					}
				}

				node->favorites = false;
				for (auto &s : favorites) {
					if (is_favorite_record(s, false, node->id)) {
						node->favorites = true;
						break;
					}
				}
			}
			checksum_match(fname, fname2, node, is_psp ? PSP_EXECUTABLE : VITA_EXECUTABLE);
			if (!is_psp && strlen(node->aux_hash) > 0) {
				sprintf(fname, "ux0:app/%s/aux_hash.vdb", node->titleid);
				for (int i = 0; i < sizeof(aux_main_files) / sizeof(*aux_main_files); i++) {
					if (checksum_match(fname, NULL, node, AUXILIARY_FILE))
						break;
				}
			}
			ptr = get_value_from_json(node->requirements, ptr, "requirements", &node->requirements);
			if (node->requirements)
				node->requirements = unescape(node->requirements);
			ptr = get_value_from_json(smalldata, ptr, "trophies", nullptr);
			node->trophies = atoi(smalldata);
			// "tags" skipped, unused by the client.
			ptr = get_value_from_json(smalldata, ptr, "ai", nullptr);
			// This source has no ai_assisted distinction, only ai.
			node->ai = atoi(smalldata) ? APP_VIBECODED : APP_HUMAN_MADE;
			// "score" skipped - no on-device equivalent for this source.
			ptr = get_value_from_json(node->url, ptr, "url", nullptr);
			ptr = get_value_from_json(node->data_link, ptr, "data", nullptr);
			sprintf(node->name, "%s %s", name, version);
			if (is_psp) {
				node->next = psp_apps;
				psp_apps = node;
			} else {
				if (node->state == APP_OUTDATED) {
					if (strlen(boot_params) > 0 && !strcmp(boot_params, node->id))
						to_download = node;
				}
				node->next = apps;
				apps = node;
			}
		} while (ptr);
		free(buffer);

		if (!is_psp) {
			clash_thd = sceKernelCreateThread("Clasher Thread", &clashThread, 0x10000100, 0x100000, 0, 0, NULL);
			sceKernelStartThread(clash_thd, 0, NULL);
		}

		if (!update_detected && missing_icons_num > 0) {
			for (int i = 0; i < missing_icons_num; i++) {
				char download_link[512];
				sprintf(download_link, is_psp ? CATALOG_PSP_ICON_FMT : CATALOG_ICON_FMT, missing_icons[i]->icon);
				download_file(download_link, "Downloading missing icons", false, i + 1, missing_icons_num);
				sprintf(download_link, "%sicons/%c%c", catalog_dir, missing_icons[i]->icon[0], missing_icons[i]->icon[1]);
				sceIoMkdir(download_link, 0777);
				sprintf(download_link, "%sicons/%c%c/%s", catalog_dir, missing_icons[i]->icon[0], missing_icons[i]->icon[1], missing_icons[i]->icon);
				sceIoRename(TEMP_DOWNLOAD_NAME, download_link);
			}
		}
	}
	return f >= 0;
}

void reset_apps_database(bool is_psp) {
	if (!is_psp)
		sceKernelWaitThreadEnd(clash_thd, NULL, NULL); // clashThread walks this same list on its own thread - must finish before we free it out from under it
	AppSelection *list = is_psp ? psp_apps : apps;
	while (list) {
		AppSelection *next = list->next; // next_clash/prev_clash point at other
		if (list->desc)                  // nodes already in this same list, so
			free(list->desc);            // walking via ->next only is safe -
		if (list->requirements)          // nothing here is a separate
			free(list->requirements);    // allocation freed more than once.
		free(list);
		list = next;
	}
	if (is_psp)
		psp_apps = nullptr;
	else
		apps = nullptr;
}

void populate_daemon_blacklist() {
	daemon_blacklist.clear();
	char blacklist_path[288];
	sprintf(blacklist_path, "%sdaemon_blacklist.txt", catalog_dir);
	SceUID fd = sceIoOpen(blacklist_path, SCE_O_RDONLY, 0777);
	if (fd >= 0) {
		uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
		sceIoLseek(fd, 0, SCE_SEEK_SET);
		char *buffer = (char *)malloc(len + 1);
		char *_buffer = buffer;
		sceIoRead(fd, buffer, len);
		buffer[len] = 0;
		sceIoClose(fd);
		for (int i = 0; i < len; i += 10) {
			buffer[9] = 0;
			daemon_blacklist.push_back(buffer);
			buffer += 10;
		}
		free(_buffer);
	}
}

void insert_daemon_blacklist(char *tid) {
	daemon_blacklist.push_back(tid);
	char blacklist_path[288];
	sprintf(blacklist_path, "%sdaemon_blacklist.txt", catalog_dir);
	SceUID fd = sceIoOpen(blacklist_path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
	uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (len > 0) {
		char buffer[12];
		sprintf(buffer, ";%s", tid);
		sceIoWrite(fd, buffer, 10);
	} else {
		sceIoWrite(fd, tid, 9);
	}
	sceIoClose(fd);
}

void remove_daemon_blacklist(char *tid) {
	if (daemon_blacklist.size() > 1) {
		char *buffer = (char *)malloc(daemon_blacklist.size() * 10 + 1);
		buffer[0] = 0;
		int idx = 0;
		int to_delete = 0;
		for (std::string& s : daemon_blacklist) {
			if (s == tid) {
				to_delete = idx;
			} else {
				strcat(buffer, s.c_str());
				strcat(buffer, ";");
			}
			idx++;
		}
		daemon_blacklist.erase(daemon_blacklist.begin() + to_delete);
		char blacklist_path[288];
		sprintf(blacklist_path, "%sdaemon_blacklist.txt", catalog_dir);
		SceUID fd = sceIoOpen(blacklist_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		sceIoWrite(fd, buffer, daemon_blacklist.size() * 10 - 1);
		sceIoClose(fd);
		free(buffer);
	} else {
		daemon_blacklist.clear();
		char blacklist_path[288];
		sprintf(blacklist_path, "%sdaemon_blacklist.txt", catalog_dir);
		sceIoRemove(blacklist_path);
	}
}

void populate_favorites() {
	favorites.clear();
	char favorites_path[288];
	sprintf(favorites_path, "%sfavorites.txt", catalog_dir);
	SceUID fd = sceIoOpen(favorites_path, SCE_O_RDONLY, 0777);
	if (fd >= 0) {
		char is_old;
		sceIoRead(fd, &is_old, 1);
		if (is_old == '.') {
			uint64_t total_len = sceIoLseek(fd, 0, SCE_SEEK_END);
			sceIoLseek(fd, 1, SCE_SEEK_SET);
			uint64_t len = total_len > 0 ? total_len - 1 : 0; // exclude the marker byte already consumed above
			char *buffer = (char *)malloc(len + 1);
			char *_buffer = buffer;
			sceIoRead(fd, buffer, len);
			buffer[len] = 0;
			sceIoClose(fd);
			for (int i = 0; i < len; i += 6) {
				buffer[5] = 0;
				favorites.push_back(buffer);
				buffer += 6;
			}
			free(_buffer);
		} else {
			// Old format: needs to convert TitleIDs to MariaDB IDs
			favorites_old_format = true;
			uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
			sceIoLseek(fd, 0, SCE_SEEK_SET);
			char *buffer = (char *)malloc(len + 1);
			char *_buffer = buffer;
			sceIoRead(fd, buffer, len);
			buffer[len] = 0;
			sceIoClose(fd);
			for (int i = 0; i < len; i += 10) {
				buffer[9] = 0;
				favorites.push_back(buffer);
				buffer += 10;
			}
			free(_buffer);
		}
	}
}

void insert_favorites(char *tid, bool is_psp) {
	char record[6];
	sprintf(record, "%c%04d", is_psp ? 'P' : 'V', atoi(tid));
	favorites.push_back(record);
	char favorites_path[288];
	sprintf(favorites_path, "%sfavorites.txt", catalog_dir);
	SceUID fd = sceIoOpen(favorites_path, SCE_O_WRONLY | SCE_O_CREAT, 0777);
	uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (len > 0) {
		char buffer[8];
		sprintf(buffer, ";%s", record);
		sceIoWrite(fd, buffer, 6);
	} else {
		sceIoWrite(fd, ".", 1);
		sceIoWrite(fd, record, 5);
	}
	sceIoClose(fd);
}

void remove_favorites(char *tid, bool is_psp) {
	if (favorites.size() > 1) {
		char record[6];
		sprintf(record, "%c%04d", is_psp ? 'P' : 'V', atoi(tid));
		char *buffer = (char *)malloc(favorites.size() * 6 + 1);
		buffer[0] = '.';
		buffer[1] = 0;
		int idx = 0;
		int to_delete = 0;
		for (std::string& s : favorites) {
			if (s == record) {
				to_delete = idx;
			} else {
				strcat(buffer, s.c_str());
				strcat(buffer, ";");
			}
			idx++;
		}
		favorites.erase(favorites.begin() + to_delete);
		char favorites_path[288];
		sprintf(favorites_path, "%sfavorites.txt", catalog_dir);
		SceUID fd = sceIoOpen(favorites_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
		sceIoWrite(fd, buffer, favorites.size() * 6);
		sceIoClose(fd);
		free(buffer);
	} else {
		favorites.clear();
		char favorites_path[288];
		sprintf(favorites_path, "%sfavorites.txt", catalog_dir);
		sceIoRemove(favorites_path);
	}
}

static inline void swap_apps(AppSelection *prev, AppSelection *cur, AppSelection *next) {
	if (prev)
		prev->next = next;
	cur->next = next->next;
	next->next = cur;
}

void sort_apps_list(AppSelection **start, int sort_idx) {
	// Ensuring clasher titleids check finished
	sceKernelWaitThreadEnd(clash_thd, NULL, NULL);

	if (start == NULL || *start == NULL)
		return;
	
	bool swapped; 
  
	do {
		AppSelection *lptr = NULL; 
		AppSelection *ptr1 = *start; 
		swapped = false; 
		
		int64_t d1, d2;
		char *dummy;
		while (ptr1->next) {
			bool last_swapped = false;
			AppSelection *old_next = ptr1->next;
			switch (sort_idx) {
			case SORT_APPS_RECENTLY_ADDED:
				if (strcasecmp(ptr1->added, ptr1->next->added) < 0) {
					swap_apps(lptr, ptr1, ptr1->next);
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_NEWEST:
				if (strcasecmp(ptr1->date, ptr1->next->date) < 0) {
					swap_apps(lptr, ptr1, ptr1->next);
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_OLDEST:
				if (strcasecmp(ptr1->date, ptr1->next->date) > 0) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_MOST_DOWNLOADED:
				d1 = strtoll(ptr1->downloads, &dummy, 10);
				d2 = strtoll(ptr1->next->downloads, &dummy, 10);
				if (d1 < d2) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_LEAST_DOWNLOADED:
				d1 = strtoll(ptr1->downloads, &dummy, 10);
				d2 = strtoll(ptr1->next->downloads, &dummy, 10);
				if (d1 > d2) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = 1;
					last_swapped = true;
				}
				break;
			case SORT_APPS_A_Z:
				if (strcasecmp(ptr1->name, ptr1->next->name) > 0) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_Z_A:
				if (strcasecmp(ptr1->name, ptr1->next->name) < 0) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_SMALLEST:
				d1 = strtoll(ptr1->size, &dummy, 10) + strtoll(ptr1->data_size, &dummy, 10);
				d2 = strtoll(ptr1->next->size, &dummy, 10) + strtoll(ptr1->next->data_size, &dummy, 10);
				if (d1 > d2) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_LARGEST:
				d1 = strtoll(ptr1->size, &dummy, 10) + strtoll(ptr1->data_size, &dummy, 10);
				d2 = strtoll(ptr1->next->size, &dummy, 10) + strtoll(ptr1->next->data_size, &dummy, 10);
				if (d1 < d2) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_HIGHEST_SCORE:
				if (ptr1->score < ptr1->next->score) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_APPS_LOWEST_SCORE:
				if (ptr1->score > ptr1->next->score) {
					swap_apps(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			default:
				break;
			}
			if (!last_swapped) {
				lptr = ptr1;
				ptr1 = ptr1->next; 
			} else {
				if (*start == ptr1)
					*start = old_next;
				lptr = old_next;
			}
		} 
	} while (swapped); 
}

void populate_themes_database(const char *file) {
	sceIoMkdir("ux0:data/NeoVitaDB/themes", 0777);
	// Burning on screen the parsing text dialog
	for (int i = 0; i < 3; i++) {
		draw_text_dialog("Parsing themes list", true, false);
	}
	SceUID f = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (f >= 0) {
		uint32_t missing_previews_num = 0;
		ThemeSelection *missing_previews[2048];
		
		size_t len = sceIoLseek(f, 0, SCE_SEEK_END);
		sceIoLseek(f, 0, SCE_SEEK_SET);
		char *buffer = (char*)malloc(len + 1);
		sceIoRead(f, buffer, len);
		buffer[len] = 0;
		char *ptr = buffer;
		char *end, *end2;
		do {
			char name[128], fname[256];
			SceIoStat st;
			ptr = get_value_from_json(name, ptr, "name", nullptr);
			//printf("parsing %s\n", name);
			if (!ptr)
				break;
			ThemeSelection *node = (ThemeSelection*)malloc(sizeof(ThemeSelection));
			sprintf(fname, "ux0:data/NeoVitaDB/previews/%s.png", name);
			if (sceIoGetstat(fname, &st) < 0)
				missing_previews[missing_previews_num++] = node;
			node->desc = nullptr;
			node->shuffle = false;
			node->search_filtered = false;
			strcpy(node->name, name);
			sprintf(fname, "ux0:data/NeoVitaDB/themes/%s/theme.ini", node->name);
			
			if (sceIoGetstat(fname, &st) >= 0)
				node->state = APP_UPDATED;
			else
				node->state = APP_UNTRACKED;
			ptr = get_value_from_json(node->author, ptr, "author", nullptr);
			//printf("%s\n", node->author);
			ptr = get_value_from_json(node->desc, ptr, "description", &node->desc);
			//printf("%s\n", node->desc);
			ptr = get_value_from_json(node->credits, ptr, "credits", nullptr);
			//printf("%s\n", node->credits);
			ptr = get_value_from_json(node->bg_type, ptr, "bg_type", nullptr);
			//printf("%s\n", node->bg_type);
			ptr = get_value_from_json(node->has_music, ptr, "has_music", nullptr);
			//printf("%s\n", node->has_music);
			ptr = get_value_from_json(node->has_font, ptr, "has_font", nullptr);
			//printf("%s\n", node->has_font);
			node->next = themes;
			themes = node;
		} while (ptr);
		sceIoClose(f);
		free(buffer);
		
		// Downloading missing previews
		for (int i = 0; i < missing_previews_num; i++) {
			char download_link[512];
			sprintf(download_link, "https://github.com/CatoTheYounger97/vitaDB_themes/raw/main/previews/%s.png", missing_previews[i]->name);
			download_file(download_link, "Downloading missing previews", false, i + 1, missing_previews_num);
			sprintf(download_link, "ux0:data/NeoVitaDB/previews/%s.png", missing_previews[i]->name);
			sceIoRename(TEMP_DOWNLOAD_NAME, download_link);
		}
	}
	//printf("finished parsing\n");
}



static inline void swap_themes(ThemeSelection *prev, ThemeSelection *cur, ThemeSelection *next) {
	if (prev)
		prev->next = next;
	cur->next = next->next;
	next->next = cur;
}

void sort_themes_list(ThemeSelection **start, int sort_idx) {
	// Checking for empty list
	if (start == NULL || *start == NULL)
		return;
	
	bool swapped; 
  
	do {
		ThemeSelection *ptr1 = *start; 
		ThemeSelection *lptr = NULL; 
		swapped = false; 

		int64_t d1, d2;
		while (ptr1->next) {
			bool last_swapped = false;
			ThemeSelection *old_next = ptr1->next;
			switch (sort_idx) {
			case SORT_THEMES_A_Z:
				if (strcasecmp(ptr1->name, ptr1->next->name) > 0) {
					swap_themes(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			case SORT_THEMES_Z_A:
				if (strcasecmp(ptr1->name, ptr1->next->name) < 0) {
					swap_themes(lptr, ptr1, ptr1->next); 
					swapped = true;
					last_swapped = true;
				}
				break;
			default:
				break;
			}
			if (!last_swapped) {
				lptr = ptr1;
				ptr1 = ptr1->next; 
			} else {
				if (*start == ptr1)
					*start = old_next;
				lptr = old_next;
			}
		} 
	} while (swapped); 
}
