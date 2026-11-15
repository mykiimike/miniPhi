/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>

#ifdef SUPPORT_COMMON_FS_FORMAT_FAT

static unsigned short _mp_fs_fat_u16(const unsigned char *p) {
	return((unsigned short)(p[0] | (p[1] << 8)));
}

static unsigned long _mp_fs_fat_u32(const unsigned char *p) {
	return((unsigned long)p[0] | ((unsigned long)p[1] << 8) |
		((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24));
}

static mp_ret_t _mp_fs_fat_fail(mp_fs_fat_t *fat, char *msg) {
	mp_printk("%s", msg);
	memset(fat, 0, sizeof(*fat));
	return(FALSE);
}

static int _mp_fs_fat_bpb(const unsigned char *sector) {
	unsigned short bps;
	unsigned char spc;

	if(sector[510] != 0x55 || sector[511] != 0xAA)
		return(0);
	if(sector[0] != 0xEB && sector[0] != 0xE9)
		return(0);
	bps = _mp_fs_fat_u16(sector + 11);
	if(bps != 512)
		return(0);
	spc = sector[13];
	if(spc == 0 || (spc & (unsigned char)(spc - 1)) != 0 || spc > 128)
		return(0);
	if(sector[16] == 0)
		return(0);
	return(1);
}

static unsigned char *_mp_fs_fat_get(mp_fs_fat_t *fat, unsigned long rel) {
	if(fat->cache_ok && fat->cache_at == rel)
		return(fat->cache);
	if(mp_fs_block_read(fat->block, fat->start + rel, fat->cache, 1) != TRUE) {
		fat->cache_ok = 0;
		return(NULL);
	}
	fat->cache_at = rel;
	fat->cache_ok = 1;
	return(fat->cache);
}

static mp_ret_t _mp_fs_fat_put(mp_fs_fat_t *fat, unsigned long rel, const unsigned char *data) {
	if(mp_fs_block_write(fat->block, fat->start + rel, data, 1) != TRUE) {
		fat->cache_ok = 0;
		return(FALSE);
	}
	if(data != fat->cache)
		memcpy(fat->cache, data, 512);
	fat->cache_at = rel;
	fat->cache_ok = 1;
	return(TRUE);
}

static int _mp_fs_fat_byte(mp_fs_fat_t *fat, unsigned long offset, unsigned char *out) {
	unsigned char *sector;

	if(offset / 512UL >= fat->spf)
		return(0);
	sector = _mp_fs_fat_get(fat, fat->fat_sector + offset / 512UL);
	if(!sector)
		return(0);
	*out = sector[offset % 512UL];
	return(1);
}

static int _mp_fs_fat_link(mp_fs_fat_t *fat, unsigned long cluster, unsigned long *out) {
	unsigned char raw[4];
	unsigned long offset;
	int n, bytes;

	if(cluster < 2)
		return(0);
	if(fat->kind == 12) {
		offset = cluster + cluster / 2UL;
		bytes = 2;
	}
	else if(fat->kind == 16) {
		offset = cluster * 2UL;
		bytes = 2;
	}
	else {
		offset = cluster * 4UL;
		bytes = 4;
	}
	for(n = 0; n < bytes; n++) {
		if(!_mp_fs_fat_byte(fat, offset + (unsigned long)n, &raw[n]))
			return(0);
	}
	if(fat->kind == 12) {
		unsigned short value = (unsigned short)(raw[0] | (raw[1] << 8));

		if(cluster & 1UL)
			value = (unsigned short)(value >> 4);
		else
			value = (unsigned short)(value & 0x0FFF);
		*out = value;
	}
	else if(fat->kind == 16)
		*out = (unsigned long)(raw[0] | (raw[1] << 8));
	else
		*out = _mp_fs_fat_u32(raw) & 0x0FFFFFFFUL;
	return(1);
}

static int _mp_fs_fat_end(mp_fs_fat_t *fat, unsigned long value) {
	if(fat->kind == 12)
		return(value >= 0xFF8UL);
	if(fat->kind == 16)
		return(value >= 0xFFF8UL);
	return(value >= 0x0FFFFFF8UL);
}

static unsigned long _mp_fs_fat_next(mp_fs_fat_t *fat, unsigned long cluster) {
	unsigned long value;

	if(!_mp_fs_fat_link(fat, cluster, &value) || value < 2 || _mp_fs_fat_end(fat, value))
		return(0);
	return(value);
}

static mp_ret_t _mp_fs_fat_map(mp_fs_fat_t *fat, unsigned long start, unsigned long pos, unsigned long *sector) {
	unsigned long cluster = start;
	unsigned long skip = (pos / 512UL) / fat->spc;
	unsigned long within = (pos / 512UL) % fat->spc;

	if(start < 2)
		return(FALSE);
	while(skip--) {
		cluster = _mp_fs_fat_next(fat, cluster);
		if(cluster < 2)
			return(FALSE);
	}
	*sector = fat->data_sector + (cluster - 2UL) * fat->spc + within;
	return(TRUE);
}

static void _mp_fs_fat_short(char *dst, const unsigned char *raw) {
	int i, n = 0;

	for(i = 0; i < 8; i++) {
		unsigned char c = raw[i];

		if(i == 0 && c == 0x05)
			c = 0xE5;
		if(c == ' ')
			break;
		dst[n++] = (char)c;
	}
	if(raw[8] != ' ') {
		dst[n++] = '.';
		for(i = 8; i < 11; i++) {
			if(raw[i] == ' ')
				break;
			dst[n++] = (char)raw[i];
		}
	}
	dst[n] = 0;
}

static unsigned char _mp_fs_fat_sum(const unsigned char *name) {
	unsigned char sum = 0;
	int i;

	for(i = 0; i < 11; i++)
		sum = (unsigned char)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + name[i]);
	return(sum);
}

