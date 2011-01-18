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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "applepart.h"
#include "endian.h"

#define reterr(err) ({				\
		errno = err;			\
		return (((err) == 0) ? 0 : -1);	\
	})

#define save_errno(unsafe_code)		({		\
		typeof(errno) _errno_tmp = errno;	\
		unsafe_code;				\
		errno = _errno_tmp;			\
	})

static int adl_priv_read_partition(AppleDiskLabel *adl, int part);
static size_t new_adl_size(int nentries);
static size_t adl_size(AppleDiskLabel *adl);
static inline off_t adl_priv_lseek(AppleDiskLabel *adl, off_t offset,
				   int whence);
static int adl_priv_get_num_partitions(AppleDiskLabel *adl);
static int partnum_ok(AppleDiskLabel *adl, int partnum);
static int pblock_in_use(AppleDiskLabel *adl, int partnum, uint32_t block);
static int numblocks_ok(AppleDiskLabel *adl, int partnum, int numblocks);
static int adl_priv_set_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					       uint32_t block);
static int adl_priv_get_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					       uint32_t *block);
static int adl_priv_set_partition_blocks(AppleDiskLabel *adl, int partnum,
					 uint32_t blocks);
static int adl_priv_get_partition_blocks(AppleDiskLabel *adl, int partnum,
					 uint32_t *blocks);
static int adl_priv_read_partition(AppleDiskLabel *adl, int part);

static size_t new_adl_size(int nentries)
{
	AppleDiskLabel *adl;
	return sizeof(*adl) + nentries * sizeof(AppleDiskPartition);
}

static size_t adl_size(AppleDiskLabel *adl)
{
	uint32_t entries;
	
	entries = be32_to_cpu(adl->Partitions[0].RawPartEntry.MapEntries);
	return sizeof(*adl) + entries * sizeof(AppleDiskPartition);
}

static inline off_t adl_priv_lseek(AppleDiskLabel *adl, off_t offset,int whence)
{
	if (whence == SEEK_SET) {
		off_t ret;
		ret = lseek(adl->fd, adl->DiskLocation + offset, whence);
		ret -= adl->DiskLocation;
		return ret;
	}
	off_t current = lseek(adl->fd, 0, SEEK_CUR);

	if (whence == SEEK_CUR) {
		if (current + offset < adl->DiskLocation) {
			errno = EINVAL;
			return -1;
		}
	} else if (whence == SEEK_END) {
		off_t end = lseek(adl->fd, 0, SEEK_END);
		if (end + offset < adl->DiskLocation) {
			lseek(adl->fd, current, SEEK_SET);
			errno = EINVAL;
			return -1;
		}
	} else {
		errno = EINVAL;
		return -1;
	}

	return lseek(adl->fd, offset, whence);
}

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


int adl_set_partition_flags(AppleDiskLabel *adl, int partnum,
			    uint32_t flags)
{
	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	adl->Partitions[partnum].RawPartEntry.Flags = cpu_to_be32(flags);
	return 0;
}

int adl_get_partition_flags(AppleDiskLabel *adl, int partnum,
			    uint32_t *flags)
{
	if (!partnum_ok(adl, partnum))
		reterr(EINVAL);
	*flags = be32_to_cpu(adl->Partitions[partnum].RawPartEntry.Flags);
	return 0;
}

AppleDiskLabel *adl_new(void)
{
	AppleDiskLabel *adl = NULL;
	MacPartitionEntry *pe = NULL;
	uint16_t magic;

	adl = calloc(1, new_adl_size(1));
	if (!adl)
		return NULL;

	magic = cpu_to_be16(MAC_LABEL_MAGIC);
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
	magic = cpu_to_be16(MAC_PARTITION_MAGIC);
	memmove(&pe->Signature, &magic, sizeof(magic));
	pe->MapEntries = cpu_to_be32(1);
	adl_priv_set_partition_pblock_start(adl, 0, 1);
	adl_priv_set_partition_blocks(adl, 0, sizeof(MacPartitionEntry) /
				      adl_get_block_size(adl));

	pe->Flags = cpu_to_be32(MAC_PARTITION_VALID|MAC_PARTITION_ALLOCATED);

	return adl;
}

