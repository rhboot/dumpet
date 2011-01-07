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
#ifndef LIBAPPLEPART_PRIVATE_H
#define LIBAPPLEPART_PRIVATE_H

#include "libapplepart.h"

struct AppleDiskPartition {
	AppleDiskLabel *Label;
	MacPartitionEntry RawPartEntry; /* always stored in big endian */
};

struct AppleDiskLabel {
	MacDiskLabel RawLabel; /* always stored in big endian */
	AppleDiskPartition Partitions[];
};

#endif /* LIBAPPLEPART_PRIVATE_H */
/* vim:set shiftwidth=8 softtabstop=8: */