static void _mp_fs_fat_utf8(char *dst, int cap, const unsigned short *src) {
	int n = 0;

	while(*src && *src != 0xFFFF) {
		unsigned int u = *src++;

		if(u < 0x80) {
			if(n + 1 >= cap)
				break;
			dst[n++] = (char)u;
		}
		else if(u < 0x800) {
			if(n + 2 >= cap)
				break;
			dst[n++] = (char)(0xC0 | (u >> 6));
			dst[n++] = (char)(0x80 | (u & 0x3F));
		}
		else {
			if(n + 3 >= cap)
				break;
			dst[n++] = (char)(0xE0 | (u >> 12));
			dst[n++] = (char)(0x80 | ((u >> 6) & 0x3F));
			dst[n++] = (char)(0x80 | (u & 0x3F));
		}
	}
	if(cap > 0)
		dst[n] = 0;
}

static int _mp_fs_fat_same(const char *a, const char *b) {
	while(*a && *b) {
		unsigned char ca = (unsigned char)*a++;
		unsigned char cb = (unsigned char)*b++;

		if(ca >= 'a' && ca <= 'z')
			ca = (unsigned char)(ca - 'a' + 'A');
		if(cb >= 'a' && cb <= 'z')
			cb = (unsigned char)(cb - 'a' + 'A');
		if(ca != cb)
			return(0);
	}
	return(*a == 0 && *b == 0);
}

static mp_ret_t _mp_fs_fat_at(mp_fs_fat_dir_t *dir, unsigned char *raw) {
	mp_fs_fat_t *fat = dir->fat;
	unsigned char *sector;
	unsigned long at, cluster, step, per;

	if(dir->cluster == 0 && fat->kind != 32) {
		if(dir->index >= fat->root_entries)
			return(FALSE);
		at = fat->root_sector + (dir->index * 32UL) / 512UL;
	}
	else {
		per = (unsigned long)fat->spc * 16UL;
		step = dir->index / per;
		cluster = dir->cluster;
		while(step--) {
			cluster = _mp_fs_fat_next(fat, cluster);
			if(cluster < 2)
				return(FALSE);
		}
		at = fat->data_sector + (cluster - 2UL) * fat->spc + ((dir->index % per) * 32UL) / 512UL;
	}
	sector = _mp_fs_fat_get(fat, at);
	if(!sector)
		return(FALSE);
	memcpy(raw, sector + (dir->index * 32UL) % 512UL, 32);
	return(TRUE);
}

