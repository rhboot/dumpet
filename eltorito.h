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
#ifndef ELTORITO_H
#define ELTORITO_H

#include "iso9660.h"

typedef union {
	Sector Raw;
	struct {
		uint8_t BootRecordIndicator;
		char Iso9660[5];
		uint8_t Version;
		char BootSystemId[32];
		uint8_t Reserved0[32];
		uint32_t BootCatalogLBA;
	} __attribute__((packed));
} BootRecordVolumeDescriptor;

typedef enum {
	ValidationIndicator = 0x01,
	SectionHeaderIndicator = 0x90,
	FinalSectionHeaderIndicator = 0x91,
	ExtensionIndicator = 0x44,
} SectionHeaderIndicatorType;

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

/* A typical boot catalog validation entry looks like this:
 * 00000000  01 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000010  00 00 00 00 00 00 00 00  00 00 00 00 aa 55 55 aa  |.............UU.|
 */
typedef struct {
	uint8_t HeaderIndicator;
	uint8_t PlatformId;
	uint8_t Reserved0[2];
	char Id[24];
	uint16_t Checksum;
	uint8_t FiveFive;
	uint8_t AA;
} __attribute__((packed)) BootCatalogValidationEntry;

/* A an Initial/Default Entry looks like this:
 * 00000020  88 00 00 00 00 00 1c 00  91 01 00 00 00 00 00 00  |................|
 * 00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef struct {
	uint8_t BootIndicator;
	uint8_t BootMediaType;
	uint16_t LoadSegment;
	uint8_t SystemType;
	uint8_t Reserved0;
	uint16_t SectorCount;
	uint32_t LoadLBA;
	char Reserved1[20];
} BootCatalogDefaultEntry;

/* An EFI Section Header Entry looks like this:
 * 00000040  91 ef 01 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 * 00000050  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
 */
typedef struct {
	uint8_t HeaderIndicator;
	uint8_t PlatformId;
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
typedef struct {
	uint8_t BootIndicator;
	uint8_t BootMediaType;
	uint16_t LoadSegment;
	uint8_t SystemType;
	uint8_t Reserved0;
	uint16_t SectorCount;
	uint32_t LoadLBA;
	uint8_t SelectionCriteriaType;
	uint8_t VendorUniqueSelectionCriteria[18];
} BootCatalogSectionEntry;

typedef union {
	uint8_t Raw[32];
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

#endif /* ELTORITO_H */
/* vim:set shiftwidth=8 softtabstop=8: */
