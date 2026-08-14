#include <vitasdk.h>
#include <taihen.h>
#include <string.h>
#include <libk/stdlib.h>
#include <libk/stdio.h>

// Must match source/catalog.h's OFFICIAL_CATALOG_BASE. This daemon is a
// separate nostdlib binary and can't link source/catalog.cpp, so it keeps
// its own copy of the config-read + slug logic below rather than sharing
// runtime state with the main app - see catalog_dir()/catalog_slug().
#define OFFICIAL_CATALOG_BASE "https://robin994.github.io/NeoVitaDB-Catalog"
#define CATALOG_CONFIG_FILE "ux0:data/NeoVitaDB/catalog.cfg"
#define LEGACY_DIR "ux0:data/NeoVitaDB"

#define BUF_SIZE (1152 * 1024)
#define NET_SIZE (141 * 1024)

const int sceKernelPreloadModuleInhibit  = SCE_KERNEL_PRELOAD_INHIBIT_LIBDBG;

int (*sceShellNoticeInit) (void *data);
int (*sceShellSetUtf8) (void *data, const char *text, SceSize len);
int (*sceShellNoticeClean) (void *data);
int (*sceLsdbSendNotification) (void *a1, int a2);

char *blacklist[] = {
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
	"YYOLOADER",
	"NZZMBSPTB",
	"XASH00001"
};

char *custom_blacklist = NULL;
int custom_blacklist_entries = 0;

int module_get_offset(SceUID modid, SceSize segidx, uint32_t offset, void *stub_out){
	int res = 0;
	SceKernelModuleInfo info;

	if (segidx > 3)
		return -1;

	if (stub_out == NULL)
		return -2;

	res = sceKernelGetModuleInfo(modid, &info);
	if(res < 0)
		return res;

	if(offset > info.segments[segidx].memsz)
		return -3;

	*(uint32_t *)stub_out = (uint32_t)(info.segments[segidx].vaddr + offset);

	return 0;
}

char *extractValue(char *dst, char *src, char *val) {
	char label[32];
	sprintf(label, "\"%s\": \"", val);
	char *ptr = sceClibStrstr(src, label) + strlen(label);
	if (ptr == strlen(label))
		return NULL;
	char *end2 = sceClibStrstr(ptr, "\"");
	memcpy(dst, ptr, end2 - ptr);
	dst[end2 - ptr] = 0;
	return end2 + 1;
}

uint8_t checksum_match(char *hash_fname, char *hash, uint8_t *updated) {
	char cur_hash[40], aux_fname[256];
	SceUID f2 = sceIoOpen(hash_fname, SCE_O_RDONLY, 0777);
	if (f2 > 0) {
		sceIoRead(f2, cur_hash, 32);
		cur_hash[32] = 0;
		sceIoClose(f2);
		if (strncmp(cur_hash, hash, 32))
			*updated = 0;
		else
			*updated = 1;
		return 1;
	}
	return 0;
}

// Derived from https://github.com/Princess-of-Sleeping/SceShell-Notice-PoC/blob/master/src/main.c
void send_update_notification(char *titleid, char *id) {
	char fname[128];
	sprintf(fname, "ux0:app/%s/sce_sys/icon0.png", titleid);
	
	char data[0x100];
	sceShellNoticeInit(data);
	sceShellSetUtf8(&data[0x0], titleid, sceClibStrnlen(titleid, 0x10));
	sceShellSetUtf8(&data[0xC], "LAUPDATE", sceClibStrnlen("LAUPDATE", 0x10));
	*(uint32_t *)(&data[0x28]) = 0x02; // Enable App open
	data[0x2C] = 0x01;
	sceShellSetUtf8(&data[0x30], fname, sceClibStrnlen(fname, 0xFFFF)); // Icon
	sceShellSetUtf8(&data[0xBC], "An update is available for this homebrew.", sceClibStrnlen("An update is available for this homebrew.", 0xFFFF));
	*(uint32_t *)(&data[0xC8]) = 0x20000;
	sceShellSetUtf8(&data[0xCC], "NEOVITADB", sceClibStrnlen("NEOVITADB", 0x10));
	sceShellSetUtf8(&data[0xD8], id, sceClibStrnlen(id, 0x10)); // Argument

	sceLsdbSendNotification(data, 1);
	sceShellNoticeClean(data);
}

