/*
 * Copyright 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author:  Peter Jones <pjones@redhat.com>
 */

#ifndef DUMPET_H
#define DUMPET_H

#include <errno.h>

typedef char Sector[0x800];

typedef union {
	Sector Raw;
	struct {
		char BootRecordIndicator;
		char Iso9660[5];
		char Version;
		char BootSystemId[32];
		char Reserved0[32];
		uint32_t BootCatalogLBA;
	} __attribute__((packed));
} BootRecordVolumeDescriptor;

typedef union {
	char Raw[32];
} BootCatalogEntry;

typedef union {
	Sector Raw;
	BootCatalogEntry Entry[64];
} BootCatalog;

static inline off_t get_sector_offset(int sector_number)
{
	Sector sector;
	return sector_number * sizeof(sector);
}

static inline int read_sector(FILE *iso, int sector_number, Sector *sector)
{
	size_t n;
	fseek(iso, get_sector_offset(sector_number), SEEK_SET);
	n = fread(sector, sizeof(*sector), 1, iso);

	if (n != 1) {
		int errnum = errno;
		fprintf(stderr, "dumpet: Error reading iso: %m\n");
		errno = errnum;
		return -errno;
	}
	return 0;
}

#endif /* DUMPET_H */
/* vim:set shiftwidth=8 softtabstop=8: */
