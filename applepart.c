/*
 * Copyright 2011 Red Hat, Inc.
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

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "applepart.h"
#include "endian.h"

#define reterr(err) ({errno = err; return (((err) == 0) ? 0 : -1);})

static size_t new_adl_size(int nentries)
{
	AppleDiskLabel *adl;
	return sizeof(*adl) + nentries * sizeof(AppleDiskPartition);
}

#if 0
static size_t adl_size(AppleDiskLabel *adl)
{
	uint32_t entries;
	
	entries = be32_to_cpu(adl->Partitions[0].RawPartEntry.MapEntries);
	return sizeof(*adl) + entries * sizeof(AppleDiskPartition);
}
#endif

int adl_set_block_size(AppleDiskLabel *adl, uint16_t blocksize)
{
	if (blocksize % 512)
		reterr(EINVAL);
	adl->RawLabel.BlockSize = cpu_to_be16(blocksize);
	reterr(0);
}

uint16_t adl_get_block_size(AppleDiskLabel *adl)
{
	return be16_to_cpu(adl->RawLabel.BlockSize);
}

int adl_set_disk_blocks(AppleDiskLabel *adl, uint32_t nblocks)
{
	adl->RawLabel.BlockCount = cpu_to_be32(nblocks);
	reterr(0);
}

uint32_t adl_get_disk_blocks(AppleDiskLabel *adl)
{
	return be32_to_cpu(adl->RawLabel.BlockCount);
}

static int adl_priv_get_num_partitions(AppleDiskLabel *adl)
{
	return be32_to_cpu(adl->Partitions[0].RawPartEntry.MapEntries);
}

int adl_get_num_partitions(AppleDiskLabel *adl)
{
	return adl_priv_get_num_partitions(adl) - 1;
}

static int partnum_ok(AppleDiskLabel *adl, int partnum)
{
	int numparts = adl_priv_get_num_partitions(adl);
	return (partnum >= 0 && partnum < numparts);
}

static int pblock_in_use(AppleDiskLabel *adl, int partnum, uint32_t block)
{
	for (int p = 0; p < adl_priv_get_num_partitions(adl); p++) {
		MacPartitionEntry *pe = &adl->Partitions[p].RawPartEntry;
		uint32_t start, nblocks;

		if (p == partnum)
			continue;
		start = be32_to_cpu(pe->PBlockStart);
		nblocks = be32_to_cpu(pe->PBlocks);
		if (block >= start && block < (start + nblocks))
			return 1;
	}
	/* if it's past the end of the disk, it's in use for this validator */
	if (block >= be32_to_cpu(adl->RawLabel.BlockCount))
		return 1;
	return 0;
}

static int numblocks_ok(AppleDiskLabel *adl, int partnum, int numblocks)
{
	uint32_t start;

	start = be32_to_cpu(adl->Partitions[partnum].RawPartEntry.PBlockStart);
	return !pblock_in_use(adl, partnum, start + numblocks);
}

static int adl_priv_set_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					       uint32_t block)
{
	uint32_t old;
	uint32_t blocks;

	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	if (pblock_in_use(adl, partnum, block))
		reterr(EEXIST);
	
	old = adl->Partitions[partnum].RawPartEntry.PBlockStart;
	adl->Partitions[partnum].RawPartEntry.PBlockStart = cpu_to_be32(block);
	blocks = be32_to_cpu(adl->Partitions[partnum].RawPartEntry.PBlocks);
	if (!numblocks_ok(adl, partnum, blocks)) {
		adl->Partitions[partnum].RawPartEntry.PBlockStart = old;
		reterr(EINVAL);
	}
	reterr(0);
}

int adl_set_partition_pblock_start(AppleDiskLabel *adl, int partnum,
				   uint32_t block)
{
	return adl_priv_set_partition_pblock_start(adl, partnum + 1, block);
}

static int adl_priv_get_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					       uint32_t *block)
{
	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	*block = be32_to_cpu(adl->Partitions[partnum].RawPartEntry.PBlockStart);
	reterr(0);
}

int adl_get_partition_pblock_start(AppleDiskLabel *adl, int partnum,
				   uint32_t *block)
{
	return adl_priv_get_partition_pblock_start(adl, partnum + 1, block);
}

static int adl_priv_set_partition_blocks(AppleDiskLabel *adl, int partnum,
					 uint32_t blocks)
{
	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	if (!numblocks_ok(adl, partnum, blocks))
		reterr(EEXIST);
	adl->Partitions[partnum].RawPartEntry.PBlocks = cpu_to_be32(blocks);
	adl->Partitions[partnum].RawPartEntry.LBlocks = cpu_to_be32(blocks);
	reterr(0);
}

int adl_set_partition_blocks(AppleDiskLabel *adl, int partnum,
			     uint32_t blocks)
{
	return adl_priv_set_partition_blocks(adl, partnum + 1, blocks);
}

static int adl_priv_get_partition_blocks(AppleDiskLabel *adl, int partnum,
					 uint32_t *blocks)
{
	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	*blocks = be32_to_cpu(adl->Partitions[partnum].RawPartEntry.PBlocks);
	reterr(0);
}

int adl_get_partition_blocks(AppleDiskLabel *adl, int partnum,
			     uint32_t *blocks)
{
	return adl_priv_get_partition_blocks(adl, partnum + 1, blocks);
}

AppleDiskLabel *adl_new(void)
{
	AppleDiskLabel *adl = NULL;
	MacPartitionEntry *pe = NULL;
	uint16_t magic;

	adl = calloc(1, new_adl_size(1));
	if (!adl)
		return NULL;

	magic = MAC_LABEL_MAGIC;
	memmove(&adl->RawLabel.Signature, &magic, sizeof(magic));

	/* set up some handy defaults for the disk until we're told
	 * real values, so that our helper functions don't error on
	 * sanity checks */
	adl_set_block_size(adl, 512);
	adl_set_disk_blocks(adl,
			    (sizeof(MacDiskLabel) + sizeof(MacPartitionEntry)) /
			    adl_get_block_size(adl));

	adl->Partitions[0].Label = adl;
	pe = &adl->Partitions[0].RawPartEntry;
	magic = MAC_PARTITION_MAGIC;
	memmove(&pe->Signature, &magic, sizeof(magic));
	pe->MapEntries = cpu_to_be32(1);
	adl_priv_set_partition_pblock_start(adl, 0, 1);
	adl_priv_set_partition_blocks(adl, 0, sizeof(MacPartitionEntry) /
				      adl_get_block_size(adl));

	pe->Flags = cpu_to_be32(MAC_PARTITION_VALID|MAC_PARTITION_ALLOCATED);

	return adl;
}

AppleDiskLabel *adl_read(int fd)
{
	AppleDiskLabel *adl = adl_new();

	if (!adl)
		return NULL;

	return adl;
}

/* vim:set shiftwidth=8 softtabstop=8: */