void check_updates(const char *file) {
	SceUID f = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (f > 0) {
		uint64_t len = sceIoLseek(f, 0, SCE_SEEK_END);
		sceIoLseek(f, 0, SCE_SEEK_SET);
		char *buffer = (char*)taipool_alloc(len + 1);
		sceIoRead(f, buffer, len);
		sceIoClose(f);
		buffer[len] = 0;
		if (!strstr(buffer, "\"titleid\":")) {
			taipool_free(buffer);
			return;
		}
		char *ptr = buffer;
		char *end, *end2;
		do {
			char hash[34], aux_hash[34] = {}, fname[128], fname2[128], titleid[10], hb_id[6];
			ptr = extractValue(hb_id, ptr, "id");
			if (!ptr)
				break;
			uint8_t skip_entry = 0;
			ptr = extractValue(titleid, ptr, "titleid");
			for (int i = 0; i < sizeof(blacklist) / sizeof(*blacklist); i++) {
				if (!strncmp(blacklist[i], titleid, 9)) {
					skip_entry = 1;
					break;
				}
			}
			if (!skip_entry && custom_blacklist) {
				for (int i = 0; i < custom_blacklist_entries; i++) {
					if (!strncmp(&custom_blacklist[i * 10], titleid, 9)) {
						skip_entry = 1;
						break;
					}
				}
			}
			if (skip_entry)
				continue;
			ptr = extractValue(hash, ptr, "hash");
			ptr = extractValue(aux_hash, ptr, "hash2");
			sprintf(fname, "ux0:app/%s/hash.vdb", titleid);
			
			uint8_t is_updated = 0;
			if (checksum_match(fname, hash, &is_updated)) {
				if (strlen(aux_hash) > 0) {
					sprintf(fname, "ux0:app/%s/aux_hash.vdb", titleid);
					if (checksum_match(fname, aux_hash, &is_updated))
							break;
				}
				if (!is_updated) {
					send_update_notification(titleid, hb_id);
				}
			}
		} while (ptr);
		taipool_free(buffer);
	}
}

// Reads ux0:data/NeoVitaDB/catalog.cfg into base_out (falling back to
// OFFICIAL_CATALOG_BASE if missing/empty), independently from
// source/catalog.cpp's init_catalog() - see the comment at the top of this
// file for why this can't just call that instead.
void read_catalog_base(char *base_out, int base_size) {
	base_out[0] = 0;
	SceUID fd = sceIoOpen(CATALOG_CONFIG_FILE, SCE_O_RDONLY, 0777);
	if (fd > 0) {
		int len = sceIoRead(fd, base_out, base_size - 1);
		sceIoClose(fd);
		if (len < 0)
			len = 0;
		base_out[len] = 0;
		while (len > 0 && (base_out[len - 1] == '\n' || base_out[len - 1] == '\r' || base_out[len - 1] == ' '))
			base_out[--len] = 0;
	}
	if (base_out[0] == 0)
		sprintf(base_out, "%s", OFFICIAL_CATALOG_BASE);
}

// Must stay identical to source/catalog.cpp's catalog_slug(): both binaries
// need to resolve the same catalog base URL to the same per-catalog folder.
void catalog_slug(char *dst, const char *base) {
	if (!strncmp(base, "https://", 8))
		base += 8;
	else if (!strncmp(base, "http://", 7))
		base += 7;
	int i = 0;
	for (; base[i] && i < 48; i++)
		dst[i] = (base[i] == '/' || base[i] == ':') ? '_' : base[i];
	dst[i] = 0;
}

