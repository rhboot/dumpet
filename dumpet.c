/*
 * dumpet -- A dumper for El Torito boot information.
 *
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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <popt.h>

#include "dumpet.h"
#include "endian.h"

static uint32_t dump_boot_record(const char *filename, FILE *iso)
{
	BootRecordVolumeDescriptor br;
	int rc;
	char BootSystemId[32];
	char *ElTorito = "EL TORITO SPECIFICATION";
	uint32_t BootCatalogLBA;

	memset(BootSystemId, '\0', sizeof(BootSystemId));
	memcpy(BootSystemId, ElTorito, strlen(ElTorito)); 

	rc = read_sector(iso, 17, (Sector *)&br);
	if (rc < 0)
		exit(3);
	//write(STDOUT_FILENO, br.Raw, 0x800);

	if (br.BootRecordIndicator != 0) {
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", filename);
		fprintf(stderr, "BootRecordIndicator: %d\n", br.BootRecordIndicator);
		exit(5);
	}
	if (strncmp(br.Iso9660, "CD001", 5)) {
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", filename);
		memcpy(BootSystemId, br.Iso9660, 5);
		BootSystemId[5] = '\0';
		fprintf(stderr, "ISO-9660 Identifier: \"%s\"\n", BootSystemId);
		exit(6);
	}
	if (memcmp(br.BootSystemId, BootSystemId, sizeof(BootSystemId))) {
		int i;
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", filename);
		fprintf(stderr, "target Boot System Identifier: \"");
		for (i = 0; i < sizeof(BootSystemId); i++)
			fprintf(stderr, "%02x", BootSystemId[i]);
		fprintf(stderr, "\n");
		memcpy(BootSystemId, br.BootSystemId, sizeof(BootSystemId));
		fprintf(stderr, "actual Boot System Identifier: \"");
		for (i = 0; i < sizeof(BootSystemId); i++)
			fprintf(stderr, "%02x", BootSystemId[i]);
		fprintf(stderr, "\n");

		exit(7);
	}
	memcpy(&BootCatalogLBA, &br.BootCatalogLBA, sizeof(BootCatalogLBA));
	return iso731_to_cpu32(BootCatalogLBA);
}

static int checkValidationEntry(BootCatalogValidationEntry *ValidationEntry)
{
	uint16_t sum = 0;
	char *ve = (char *)ValidationEntry;
	uint16_t checksum;
	int i;

	memcpy(&checksum, &ValidationEntry->Checksum, sizeof(checksum));
	checksum = iso721_to_cpu16(checksum);
	ValidationEntry->Checksum = 0;

	for (i = 0; i < 32; i+=2) {
		sum += ve[i];
		sum += ve[i+1] * 256;
	}

	sum += checksum;
	if (sum != 0) {
		printf("Validation Entry Checksum is incorrect: %d (%04x)\n",
			sum, sum);
		return 1;
	}

	return 0;
}

static int dumpEntry(BootCatalogEntry *bc, int header_num, int entry_num,
			int *next_header_num, const char *filename,
			int file_num)
{
	BootCatalogValidationEntry *ValidationEntry =
		(BootCatalogValidationEntry *)&bc[header_num];
	BootCatalogSectionHeaderEntry *SectionHeaderEntry =
		(BootCatalogSectionHeaderEntry *)&bc[header_num];
	uint16_t loadseg;
	uint16_t sectors;
	uint32_t lba;
	unsigned char platform_id;

	switch (ValidationEntry->HeaderIndicator) {
		case ValidationIndicator:
			platform_id = ValidationEntry->PlatformId;
			break;
		case SectionHeaderIndicator:
		case FinalSectionHeaderIndicator:
			platform_id = SectionHeaderEntry->PlatformId;
			break;
		default:
			/* almost always x86 anyway -- if it's broken, it's
			 * probably still x86. */
			platform_id = 0;
			break;		
	}

	if (ValidationEntry->HeaderIndicator == ValidationIndicator) {
		BootCatalogDefaultEntry *DefaultEntry =
			(BootCatalogDefaultEntry *)&bc[entry_num];

		printf("Boot Catalog Default Entry:\n");
		switch (DefaultEntry->BootIndicator) {
			case NotBootable:
				printf("\tEntry is not bootable\n");
				break;
			case Bootable:
				printf("\tEntry is bootable\n");
				break;
			default:
				printf("\tInvalid boot indicator\n");
				break;
		}

		printf("\tBoot Media emulation type: ");
		switch (DefaultEntry->BootMediaType) {
			case NoEmulation:
				printf("no emulation\n");
				break;
			case OneTwoDiskette:
				printf("1.2MB floppy diskette emulation\n");
				break;
			case OneFourFourDiskette:
				printf("1.44MB floppy diskette emulation\n");
				break;
			case TwoEightEightDiskette:
				printf("2.88MB floppy diskette emulation\n");
				break;
			case HardDisk:
				printf("hard disk emulation\n");
				break;
			default:
				printf("invalid boot media emulation type\n");
				break;
		}

		memcpy(&loadseg, &DefaultEntry->LoadSegment, sizeof(loadseg));
		loadseg = iso721_to_cpu16(loadseg);

		switch (platform_id) {
			case x86:
				printf("\tMedia load segment: 0x%04x\n",
					loadseg == 0 ? 0x7c0 : loadseg);
				break;
			case ppc:
			case m68kmac:
			case efi:
				printf("\tMedia load address: %d (0x%04x)\n",
					loadseg * 0x10, loadseg * 0x10);
				break;
			default:
				printf("\tMedia load address: %d (0x%04x) (raw value)\n",
					loadseg, loadseg);
				break;
		}

		printf("\tSystem type: %d (0x%02x)\n", DefaultEntry->SystemType,
			DefaultEntry->SystemType);

		memcpy(&sectors, &DefaultEntry->SectorCount, sizeof(sectors));
#if 0		/* genisoimage doesn't actually convert this -- so it's actually in 
		 * host byte order :(
		 */
		sectors = iso721_to_cpu16(sectors);
#endif
		printf("\tLoad Sectors: %d (0x%04x)\n", sectors, sectors);

		memcpy(&lba, &DefaultEntry->LoadLBA, sizeof(lba));
		lba = iso731_to_cpu32(lba);
		printf("\tLoad LBA: %d (0x%08x)\n", lba, lba);
	} else {
		BootCatalogSectionEntry *SectionEntry =
			(BootCatalogSectionEntry *)&bc[entry_num];

		printf("Boot Catalog Section Entry:\n");
		switch (SectionEntry->BootIndicator) {
			case NotBootable:
				printf("\tEntry is not bootable\n");
				break;
			case Bootable:
				printf("\tEntry is bootable\n");
				break;
			default:
				printf("\tInvalid boot indicator\n");
				break;
		}
#if 0
	printf("\tBoot Media emulation type: ");
	switch (DefaultEntry->BootMediaType) {
		case NoEmulation:
			printf("no emulation\n");
			break;
		case OneTwoDiskette:
			printf("1.2MB floppy diskette emulation\n");
			break;
		case OneFourFourDiskette:
			printf("1.44MB floppy diskette emulation\n");
			break;
		case TwoEightEightDiskette:
			printf("2.88MB floppy diskette emulation\n");
			break;
		case HardDisk:
			printf("hard disk emulation\n");
			break;
		default:
			printf("invalid boot media emulation type\n");
			break;
	}

	memcpy(&loadseg, &DefaultEntry->LoadSegment, sizeof(loadseg));
	loadseg = iso721_to_cpu16(loadseg);

	switch (platform_id) {
		case x86:
			printf("\tMedia load segment: 0x%04x\n",
				loadseg == 0 ? 0x7c0 : loadseg);
			break;
		case ppc:
		case m68kmac:
		case efi:
			printf("\tMedia load address: %d (0x%04x)\n",
				loadseg * 0x10, loadseg * 0x10);
			break;
		default:
			printf("\tMedia load address: %d (0x%04x) (raw value)\n",
				loadseg, loadseg);
			break;
	}

	printf("\tSystem type: %d (0x%02x)\n", DefaultEntry->SystemType,
		DefaultEntry->SystemType);

	memcpy(&sectors, &DefaultEntry->SectorCount, sizeof(sectors));
#if 0	/* genisoimage doesn't actually convert this -- so it's actually in 
	 * host byte order :(
	 */
	sectors = iso721_to_cpu16(sectors);
#endif
	printf("\tLoad Sectors: %d (0x%04x)\n", sectors, sectors);

	memcpy(&lba, &DefaultEntry->LoadLBA, sizeof(lba));
	lba = iso731_to_cpu32(lba);
	printf("\tLoad LBA: %d (0x%08x)\n", lba, lba);
#endif
	}

	return 0;
}

static int dumpet(const char *filename, FILE *iso)
{
	BootCatalog bc;
	uint32_t bootCatLba;
	int filenum = 0;
	int next_header_num = 0;
	int rc;

	bootCatLba = dump_boot_record(filename, iso);
	//fprintf(stdout, "Boot Catalog LBA is 0x%08x\n", bootCatLba);

	rc = read_sector(iso, bootCatLba, (Sector *)&bc);
	if (rc < 0)
		exit(4);

	rc = checkValidationEntry(&bc.Catalog[0].ValidationEntry);

	rc = dumpEntry(&bc.Catalog[0], next_header_num, next_header_num+1,
			&next_header_num, filename, filenum++);

	while (1) {
		BootCatalogSectionHeaderEntry *SectionHeader =
			(BootCatalogSectionHeaderEntry *)&bc.Catalog[next_header_num];

		if (SectionHeader->HeaderIndicator == SectionHeaderIndicator ||
				SectionHeader->HeaderIndicator == FinalSectionHeaderIndicator) {
			rc = dumpEntry(&bc.Catalog[0], next_header_num, next_header_num+1,
					&next_header_num, filename, filenum++);
		} else {
			break;
		}
	}

	//write(STDOUT_FILENO, &bc, sizeof(bc));

	return 0;
}

static void usage(int error)
{
	FILE *outfile = error ? stderr : stdout;

	fprintf(outfile, "usage: dumpet --help\n"
	                 "       dumpet -i <file> [-d]\n");
	exit(error);
}


int main(int argc, char *argv[])
{
	FILE *iso = NULL;
	int rc;

	int help = 0;
	int dumpDiskImage = 0;
	char *filename = NULL;

	poptContext optCon;
	struct poptOption optionTable[] = {
		{ "help", '?', POPT_ARG_NONE, &help, 0, NULL, "help"},
		{ "dumpdisks", 'd', POPT_ARG_NONE, &dumpDiskImage, 0, NULL, "dump each El Torito boot image into a file"},
		{ "iso", 'i', POPT_ARG_STRING, &filename, 0, NULL, "input ISO image"},
		{0}
	};

	optCon = poptGetContext(NULL, argc, (const char **)argv, optionTable, 0);

	if ((rc = poptGetNextOpt(optCon)) < -1) {
		fprintf(stderr, "dumpet: bad option \"%s\": %s\n",
			poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		usage(2);
		exit(2);
	}

	if (help)
		usage(0);
	else if (!filename)
		usage(3);

	iso = fopen(filename, "r");
	if (!iso) {
		fprintf(stderr, "Could not open \"%s\": %m\n", filename);
		exit(2);
	}

	rc = dumpet(filename, iso);
	
	free(filename);
	poptFreeContext(optCon);
	return rc;
}

/* vim:set shiftwidth=8 softtabstop=8: */
