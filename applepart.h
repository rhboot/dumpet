/*
 * Copyright 2010 Red Hat, Inc.
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
#ifndef APPLEPART_H
#define APPLEPART_H

#include "libapplepart.h"

/* This format is documented (slightly) at:
 * http://www.opensource.apple.com/source/IOStorageFamily/IOStorageFamily-116.1.20/IOApplePartitionScheme.h
 * All fields are big endian
 */

#define MAC_PARTITION_MAGIC 0x504d

typedef struct {
	uint16_t Signature;	/* MAC_PARTITION_MAGIC */
	uint16_t Reserved0;
	uint32_t MapEntries;	/* number of partition entries */
	uint32_t PBlockStart;	/* physical start block of this partition */
	uint32_t PBlocks;	/* number of blocks for this partition */
	char Name[32];		/* partition name */
	char Type[32];		/* type description */
	uint32_t LBlockStart;	/* logical start block of partition */
	uint32_t LBlocks;	/* number of data blocks */
	uint32_t Flags;		/* flags */
	uint32_t LBootBlock;	/* logical block start of boot code */
	uint32_t BootBytes;	/* size of boot code */
	uint32_t BootLoad0;	/* memory load address of boot code */
	uint32_t BootLoad1;	/* reserved */
	uint32_t BootGoto0;	/* jump address in memory of boot code */
	uint32_t BootGoto1;	/* reserved */
	uint32_t Checksum;	/* checksum of boot code */
	uint8_t ProcessorId[16];/* processor type */
	uint32_t Reserved1[32];
	uint32_t Reserved2[62];
} __attribute__((packed)) MacPartitionEntry;

typedef struct {
	uint32_t DdBlock;	/* block start in label->BlockSize blocks */
	uint16_t DdSize;	/* block count in 512b blocks */
	uint16_t DdType;	/* driver's system type */
} __attribute__((packed)) DriverDescriptorMapEntry;

#define MAC_LABEL_MAGIC 0x4552

typedef struct {
	uint16_t Signature;	/* MAC_LABEL_MAGIC */
	uint16_t BlockSize;	/* logical sector size */
	uint32_t BlockCount;
	uint16_t DevType;
	uint16_t DevId;
	uint32_t DriverData;
	uint16_t DriverCount;	/* number of driver descriptor entries */
	DriverDescriptorMapEntry DriverMap[8];
	uint8_t Reserved1[430];
} __attribute__((packed)) MacDiskLabel;

struct AppleDiskPartition {
	AppleDiskLabel *Label;
	MacPartitionEntry RawPartEntry; /* always stored in big endian */
};

struct AppleDiskLabel {
	off_t DiskLocation;
	int fd;
	MacDiskLabel RawLabel; /* always stored in big endian */
	AppleDiskPartition Partitions[];
};

#endif /* APPLEPART_H */
/* vim:set shiftwidth=8 softtabstop=8: */
