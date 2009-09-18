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

typedef enum {
	x86 = 0,
	ppc = 1,
	m68kmac = 2,
	efi=0xef
} PlatformId;

typedef enum {
	NotBootable,
	Bootable = 0x88
} BootIndicator;

typedef enum {
	NoEmulation,
	OneTwoDiskette,
	OneFourFourDiskette,
	TwoEightEightDiskette,
	HardDisk
} BootMediaType;

typedef enum {
	Header = 0x90,
	FinalHeader = 0x91
} HeaderIndicator;

/* A typical boot catalog validation entry looks like this:
 * 00000000  01 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000010  00 00 00 00 00 00 00 00  00 00 00 00 aa 55 55 aa  |.............UU.|
 */
typedef struct {
	char HeaderId;
	char PlatformId;
	char Reserved0[2];
	char Id[24];
	uint16_t Checksum;
	char FiveFive;
	char AA;
} __attribute__((packed)) BootCatalogValidationEntry;

/* A an Initial/Default Entry looks like this:
 * 00000020  88 00 00 00 00 00 1c 00  91 01 00 00 00 00 00 00  |................|
 * 00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef struct {
	char BootIndicator;
	char BootMediaType;
	uint16_t LoadSegment;
	char SystemType;
	char Reserved0;
	uint16_t SectorCount;
	uint32_t LoadRBA;
} BootCatalogDefaultEntry;

/* An EFI Section Header Entry looks like this:
 * 00000040  91 ef 01 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef {
	char HeaderIndicator;
	char PlatformId;
	uint16_t SectionEntryCount;
	char Id[28];
} BootCatalogSectionHeaderEntry;

typedef enum {
	NoSelectionCriteria,
	LanguageAndVersionInformation
} SelectionCriteriaType;

/* An EFI Section Entry looks like this:
 * 00000060  88 00 00 00 00 00 04 00  b1 00 00 00 00 00 00 00  |................|
 * 00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef {
	char BootIndicator;
	char BootMediaType;
	uint16_t LoadSegment;
	char SystemType;
	char Reserved0;
	uint16_t SectorCount;
	uint32_t LoadRBA;
	char SelectionCriteriaType;
	char VendorUniqueSelectionCriteria[18];
} BootCatalogSectionEntry;

typedef union {
	char Raw[32];
	BootCatalogValidationEntry ValidationEntry;
	BootCatalogDefaultEntry DefaultEntry;
	BootCatalogSectionHeaderEntry SectionHeaderEntry;
	BootCatalogSectionEntry SectionEntry;
} BootCatalogEntry;

/* A(n a)typical boot catalog looks like this:
 * Validation Entry:
 * 00000000  01 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000010  00 00 00 00 00 00 00 00  00 00 00 00 aa 55 55 aa  |.............UU.|
 * Initial/Default Entry:
 * 00000020  88 00 00 00 00 00 1c 00  91 01 00 00 00 00 00 00  |................|
 * 00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * Section Header Entry:
 * 00000040  91 ef 01 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * Section Entry:
 * 00000060  88 00 00 00 00 00 04 00  b1 00 00 00 00 00 00 00  |................|
 * 00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef union {
	Sector Raw;
	BootCatalogEntry Catalog[64];
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