static void _mp_fs_fat_lfn(unsigned short *lfn, const unsigned char *raw) {
	static const int pos[13] = { 1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30 };
	int seq = raw[0] & 0x1F;
	int slot, i;

	if(seq < 1 || seq > 20)
		return;
	slot = (seq - 1) * 13;
	for(i = 0; i < 13 && slot + i < 255; i++)
		lfn[slot + i] = (unsigned short)(raw[pos[i]] | (raw[pos[i] + 1] << 8));
}

mp_ret_t mp_fs_fat_readdir(mp_fs_fat_dir_t *dir, mp_fs_fat_ent_t *ent) {
	unsigned short lfn[256];
	int lfn_on = 0;
	unsigned char lfn_sum = 0;

	if(!dir || !dir->fat || !ent)
		return(FALSE);
	memset(lfn, 0, sizeof(lfn));
	while(!dir->done) {
		unsigned char raw[32];

		if(_mp_fs_fat_at(dir, raw) != TRUE)
			return(FALSE);
		if(raw[0] == 0x00) {
			dir->done = 1;
			return(FALSE);
		}
		dir->index++;
		if((unsigned char)raw[0] == 0xE5) {
			lfn_on = 0;
			continue;
		}
		if(raw[11] == 0x0F) {
			if(raw[0] & 0x40) {
				memset(lfn, 0, sizeof(lfn));
				lfn_on = 1;
				lfn_sum = raw[13];
			}
			if(!lfn_on || raw[13] != lfn_sum)
				lfn_on = 0;
			else
				_mp_fs_fat_lfn(lfn, raw);
			continue;
		}
		if((raw[11] & 0x08) && !(raw[11] & 0x10)) {
			lfn_on = 0;
			continue;
		}
		memset(ent, 0, sizeof(*ent));
		_mp_fs_fat_short(ent->short_name, raw);
		if(lfn_on && lfn_sum == _mp_fs_fat_sum(raw))
			_mp_fs_fat_utf8(ent->name, MP_FS_FAT_NAME, lfn);
		else
			memcpy(ent->name, ent->short_name, sizeof(ent->short_name));
		ent->attr = raw[11];
		ent->dir = (raw[11] & 0x10) ? 1 : 0;
		ent->size = _mp_fs_fat_u32(raw + 28);
		ent->cluster = _mp_fs_fat_u16(raw + 26);
		if(dir->fat->kind == 32)
			ent->cluster |= (unsigned long)_mp_fs_fat_u16(raw + 20) << 16;
		return(TRUE);
	}
	return(FALSE);
}

static mp_ret_t _mp_fs_fat_scan(mp_fs_fat_t *fat, unsigned long folder, const char *name, mp_fs_fat_ent_t *ent) {
	mp_fs_fat_dir_t dir;

	memset(&dir, 0, sizeof(dir));
	dir.fat = fat;
	dir.cluster = folder;
	while(mp_fs_fat_readdir(&dir, ent) == TRUE) {
		if(_mp_fs_fat_same(ent->name, name) || _mp_fs_fat_same(ent->short_name, name))
			return(TRUE);
	}
	return(FALSE);
}

