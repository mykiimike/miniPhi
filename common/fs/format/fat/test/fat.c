/* Host test of FAT12, FAT16 and FAT32. Not linked into the firmware. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define SUPPORT_COMMON_FS
#define SUPPORT_COMMON_FS_FORMAT_FAT
#define SUPPORT_COMMON_FS_PART

#include "../../../fs.h"
#include "../fat.h"
#include "../../../part/part.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

#include "../../../fs.c"
#include "../fat.c"
#include "../../../part/part.c"

#define SLOTS 16

typedef struct {
	unsigned long at;
	unsigned char data[512];
	int used;
} slot_t;

typedef struct {
	slot_t slot[SLOTS];
	mp_fs_block_t block;
} image_t;

typedef struct {
	image_t *image;
	mp_fs_block_t block;
} parted_t;

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static slot_t *slot_of(image_t *image, unsigned long sector, int create) {
	int i, free_slot = -1;

	for(i = 0; i < SLOTS; i++) {
		if(image->slot[i].used && image->slot[i].at == sector)
			return(&image->slot[i]);
		if(!image->slot[i].used && free_slot < 0)
			free_slot = i;
	}
	if(!create || free_slot < 0)
		return(NULL);
	image->slot[free_slot].used = 1;
	image->slot[free_slot].at = sector;
	memset(image->slot[free_slot].data, 0, 512);
	return(&image->slot[free_slot]);
}

static mp_ret_t image_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	image_t *image = block->user;
	unsigned long n;

	for(n = 0; n < count; n++) {
		slot_t *slot = slot_of(image, sector + n, 0);

		if(slot)
			memcpy(data + n * 512UL, slot->data, 512);
		else
			memset(data + n * 512UL, 0, 512);
	}
	return(TRUE);
}

static mp_ret_t image_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	image_t *image = block->user;
	unsigned long n;

	for(n = 0; n < count; n++) {
		slot_t *slot = slot_of(image, sector + n, 1);

		if(!slot)
			return(FALSE);
		memcpy(slot->data, data + n * 512UL, 512);
	}
	return(TRUE);
}

static void image_init(image_t *image, unsigned long sectors) {
	memset(image, 0, sizeof(*image));
	image->block.user = image;
	image->block.size = 512;
	image->block.sectors = sectors;
	image->block.read = image_read;
	image->block.write = image_write;
}

static void put16(unsigned char *p, unsigned short v) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, unsigned long v) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

static unsigned char *need(image_t *image, unsigned long sector) {
	slot_t *slot = slot_of(image, sector, 1);

	expect(slot != NULL, "image has room");
	return(slot ? slot->data : NULL);
}

static void boot(image_t *image, int kind, unsigned short reserved, unsigned char fats,
	unsigned short root_entries, unsigned short spf16, unsigned long spf32,
	unsigned long total, unsigned long root_cluster) {
	unsigned char *s = need(image, 0);

	if(!s)
		return;
	memset(s, 0, 512);
	s[0] = 0xEB;
	s[1] = 0x3C;
	s[2] = 0x90;
	memcpy(s + 3, "MINIPHI ", 8);
	put16(s + 11, 512);
	s[13] = 1;
	put16(s + 14, reserved);
	s[16] = fats;
	put16(s + 17, root_entries);
	if(total < 0x10000UL)
		put16(s + 19, (unsigned short)total);
	s[21] = 0xF8;
	put16(s + 22, spf16);
	if(total >= 0x10000UL)
		put32(s + 32, total);
	if(kind == 32) {
		put32(s + 36, spf32);
		put32(s + 44, root_cluster);
		memcpy(s + 82, "FAT32   ", 8);
	}
	else
		memcpy(s + 54, kind == 16 ? "FAT16   " : "FAT12   ", 8);
	s[510] = 0x55;
	s[511] = 0xAA;
}

static void fat12_put(unsigned char *fat, int cluster, int value) {
	int offset = cluster + cluster / 2;

	if(cluster & 1) {
		fat[offset] = (unsigned char)((fat[offset] & 0x0F) | ((value & 0x0F) << 4));
		fat[offset + 1] = (unsigned char)((value >> 4) & 0xFF);
	}
	else {
		fat[offset] = (unsigned char)(value & 0xFF);
		fat[offset + 1] = (unsigned char)((fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F));
	}
}

static void short_ent(unsigned char *e, const char *name11, unsigned char attr, unsigned long cluster, unsigned long size) {
	memset(e, 0, 32);
	memcpy(e, name11, 11);
	e[11] = attr;
	put16(e + 20, (unsigned short)(cluster >> 16));
	put16(e + 26, (unsigned short)cluster);
	put32(e + 28, size);
}

static void lfn_readme(unsigned char *e) {
	static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
	const char *text = "Read Me.txt";
	int i, n = 11;

	memset(e, 0xFF, 32);
	e[0] = 0x41;
	e[11] = 0x0F;
	e[13] = 0x6B;
	e[26] = 0;
	e[27] = 0;
	for(i = 0; i < 13; i++) {
		unsigned short u = 0xFFFF;

		if(i < n)
			u = (unsigned char)text[i];
		else if(i == n)
			u = 0;
		e[pos[i]] = (unsigned char)u;
		e[pos[i] + 1] = (unsigned char)(u >> 8);
	}
}

static void build_fat12(image_t *image) {
	unsigned char *fat, *root, *sub, *data;

	image_init(image, 11);
	boot(image, 12, 1, 1, 16, 1, 0, 11, 0);
	fat = need(image, 1);
	root = need(image, 2);
	data = need(image, 3);
	need(image, 4);
	sub = need(image, 5);
	need(image, 6);
	if(!fat || !root || !data || !sub)
		return;
	fat12_put(fat, 0, 0xFF8);
	fat12_put(fat, 1, 0xFFF);
	fat12_put(fat, 2, 3);
	fat12_put(fat, 3, 0xFFF);
	fat12_put(fat, 4, 0xFFF);
	fat12_put(fat, 5, 0xFFF);
	lfn_readme(root);
	short_ent(root + 32, "README~1TXT", 0x20, 2, 516);
	short_ent(root + 64, "SUB        ", 0x10, 4, 0);
	memset(data, 'a', 512);
	memset(need(image, 4), 'b', 4);
	short_ent(sub, ".          ", 0x10, 4, 0);
	short_ent(sub + 32, "..         ", 0x10, 0, 0);
	short_ent(sub + 64, "NOTE    TXT", 0x20, 5, 4);
	memcpy(need(image, 6), "note", 4);
}

static void build_fat16(image_t *image) {
	unsigned char *fat, *root, *data;

	image_init(image, 4151);
	boot(image, 16, 1, 2, 16, 32, 0, 4151, 0);
	fat = need(image, 1);
	root = need(image, 65);
	data = need(image, 66);
	if(!fat || !root || !data)
		return;
	put16(fat, 0xFFF8);
	put16(fat + 2, 0xFFFF);
	put16(fat + 4, 0xFFFF);
	short_ent(root, "B       TXT", 0x20, 2, 5);
	memcpy(data, "fat16", 5);
}

static void build_fat32(image_t *image) {
	unsigned char *fat, *root, *data;
	unsigned long total = 1UL + 1024UL + 65525UL;

	image_init(image, total);
	boot(image, 32, 1, 2, 0, 0, 512, total, 2);
	fat = need(image, 1);
	root = need(image, 1025);
	data = need(image, 1026);
	if(!fat || !root || !data)
		return;
	put32(fat, 0x0FFFFFF8UL);
	put32(fat + 4, 0x0FFFFFFFUL);
	put32(fat + 8, 0x0FFFFFFFUL);
	put32(fat + 12, 0x0FFFFFFFUL);
	short_ent(root, "C       TXT", 0x20, 3, 5);
	memcpy(data, "fat32", 5);
}

static mp_ret_t parted_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	parted_t *part = block->user;
	unsigned long n;

	for(n = 0; n < count; n++) {
		unsigned long at = sector + n;

		if(at == 0) {
			memset(data, 0, 512);
			data[446 + 4] = 0x01;
			data[446 + 8] = 64;
			data[446 + 12] = 11;
			data[510] = 0x55;
			data[511] = 0xAA;
		}
		else if(at < 64)
			memset(data + n * 512UL, 0, 512);
		else if(image_read(&part->image->block, at - 64, data + n * 512UL, 1) != TRUE)
			return(FALSE);
	}
	return(TRUE);
}

static mp_ret_t parted_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	parted_t *part = block->user;
	unsigned long n;

	for(n = 0; n < count; n++) {
		if(sector + n < 64)
			return(FALSE);
		if(image_write(&part->image->block, sector + n - 64, data + n * 512UL, 1) != TRUE)
			return(FALSE);
	}
	return(TRUE);
}

static int saw(mp_fs_fat_dir_t *dir, const char *name) {
	mp_fs_fat_ent_t ent;

	while(mp_fs_fat_readdir(dir, &ent) == TRUE) {
		if(strcmp(ent.name, name) == 0 || strcmp(ent.short_name, name) == 0)
			return(1);
	}
	return(0);
}

int main(void) {
	image_t fat12, fat16, fat32;
	parted_t part;
	mp_fs_fat_t vol12, vol16, vol32, mbr;
	mp_fs_fat_file_t file;
	mp_fs_fat_dir_t dir;
	unsigned char buf[520];
	int n;

	build_fat12(&fat12);
	build_fat16(&fat16);
	build_fat32(&fat32);

	expect(mp_fs_fat_mount(&vol12, &fat12.block) == TRUE, "mount fat12");
	expect(vol12.kind == 12 && vol12.clusters == 8, "detected fat12");
	expect(mp_fs_fat_mount(&vol32, &fat32.block) == TRUE, "mount fat32 beside it");
	expect(vol32.kind == 32 && vol32.clusters == 65525, "detected fat32");
	expect(mp_fs_fat_open(&vol12, &file, "Read Me.txt") == TRUE, "long name opens");
	n = mp_fs_fat_read(&file, buf, 516);
	expect(n == 516 && buf[0] == 'a' && buf[511] == 'a' && memcmp(buf + 512, "bbbb", 4) == 0, "chain crosses two clusters");
	expect(mp_fs_fat_read(&file, buf, 4) == 0, "end of file");
	expect(mp_fs_fat_seek(&file, 0) == TRUE, "seek to the start");
	expect(mp_fs_fat_write(&file, "XYZ", 3) == 3, "overwrite inside the file");
	expect(mp_fs_fat_seek(&file, 0) == TRUE, "back to the start");
	expect(mp_fs_fat_read(&file, buf, 4) == 4 && memcmp(buf, "XYZa", 4) == 0, "the overwrite is what is read");
	expect(mp_fs_fat_seek(&file, 514) == TRUE, "near the end");
	expect(mp_fs_fat_write(&file, "ZZZZZZZZZZ", 10) == 2, "a write stops at the recorded size");
	expect(mp_fs_fat_seek(&file, file.size) == TRUE, "position at the size");
	expect(mp_fs_fat_write(&file, "Z", 1) == 0, "no byte past the size");
	expect(mp_fs_fat_seek(&file, file.size + 1) == FALSE, "seek past the size");
	mp_fs_fat_close(&file);
	expect(mp_fs_fat_read(&file, buf, 1) == -1, "closed file");
	expect(mp_fs_fat_open(&vol12, &file, "readme~1.txt") == TRUE, "short name, any case");
	mp_fs_fat_close(&file);
	expect(mp_fs_fat_open(&vol12, &file, "SUB") == FALSE, "a directory is not a file");
	expect(mp_fs_fat_open(&vol12, &file, "missing.txt") == FALSE, "missing file");
	expect(mp_fs_fat_opendir(&vol12, &dir, "SUB") == TRUE, "open the directory");
	expect(saw(&dir, "NOTE.TXT") == 1, "the directory lists the note");
	expect(mp_fs_fat_open(&vol12, &file, "SUB/NOTE.TXT") == TRUE, "path through the directory");
	expect(mp_fs_fat_read(&file, buf, 4) == 4 && memcmp(buf, "note", 4) == 0, "note contents");
	mp_fs_fat_close(&file);
	expect(mp_fs_fat_open(&vol12, &file, "Read Me.txt") == TRUE, "fat12 still reads after fat32 was mounted");
	expect(mp_fs_fat_read(&file, buf, 3) == 3 && memcmp(buf, "XYZ", 3) == 0, "its cache stayed its own");
	mp_fs_fat_close(&file);

	expect(mp_fs_fat_open(&vol32, &file, "C.TXT") == TRUE, "fat32 file");
	expect(mp_fs_fat_read(&file, buf, 5) == 5 && memcmp(buf, "fat32", 5) == 0, "fat32 contents");
	mp_fs_fat_close(&file);

	expect(mp_fs_fat_mount(&vol16, &fat16.block) == TRUE, "mount fat16");
	expect(vol16.kind == 16 && vol16.clusters == 4085, "detected fat16");
	expect(mp_fs_fat_open(&vol16, &file, "B.TXT") == TRUE, "fat16 file");
	expect(mp_fs_fat_read(&file, buf, 5) == 5 && memcmp(buf, "fat16", 5) == 0, "fat16 contents");
	mp_fs_fat_close(&file);

	memset(&part, 0, sizeof(part));
	part.image = &fat12;
	part.block.user = &part;
	part.block.size = 512;
	part.block.sectors = 75;
	part.block.read = parted_read;
	part.block.write = parted_write;
	expect(mp_fs_fat_mount(&mbr, &part.block) == FALSE, "the partition table is not a volume");
	{
		mp_fs_part_t found[4];
		int parts = mp_fs_part_scan(&part.block, found, 4);

		expect(parts == 1 && found[0].type == 0x01 && found[0].first == 64 && found[0].count == 11, "the fat12 partition");
		expect(mp_fs_fat_mount(&mbr, &found[0].block) == TRUE, "mount the partition block");
	}
	expect(mbr.start == 0 && mbr.kind == 12, "the volume is the partition");
	expect(mp_fs_fat_open(&mbr, &file, "/SUB/NOTE.TXT") == TRUE, "absolute path on the partition");
	expect(mp_fs_fat_read(&file, buf, 4) == 4 && memcmp(buf, "note", 4) == 0, "partition contents");
	mp_fs_fat_close(&file);
	mp_fs_fat_unmount(&mbr);
	expect(mp_fs_fat_open(&mbr, &file, "SUB/NOTE.TXT") == FALSE, "unmounted volume");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
