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
#ifndef LIBAPPLEPART_H
#define LIBAPPLEPART_H

#include "applepart.h"

struct AppleDiskLabel;
typedef struct AppleDiskLabel AppleDiskLabel;
struct AppleDiskPartition;
typedef struct AppleDiskPartition AppleDiskPartition;

extern AppleDiskLabel *adl_new(void);
extern AppleDiskLabel *adl_read(int fd);

#endif /* LIBAPPLEPART_H */
/* vim:set shiftwidth=8 softtabstop=8: */