static int adl_priv_read_partition(AppleDiskLabel *adl, int part)
{
	off_t offset;
	ssize_t bytes;
	uint16_t bs = be16_to_cpu(adl->RawLabel.BlockSize);
	MacPartitionEntry *entry = &adl->Partitions[part].RawPartEntry;
	
	offset = adl_priv_lseek(adl, (part+1) * bs, SEEK_SET);
	if (offset < 0)
		return -1;

	/* XXX PJFIX handle short reads correctly */
	bytes = read(adl->fd, entry, sizeof (*entry));
	if (bytes < 0) {
		memset(entry, '\0', sizeof(*entry));
		return -1;
	}

	uint16_t magic = cpu_to_be16(MAC_PARTITION_MAGIC);
	if (memcmp(&entry->Signature, &magic, sizeof(magic))) {
		memset(entry, '\0', sizeof(*entry));
		errno = EINVAL;
		return -1;
	}
	adl->Partitions[part].Label = adl;

	return 0;
}

AppleDiskLabel *adl_read(int fd)
{
	int rc;

	AppleDiskLabel *adl = adl_new();
	if (!adl)
		return NULL;

	adl->fd = fd;
	adl->DiskLocation = lseek(fd, 0, SEEK_CUR);
	/* XXX PJFIX handle short reads correctly */
	rc = read(fd, &adl->RawLabel, sizeof (adl->RawLabel));
	if (rc < 0) {
		save_errno(adl_free(adl));
		return NULL;
	}

	uint16_t magic = cpu_to_be16(MAC_LABEL_MAGIC);
	if (memcmp(&adl->RawLabel.Signature, &magic, sizeof(magic))) {
		errno = EINVAL;
bad:
		adl_free(adl);
		return NULL;
	}

	errno = EINVAL;
	uint16_t bs = be32_to_cpu(adl->RawLabel.BlockSize);
	if (bs % 512 != 0)
		goto bad;

	rc = adl_priv_read_partition(adl, 0);
	if (rc < 0)
		goto bad;

	MacPartitionEntry *entry = &adl->Partitions[0].RawPartEntry;
	char buf[32];
	memset(buf, '\0', 32);
	memcpy(buf, "Apple", 5);

	errno = EINVAL;
	if (memcmp(entry->Name, buf, 32))
		goto bad;

	memcpy(buf, "Apple_partition_map", 19);
	if (memcmp(entry->Type, buf, 32))
		goto bad;

	uint32_t nparts = be32_to_cpu(entry->MapEntries);
	if (nparts < 1)
		goto bad;

	AppleDiskLabel *new = realloc(adl, adl_size(adl));
	if (!new)
		goto bad;
	adl = new;

	for (int i = 1; i < nparts; i++) {
		if (adl_priv_read_partition(adl, i) < 0)
			goto bad;
	}

	errno = 0;
	return adl;
}

void _adl_free(AppleDiskLabel **adlp)
{
	if (adlp && *adlp) {
		AppleDiskLabel *adl = *adlp;
		free(adl);
		*adlp = NULL;
	}
}

#ifdef TEST_DUMPER
static void usage(int retcode) __attribute__((noreturn));
static void usage(int retcode)
{
	FILE *f = retcode ? stderr : stdout;

	fprintf(f, "Usage: apmtest <inputfile>\n");
	exit(retcode);
}

int main(int argc, char *argv[])
{
	int fd;
	AppleDiskLabel *adl = NULL;

	if (argc != 2)
		usage(1);
	if (!strcmp(argv[1], "--help") ||
			!strcmp(argv[1], "--usage") ||
			!strcmp(argv[1], "-?") ||
			!strcmp(argv[1], "-h"))
		usage(0);

	fd = open(argv[1], O_RDONLY);
	if (!fd) {
		fprintf(stderr, "apmtest: cannot open \"%s\": %m\n", argv[1]);
		exit(2);
	}

	adl = adl_read(fd);
	save_errno(close(fd));
	if (!adl) {
		fprintf(stderr, "apmtest: cannot parse \"%s\": %m\n", argv[1]);
		exit(3);
	}


	adl_free(adl);
	return 0;
}
#endif

/* vim:set shiftwidth=8 softtabstop=8: */