static mp_ret_t _mp_fs_fat_find(mp_fs_fat_t *fat, const char *path, mp_fs_fat_ent_t *ent, int want_dir) {
	unsigned long folder = fat->kind == 32 ? fat->root_cluster : 0;
	const char *p = path;
	int walked = 0;
	int slash = 0;

	if(!p || !p[0] || (p[0] == '/' && p[1] == 0)) {
		if(!want_dir)
			return(FALSE);
		memset(ent, 0, sizeof(*ent));
		ent->dir = 1;
		ent->cluster = folder;
		return(TRUE);
	}
	while(*p == '/')
		p++;
	while(*p) {
		char comp[MP_FS_FAT_NAME];
		int n = 0;

		slash = 0;
		while(*p && *p != '/') {
			if(n < MP_FS_FAT_NAME - 1)
				comp[n++] = *p;
			p++;
		}
		comp[n] = 0;
		while(*p == '/') {
			slash = 1;
			p++;
		}
		if(!comp[0] || (comp[0] == '.' && comp[1] == 0))
			continue;
		if(_mp_fs_fat_scan(fat, folder, comp, ent) != TRUE)
			return(FALSE);
		walked = 1;
		if(*p) {
			if(!ent->dir || ent->cluster < 2)
				return(FALSE);
			folder = ent->cluster;
		}
	}
	if(!walked) {
		if(!want_dir)
			return(FALSE);
		memset(ent, 0, sizeof(*ent));
		ent->dir = 1;
		ent->cluster = fat->kind == 32 ? fat->root_cluster : 0;
		return(TRUE);
	}
	if(slash && !ent->dir)
		return(FALSE);
	if(want_dir && !ent->dir)
		return(FALSE);
	if(!want_dir && ent->dir)
		return(FALSE);
	return(TRUE);
}

mp_ret_t mp_fs_fat_mount(mp_fs_fat_t *fat, mp_fs_block_t *block) {
	unsigned char *boot;
	unsigned long total, spf, root_entries, root_sectors, used, data_sectors, clusters;
	unsigned long fats;

	if(!fat || !block || block->size != 512)
		return(FALSE);
	memset(fat, 0, sizeof(*fat));
	fat->block = block;
	fat->start = 0;
	if(mp_fs_block_read(block, 0, fat->cache, 1) != TRUE)
		return(_mp_fs_fat_fail(fat, "FAT: no boot sector"));
	if(!_mp_fs_fat_bpb(fat->cache))
		return(_mp_fs_fat_fail(fat, "FAT: not a FAT volume"));
	boot = fat->cache;
	fat->spc = boot[13];
	fat->fats = boot[16];
	fats = boot[16];
	root_entries = _mp_fs_fat_u16(boot + 17);
	spf = _mp_fs_fat_u16(boot + 22);
	if(spf == 0)
		spf = _mp_fs_fat_u32(boot + 36);
	total = _mp_fs_fat_u16(boot + 19);
	if(total == 0)
		total = _mp_fs_fat_u32(boot + 32);
	if(spf == 0 || fats == 0 || spf > 0xFFFFFFFFUL / fats)
		return(_mp_fs_fat_fail(fat, "FAT: bad allocation table"));
	root_sectors = (root_entries * 32UL + 511UL) / 512UL;
	used = (unsigned long)boot[14] | ((unsigned long)boot[15] << 8);
	used += fats * spf + root_sectors;
	if(used >= total)
		return(_mp_fs_fat_fail(fat, "FAT: bad allocation table"));
	if(block->sectors && total > block->sectors)
		return(_mp_fs_fat_fail(fat, "FAT: volume is past the end of the medium"));
	data_sectors = total - used;
	clusters = data_sectors / fat->spc;
	if(clusters < 4085UL)
		fat->kind = 12;
	else if(clusters < 65525UL)
		fat->kind = 16;
	else
		fat->kind = 32;
	fat->root_entries = (unsigned short)root_entries;
	fat->spf = spf;
	fat->fat_sector = (unsigned long)boot[14] | ((unsigned long)boot[15] << 8);
	fat->root_sector = fat->fat_sector + fats * spf;
	fat->data_sector = fat->root_sector + root_sectors;
	fat->clusters = clusters;
	fat->root_cluster = _mp_fs_fat_u32(boot + 44);
	if(fat->kind == 32 && fat->root_cluster < 2)
		return(_mp_fs_fat_fail(fat, "FAT: bad root"));
	fat->cache_at = 0;
	fat->cache_ok = 1;
	mp_printk("FAT%u, %lu clusters", (unsigned)fat->kind, fat->clusters);
	return(TRUE);
}

