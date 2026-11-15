/* Host test of MBR partition views. Not linked into the firmware. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define SUPPORT_COMMON_FS
#define SUPPORT_COMMON_FS_PART

#include "../../fs.h"
#include "../part.h"

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

#include "../../fs.c"
#include "../part.c"

#define DISK_SECTORS 64

typedef struct {
	unsigned char data[DISK_SECTORS][512];
	int writes;
	mp_fs_block_t block;
} disk_t;

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_ret_t disk_read(mp_fs_block_t *block, unsigned long sector, unsigned char *data, unsigned long count) {
	disk_t *disk = block->user;
	unsigned long n;

	if(sector >= DISK_SECTORS || count > DISK_SECTORS - sector)
		return(FALSE);
	for(n = 0; n < count; n++)
		memcpy(data + n * 512UL, disk->data[sector + n], 512);
	return(TRUE);
}

static mp_ret_t disk_write(mp_fs_block_t *block, unsigned long sector, const unsigned char *data, unsigned long count) {
	disk_t *disk = block->user;
	unsigned long n;

	if(sector >= DISK_SECTORS || count > DISK_SECTORS - sector)
		return(FALSE);
	for(n = 0; n < count; n++)
		memcpy(disk->data[sector + n], data + n * 512UL, 512);
	disk->writes++;
	return(TRUE);
}

static void disk_init(disk_t *disk, unsigned long sectors) {
	memset(disk, 0, sizeof(*disk));
	disk->block.user = disk;
	disk->block.size = 512;
	disk->block.sectors = sectors;
	disk->block.read = disk_read;
	disk->block.write = disk_write;
}

static void put32(unsigned char *p, unsigned long value) {
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)(value >> 16);
	p[3] = (unsigned char)(value >> 24);
}

static void entry(unsigned char *sector, int index, unsigned char type, unsigned long start, unsigned long count) {
	unsigned char *e = sector + 446 + index * 16;

	memset(e, 0, 16);
	e[4] = type;
	put32(e + 8, start);
	put32(e + 12, count);
}

static void sign(unsigned char *sector) {
	sector[510] = 0x55;
	sector[511] = 0xAA;
}

int main(void) {
	disk_t disk;
	mp_fs_part_t parts[4];
	unsigned char buf[1024];
	int n;

	disk_init(&disk, DISK_SECTORS);
	sign(disk.data[0]);
	entry(disk.data[0], 0, 0x0B, 8, 8);
	entry(disk.data[0], 1, 0x0C, 16, 4);
	entry(disk.data[0], 2, 0x00, 24, 4);
	entry(disk.data[0], 3, 0x06, 0, 4);
	memset(parts, 0, sizeof(parts));
	n = mp_fs_part_scan(&disk.block, parts, 4);
	expect(n == 2, "two usable primaries");
	expect(parts[0].type == 0x0B && parts[0].first == 8 && parts[0].count == 8, "first window");
	expect(parts[1].type == 0x0C && parts[1].first == 16 && parts[1].count == 4, "second window");
	expect(parts[0].block.user == &parts[0] && parts[0].block.size == 512 && parts[0].block.sectors == 8, "part face");
	expect(parts[2].type == 0, "empty and zero-start entries are not stored");

	memset(parts, 0, sizeof(parts));
	n = mp_fs_part_scan(&disk.block, parts, 1);
	expect(n == 2 && parts[0].first == 8 && parts[1].type == 0, "max stores the first and still reports both");

	disk.data[8][0] = 0x11;
	disk.data[9][0] = 0x22;
	disk.data[7][0] = 0xA5;
	expect(mp_fs_block_read(&parts[0].block, 0, buf, 1) == TRUE && buf[0] == 0x11, "read adds the offset");
	expect(mp_fs_block_read(&parts[0].block, 1, buf, 1) == TRUE && buf[0] == 0x22, "second sector of the window");
	expect(mp_fs_block_read(&parts[0].block, 8, buf, 1) == FALSE, "sector count is the end");
	expect(mp_fs_block_read(&parts[0].block, 7, buf, 2) == FALSE, "a range may not cross the end");
	buf[0] = 0x5A;
	expect(mp_fs_block_write(&parts[0].block, 0, buf, 1) == TRUE, "write inside the window");
	expect(disk.data[8][0] == 0x5A && disk.data[7][0] == 0xA5, "the write lands at the parent offset");
	expect(mp_fs_block_write(&parts[0].block, 8, buf, 1) == FALSE && disk.writes == 1, "a write past the end does not reach the parent");

	disk.data[0][510] = 0x00;
	expect(mp_fs_part_scan(&disk.block, parts, 4) == 0, "bad signature is no table");
	disk.data[0][510] = 0x55;

	memset(disk.data[0] + 446, 0, 64);
	sign(disk.data[0]);
	entry(disk.data[0], 0, 0x0C, 60, 10);
	entry(disk.data[0], 1, 0x0B, 2, 4);
	memset(parts, 0, sizeof(parts));
	n = mp_fs_part_scan(&disk.block, parts, 4);
	expect(n == 1 && parts[0].first == 2 && parts[0].count == 4, "an entry past the disk is skipped");

	entry(disk.data[0], 1, 0x0B, 0xFFFFFFF0UL, 0x20);
	memset(parts, 0, sizeof(parts));
	expect(mp_fs_part_scan(&disk.block, parts, 4) == 0, "an entry that does not fit is skipped");

	disk.block.sectors = 0;
	entry(disk.data[0], 0, 0x0B, 100, 50);
	memset(disk.data[0] + 446 + 16, 0, 48);
	memset(parts, 0, sizeof(parts));
	n = mp_fs_part_scan(&disk.block, parts, 4);
	expect(n == 1 && parts[0].first == 100 && parts[0].count == 50, "an unknown disk length is not a bound");
	disk.block.sectors = DISK_SECTORS;

	memset(disk.data[0] + 446, 0, 64);
	sign(disk.data[0]);
	entry(disk.data[0], 0, 0xEE, 1, 63);
	entry(disk.data[0], 1, 0x0C, 8, 8);
	memset(parts, 0x5A, sizeof(parts));
	n = mp_fs_part_scan(&disk.block, parts, 4);
	expect(n == MP_FS_PART_GPT && parts[0].type == 0x5A, "a protective MBR writes nothing");
	expect(mp_fs_part_gpt(&disk.block, parts, 4) == MP_FS_PART_GPT, "gpt hook is unsupported");

	expect(mp_fs_part_scan(NULL, parts, 4) == MP_FS_PART_ERROR, "no disk");
	disk.block.size = 256;
	expect(mp_fs_part_scan(&disk.block, parts, 4) == MP_FS_PART_ERROR, "sector size is 512");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
