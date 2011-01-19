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
#ifndef LIBAPPLEPART_H
#define LIBAPPLEPART_H

struct AppleDiskLabel;
typedef struct AppleDiskLabel AppleDiskLabel;
struct AppleDiskPartition;
typedef struct AppleDiskPartition AppleDiskPartition;

extern AppleDiskLabel *adl_new(void);
extern AppleDiskLabel *adl_read(int fd);
extern void _adl_free(AppleDiskLabel **adlp);
#define adl_free(adl) _adl_free(&(adl))

/* label accessors */
extern int adl_set_block_size(AppleDiskLabel *adl, uint16_t blocksize);
extern uint16_t adl_get_block_size(AppleDiskLabel *adl);

extern int adl_set_disk_blocks(AppleDiskLabel *adl, uint32_t nblocks);
extern uint32_t adl_get_disk_blocks(AppleDiskLabel *adl);

extern int adl_get_num_partitions(AppleDiskLabel *adl);

/* per-partition accessors */
extern int adl_set_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					  uint32_t block);
extern int adl_get_partition_pblock_start(AppleDiskLabel *adl, int partnum,
					  uint32_t *block);

extern int adl_set_partition_blocks(AppleDiskLabel *adl, int partnum,
				    uint32_t blocks);
extern int adl_get_partition_blocks(AppleDiskLabel *adl, int partnum,
				    uint32_t *blocks);

#define MAC_PARTITION_VALID		0x1
#define MAC_PARTITION_ALLOCATED		0x2
#define MAC_PARTITION_IN_USE		0x4
#define MAC_PARTITION_BOOTABLE		0x8
#define MAC_PARTITION_READABLE		0x10
#define MAC_PARTITION_WRITABLE		0x20
#define MAC_PARTITION_OS_PIC_CODE	0x40
#define MAC_PARTITION_OS_SPECIFIC_2	0x80
#define MAC_PARTITION_OS_SPECIFIC_1	0x100
#define MAC_PARTITION_OS_SPECIFIC_0	0x200
#define MAC_PARTITION_DUMMY		0x40000000
#define MAC_PARTITION_RESERVED_BITS	0xBFFFFC00
extern int adl_set_partition_flags(AppleDiskLabel *adl, int partnum,
				   uint32_t flags);
extern int adl_get_partition_flags(AppleDiskLabel *adl, int partnum,
				   uint32_t *flags);

extern int adl_set_partition_name(AppleDiskLabel *adl, int partnum, char *name);
extern int adl_get_partition_name(AppleDiskLabel *adl, int partnum,
				  char **name);

extern int adl_set_partition_type(AppleDiskLabel *adl, int partnum, char *type);
extern int adl_get_partition_type(AppleDiskLabel *adl, int partnum,
				  char **type);

extern int _adl_add_partition(AppleDiskLabel **adl);
#define adl_add_partition(adl) _adl_add_partition(&(adl))

#endif /* LIBAPPLEPART_H */
/* vim:set shiftwidth=8 softtabstop=8: */