void mp_fs_fat_unmount(mp_fs_fat_t *fat) {
	if(!fat)
		return;
	memset(fat, 0, sizeof(*fat));
}

mp_ret_t mp_fs_fat_opendir(mp_fs_fat_t *fat, mp_fs_fat_dir_t *dir, const char *path) {
	mp_fs_fat_ent_t ent;

	if(!fat || !fat->block || !dir)
		return(FALSE);
	memset(dir, 0, sizeof(*dir));
	if(_mp_fs_fat_find(fat, path, &ent, 1) != TRUE)
		return(FALSE);
	if(path && path[0] && !(path[0] == '/' && path[1] == 0) && ent.cluster < 2 && fat->kind == 32)
		return(FALSE);
	dir->fat = fat;
	dir->cluster = ent.cluster;
	return(TRUE);
}

mp_ret_t mp_fs_fat_open(mp_fs_fat_t *fat, mp_fs_fat_file_t *file, const char *path) {
	mp_fs_fat_ent_t ent;

	if(!fat || !fat->block || !file)
		return(FALSE);
	memset(file, 0, sizeof(*file));
	if(_mp_fs_fat_find(fat, path, &ent, 0) != TRUE)
		return(FALSE);
	if(ent.size && ent.cluster < 2)
		return(FALSE);
	file->fat = fat;
	file->start = ent.cluster;
	file->size = ent.size;
	return(TRUE);
}

int mp_fs_fat_read(mp_fs_fat_file_t *file, void *data, int len) {
	unsigned char *out = data;
	int done = 0;

	if(!file || !file->fat || !file->fat->block || (len > 0 && !data) || len < 0)
		return(-1);
	while(done < len && file->pos < file->size) {
		unsigned char *sector;
		unsigned long at, offset, chunk, left;

		if(_mp_fs_fat_map(file->fat, file->start, file->pos, &at) != TRUE)
			return(done ? done : -1);
		sector = _mp_fs_fat_get(file->fat, at);
		if(!sector)
			return(done ? done : -1);
		offset = file->pos % 512UL;
		chunk = 512UL - offset;
		left = file->size - file->pos;
		if(chunk > (unsigned long)(len - done))
			chunk = (unsigned long)(len - done);
		if(chunk > left)
			chunk = left;
		memcpy(out + done, sector + offset, chunk);
		file->pos += chunk;
		done += (int)chunk;
	}
	return(done);
}

int mp_fs_fat_write(mp_fs_fat_file_t *file, const void *data, int len) {
	const unsigned char *in = data;
	int done = 0;

	if(!file || !file->fat || !file->fat->block || (len > 0 && !data) || len < 0)
		return(-1);
	if(file->pos >= file->size)
		return(0);
	if((unsigned long)len > file->size - file->pos)
		len = (int)(file->size - file->pos);
	while(done < len) {
		unsigned char *sector;
		unsigned long at, offset, chunk;

		if(_mp_fs_fat_map(file->fat, file->start, file->pos, &at) != TRUE)
			return(done ? done : -1);
		offset = file->pos % 512UL;
		chunk = 512UL - offset;
		if(chunk > (unsigned long)(len - done))
			chunk = (unsigned long)(len - done);
		if(offset == 0 && chunk == 512UL) {
			if(_mp_fs_fat_put(file->fat, at, in + done) != TRUE)
				return(done ? done : -1);
		}
		else {
			sector = _mp_fs_fat_get(file->fat, at);
			if(!sector)
				return(done ? done : -1);
			memcpy(sector + offset, in + done, chunk);
			if(_mp_fs_fat_put(file->fat, at, sector) != TRUE)
				return(done ? done : -1);
		}
		file->pos += chunk;
		done += (int)chunk;
	}
	return(done);
}

mp_ret_t mp_fs_fat_seek(mp_fs_fat_file_t *file, unsigned long pos) {
	if(!file || !file->fat || pos > file->size)
		return(FALSE);
	file->pos = pos;
	return(TRUE);
}

void mp_fs_fat_close(mp_fs_fat_file_t *file) {
	if(file)
		file->fat = NULL;
}

#endif
