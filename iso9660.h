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
#ifndef ISO9660_H
#define ISO9660_H

#include "endian.h"

typedef char Sector[0x800];

/* ISO 9660 section 7.2.1 = 16 bit little endian */
#define cpu16_to_iso721(x) cpu_to_le16(x)
#define iso721_to_cpu16(x) le16_to_cpu(x)

/* ISO 9660 section 7.2.2 = 16 bit big endian */
#define cpu16_to_iso722(x) cpu_to_be16(x)
#define iso722_to_cpu16(x) be16_to_cpu(x)

/* ISO 9660 section 7.2.3 = Both endians! */
#define cpu16_to_iso723(x) ((uint32_t)(cpu_to_be16(x) | (cpu_to_le16(x) << 16)))
#define iso723_to_cpu16(x) be16_to_cpu((uint16_t)((x) & 0xffff))

/* ISO 9660 section 7.3.1 = 32 bit little endian */
#define cpu32_to_iso731(x) cpu_to_le32(x)
#define iso731_to_cpu32(x) le32_to_cpu(x)

/* ISO 9660 section 7.3.2 = 32 bit big endian */
#define cpu32_to_iso732(x) cpu_to_be32(x)
#define iso732_to_cpu32(x) be32_to_cpu(x)

/* ISO9660 section 7.3.3 = 32-bit both endians! */
#define cpu32_to_iso733(x) ((uint64_t)(cpu_to_be32(x) | ( ((uint64_t)cpu_to_le32(x)) << 32)))
#define iso733_to_cpu32(x) be32_to_cpu((uint32_t)((x) & 0xffffffff))

#if 0
static void test_iso_functions(void)
{
	char input[] = "\x01\x23\x45\x67\x89\xab\xcd\xef";
	uint16_t input16;
	uint32_t input32;
	uint64_t input64;

	uint16_t output16;
	uint32_t output32;
	uint64_t output64;

	memcpy(&input16, input, sizeof(input16));
	output16 = cpu16_to_iso721(input16);
	fprintf(stdout, "cpu16_to_iso721(0x%04x) = 0x%04x\n", input16, output16);
	input16 = output16;
	output16 = iso721_to_cpu16(input16);
	fprintf(stdout, "iso721_to_cpu16(0x%04x) = 0x%04x\n", input16, output16);

	memcpy(&input16, input, sizeof(input16));
	output16 = cpu16_to_iso722(input16);
	fprintf(stdout, "cpu16_to_iso722(0x%04x) = 0x%04x\n", input16, output16);
	input16 = output16;
	output16 = iso722_to_cpu16(input16);
	fprintf(stdout, "iso722_to_cpu16(0x%04x) = 0x%04x\n", input16, output16);

	memcpy(&input16, input, sizeof(input16));
	output32 = cpu16_to_iso723(input16);
	fprintf(stdout, "cpu16_to_iso723(0x%04x) = 0x%08x\n", input16, output32);
	input32 = output32;
	output16 = iso723_to_cpu16(input32);
	fprintf(stdout, "iso723_to_cpu16(0x%08x) = 0x%04x\n", input32, output16);

	memcpy(&input32, input, sizeof(input32));
	output32 = cpu32_to_iso731(input32);
	fprintf(stdout, "cpu32_to_iso731(0x%08x) = 0x%08x\n", input32, output32);
	input32 = output32;
	output32 = iso731_to_cpu32(input32);
	fprintf(stdout, "iso731_to_cpu32(0x%08x) = 0x%08x\n", input32, output32);

	memcpy(&input32, input, sizeof(input32));
	output32 = cpu32_to_iso732(input32);
	fprintf(stdout, "cpu32_to_iso732(0x%08x) = 0x%08x\n", input32, output32);
	input32 = output32;
	output32 = iso732_to_cpu32(input32);
	fprintf(stdout, "iso732_to_cpu32(0x%08x) = 0x%08x\n", input32, output32);

	memcpy(&input32, input, sizeof(input32));
	output64 = cpu32_to_iso733(input32);
	fprintf(stdout, "cpu32_to_iso733(0x%04x) = 0x%0" PRIx64 "\n", input32, output64);
	input64 = output64;
	output32 = iso733_to_cpu32(input64);
	fprintf(stdout, "iso733_to_cpu32(0x%0" PRIx64") = 0x%04x\n", input64, output32);

	exit(0);
}
#endif


#endif /* ISO9660_H */
/* vim:set shiftwidth=8 softtabstop=8: */