int daemon_thread(SceSize args, void *argp) {
	char catalog_base[192];
	read_catalog_base(catalog_base, sizeof(catalog_base));
	char slug[64];
	catalog_slug(slug, catalog_base);
	char catalog_dir[256];
	sprintf(catalog_dir, LEGACY_DIR "/catalogs/%s/", slug);

	char blacklist_path[288];
	sprintf(blacklist_path, "%sdaemon_blacklist.txt", catalog_dir);
	SceUID fd = sceIoOpen(blacklist_path, SCE_O_RDONLY, 0777);
	if (fd > 0) {
		uint64_t len = sceIoLseek(fd, 0, SCE_SEEK_END);
		sceIoLseek(fd, 0, SCE_SEEK_SET);
		custom_blacklist = (char *)taipool_alloc(len + 1);
		sceIoRead(fd, custom_blacklist, len);
		custom_blacklist[len] = 0;
		custom_blacklist_entries = (len + 1) / 10;
		sceIoClose(fd);
	}

	sceKernelDelayThread(8 * 1000 * 1000);
	
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	sceSysmoduleLoadModule(SCE_SYSMODULE_HTTPS);
	
	int ret = sceNetShowNetstat();
	SceNetInitParam initparam;
	if (ret == SCE_NET_ERROR_ENOTINIT) {
		initparam.memory = taipool_alloc(NET_SIZE);
		initparam.size = NET_SIZE;
		initparam.flags = 0;
		sceNetInit(&initparam);
	}
	
	sceNetCtlInit();
	sceHttpInit(0x100000);
	sceSslInit(0x100000);
	taiGetModuleExportFunc("SceLsdb", 0xFFFFFFFF, 0x315B9FD6, (uintptr_t *)&sceLsdbSendNotification);

	char vita_list_url[224];
	sprintf(vita_list_url, "%s/vita.json", catalog_base);
	char db_file_path[300];
	sprintf(db_file_path, "%sNeoVitaDB.json", catalog_dir);

	for (;;) {
		SceUID http_template = sceHttpCreateTemplate("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36", 2, 1);
		SceUID conn = sceHttpCreateConnectionWithURL(http_template, vita_list_url, 0);
		SceUID req = sceHttpCreateRequestWithURL(conn, 0, vita_list_url, 0);
		if (!sceHttpSendRequest(req, NULL, 0)) {
			fd = sceIoOpen(db_file_path, SCE_O_TRUNC | SCE_O_CREAT | SCE_O_WRONLY, 0777);
			void *buffer = taipool_alloc(BUF_SIZE);
			SceUID res;
			do {
				res = sceHttpReadData(req, buffer, BUF_SIZE);
				if(res > 0)
					res = sceIoWrite(fd, buffer, res);
			} while(res > 0);
			sceIoClose(fd);
			taipool_free(buffer);
			check_updates(db_file_path);
		}
		sceHttpDeleteRequest(req);
		sceHttpDeleteConnection(conn);
		sceHttpDeleteTemplate(http_template);
		sceKernelDelayThread(60 * 60 * 1000 * 1000);
	}

	return sceKernelExitDeleteThread(0);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	// Taken from https://github.com/Princess-of-Sleeping/SceShell-Notice-PoC/blob/master/src/main.c
	tai_module_info_t info;
	info.size = sizeof(info);

	if(taiGetModuleInfo("SceShell", &info) < 0)
		return SCE_KERNEL_START_FAILED;

	switch(info.module_nid){
	case 0x0552F692: // 3.60 Retail
		module_get_offset(info.modid, 0, 0x42930C | 1, &sceShellNoticeInit);
		module_get_offset(info.modid, 0, 0x408E14 | 1, &sceShellSetUtf8);
		module_get_offset(info.modid, 0, 0x4163E8 | 1, &sceShellNoticeClean);
		break;
	case 0x6CB01295: // 3.60 Devkit
		module_get_offset(info.modid, 0, 0x41AA30 | 1, &sceShellNoticeInit);
		module_get_offset(info.modid, 0, 0x3FAD88 | 1, &sceShellSetUtf8);
		module_get_offset(info.modid, 0, 0x408298 | 1, &sceShellNoticeClean);
		break;
	case 0x5549BF1F: // 3.65 Retail
		module_get_offset(info.modid, 0, 0x429754 | 1, &sceShellNoticeInit);
		module_get_offset(info.modid, 0, 0x40925C | 1, &sceShellSetUtf8);
		module_get_offset(info.modid, 0, 0x416830 | 1, &sceShellNoticeClean);
		break;
	default:
		return SCE_KERNEL_START_FAILED;
	}
	
	taipool_init(BUF_SIZE + NET_SIZE);
	
	SceUID thid = sceKernelCreateThread("daemon", daemon_thread, 0xA0, 0x40000, 0, 0, NULL);
	sceKernelStartThread(thid, 0, NULL);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	return SCE_KERNEL_STOP_SUCCESS;
}
