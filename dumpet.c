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

#define _GNU_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>

#include <popt.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#include "dumpet.h"
#include "endian.h"

struct context {
	int dumpStdOut;
	int dumpDiskImage;
	int dumpHex;
	int dumpXml;

	xmlTextWriterPtr writer;
	char *filename;
	FILE *iso;
};

static uint32_t dump_boot_record(struct context *context)
{
	BootRecordVolumeDescriptor br;
	int rc;
	char BootSystemId[32] = "EL TORITO SPECIFICATION";
	uint32_t BootCatalogLBA;

	rc = read_sector(context->iso, 17, (Sector *)&br);
	if (rc < 0)
		exit(3);

	if (br.BootRecordIndicator != 0) {
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", context->filename);
		fprintf(stderr, "BootRecordIndicator: %d\n", br.BootRecordIndicator);
		exit(5);
	}
	if (strncmp(br.Iso9660, "CD001", 5)) {
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", context->filename);
		memcpy(BootSystemId, br.Iso9660, 5);
		BootSystemId[5] = '\0';
		fprintf(stderr, "ISO-9660 Identifier: \"%s\"\n", BootSystemId);
		exit(6);
	}
	if (memcmp(br.BootSystemId, BootSystemId, sizeof(BootSystemId))) {
		int i;
		fprintf(stderr, "\"%s\" does not contain an El Torito bootable image.\n", context->filename);
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

static int checkValidationEntry(struct context *context,
				BootCatalogValidationEntry *ValidationEntry)
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
	checksum = cpu16_to_iso721(checksum);
	memcpy(&ValidationEntry->Checksum, &checksum, sizeof(checksum));
	if (sum != 0)
		return -1;

	return 0;
}

static void snprintPlatformId(char *buf, size_t n, uint16_t platformId)
{
	switch (platformId) {
		case x86:
			snprintf(buf, n, "80x86");
			break;
		case ppc:
			snprintf(buf, n, "PPC");
			break;
		case m68kmac:
			snprintf(buf, n, "m68k Macintosh");
			break;
		case efi:
			snprintf(buf, n, "EFI");
			break;
		default:
			snprintf(buf, n, "Unknown");
			break;
	}
}

static void dumpHex(void *voiddata, ssize_t length)
{
	uint8_t *data = voiddata;
	int i, j;
	for (i = j = 0; i < length ; i++) {
		if (i % 16 == 0) {
			j = i;
			printf("%08x  ", i);
		}
		printf("%2.2x ", data[i]);
		if ((i+1) % 16 == 0) {
			printf(" |");
			for (; j <= i; j++) {
				if (isalnum(data[j]))
					printf("%c", data[j]);
				else
					printf(".");
			}
			printf("|\n");
		}
	}
}

static void dumpValidationEntry(BootCatalogValidationEntry *ValidationEntry, struct context *context)
{
	char platformbuf[16];
	char id_string[25];
	uint16_t csum;
	
	snprintPlatformId(platformbuf, 15, ValidationEntry->PlatformId);

	memcpy(id_string, ValidationEntry->Id, 24);
	id_string[24] = '\0';

	memcpy(&csum, &ValidationEntry->Checksum, sizeof(csum));
	csum = iso721_to_cpu16(csum);

	if (context->dumpStdOut) {
		printf("Validation Entry:\n");

		if (context->dumpHex)
			dumpHex(ValidationEntry, sizeof(*ValidationEntry));

		printf("\tHeader Indicator: 0x%02x (Validation Entry)\n",
			ValidationEntry->HeaderIndicator);
		printf("\tPlatformId: 0x%02x (%s)\n", ValidationEntry->PlatformId, platformbuf);
		printf("\tID: \"%s\"\n", id_string);
		printf("\tChecksum: 0x%04x\n", csum);
		printf("\tKey bytes: 0x%02x%02x\n", ValidationEntry->FiveFive,
						    ValidationEntry->AA);
	} else if (context->dumpXml) {
		xmlTextWriterStartElement(context->writer, BAD_CAST "BootCatalogValidationEntry");

		xmlTextWriterStartElement(context->writer, BAD_CAST "PlatformId");
		xmlTextWriterWriteFormatAttribute(context->writer,
					BAD_CAST "RawValue", "0x%02x",
					ValidationEntry->PlatformId);
		xmlTextWriterStartAttribute(context->writer, BAD_CAST "Value");
		xmlTextWriterWriteString(context->writer, BAD_CAST platformbuf);
		xmlTextWriterEndAttribute(context->writer);
		xmlTextWriterEndElement(context->writer);

		xmlTextWriterWriteFormatElement(context->writer,
			BAD_CAST "IdString", "%s", id_string);

		xmlTextWriterWriteFormatElement(context->writer,
			BAD_CAST "Checksum", "%04x", csum);

		xmlTextWriterWriteFormatElement(context->writer,
			BAD_CAST "KeyBytes", "%02x%02x",
			ValidationEntry->FiveFive, ValidationEntry->AA);
	}
}

static void dumpSectionHeaderEntry(BootCatalogSectionHeaderEntry *SectionHeaderEntry, struct context *context)
{
	char platformbuf[16];
	char id_string[29];

	snprintPlatformId(platformbuf, 15, SectionHeaderEntry->PlatformId);

	memcpy(id_string, SectionHeaderEntry->Id, 28);
	id_string[28] = '\0';

	if (context->dumpStdOut) {
		printf("Section Header Entry:\n");

		if (context->dumpHex)
			dumpHex(SectionHeaderEntry, sizeof(*SectionHeaderEntry));

		printf("\tHeader Indicator: 0x%02x ", SectionHeaderEntry->HeaderIndicator);
		switch (SectionHeaderEntry->HeaderIndicator) {
			case SectionHeaderIndicator:
				printf("(Section Header Entry)\n");
				break;
			case FinalSectionHeaderIndicator:
				printf("(Final Section Header Entry)\n");
				break;
		}

		printf("\tPlatformId: 0x%02x (%s)\n", SectionHeaderEntry->PlatformId, platformbuf);
		printf("\tSection Entries: %d\n", SectionHeaderEntry->SectionEntryCount);
		printf("\tID: \"%s\"\n", id_string);
	} else if (context->dumpXml) {
		xmlTextWriterStartElement(context->writer, BAD_CAST "BootCatalogSectionHeaderEntry");

		xmlTextWriterWriteFormatAttribute(context->writer,
			BAD_CAST "FinalSectionHeaderEntry", "%s",
			(SectionHeaderEntry->HeaderIndicator == FinalSectionHeaderIndicator ?
				  "True":"False"));

		xmlTextWriterStartElement(context->writer, BAD_CAST "PlatformId");
		xmlTextWriterWriteFormatAttribute(context->writer,
					BAD_CAST "RawValue", "0x%02x",
					SectionHeaderEntry->PlatformId);
		xmlTextWriterStartAttribute(context->writer, BAD_CAST "Value");
		xmlTextWriterWriteString(context->writer, BAD_CAST platformbuf);
		xmlTextWriterEndAttribute(context->writer);
		xmlTextWriterEndElement(context->writer);

		xmlTextWriterWriteFormatElement(context->writer,
			BAD_CAST "IdString", "%s", id_string);

		xmlTextWriterWriteFormatElement(context->writer,
			BAD_CAST "SectionEntryCount", "%d",
			SectionHeaderEntry->SectionEntryCount);
	}
}

static int dumpBootImage(struct context *context,
			uint32_t lba, uint32_t sectors,
			int filenum)
{
	int i;
	int rc = 0;

	if (context->dumpStdOut) {
		FILE *image = NULL;
		char *filename = NULL;
		char *template = context->filename;
		Sector sector;

		rc = asprintf(&filename, "%s.%d", template, filenum);
		if (rc < 0)
			return rc;
		printf("Dumping boot image to \"%s\"\n", filename);
		image = fopen(filename, "w+");
		if (!image) {
			int errnum;
			fprintf(stderr, "Could not open \"%s\": %m\n", filename);
			errnum = errno;
			free(filename);
			return -errnum;
		}
		free(filename);
		for (i = 0; i<sectors; i++) {
			rc = read_sector(context->iso, lba++, &sector);
			if (rc < 0) {
				fclose(image);
				return rc;
			}
			rc = write_sector(image, i, &sector);
			if (rc < 0) {
				fclose(image);
				return rc;
			}
		}
	} else if (context->dumpXml) {
		Sector data[sectors];
		xmlTextWriterStartElement(context->writer, BAD_CAST "BootImage");
		int n = 0;

		for (i = 0; i<sectors; i++) {
			rc = read_sector(context->iso, lba++, &data[i]);
			if (rc < 0)
				break;
			n++;
		}
		xmlTextWriterWriteFormatAttribute(context->writer,
			BAD_CAST "HeaderSize", "0x%x", sectors * 2048);
		xmlTextWriterWriteFormatAttribute(context->writer,
			BAD_CAST "ActualSize", "0x%x", n * 2048);
		xmlTextWriterWriteBinHex(context->writer, (char *)data,
			0, n * sizeof data[0]);
		xmlTextWriterEndElement(context->writer); /* end BootImage */
	}
	return 0;
}

static void snprintBootMediaType(char *buf, size_t n, BootMediaType type)
{
	switch (type) {
		case NoEmulation:
			snprintf(buf, n, "no emulation");
			break;
		case OneTwoDiskette:
			snprintf(buf, n, "1.2MB floppy diskette emulation");
			break;
		case OneFourFourDiskette:
			snprintf(buf, n, "1.44MB floppy diskette emulation");
			break;
		case TwoEightEightDiskette:
			snprintf(buf, n, "2.88MB floppy diskette emulation");
			break;
		case HardDisk:
			snprintf(buf, n, "hard disk emulation");
			break;
		default:
			snprintf(buf, n, "invalid boot media emulation type");
			break;
	}
}

static int dumpEntry(FILE *iso, BootCatalogEntry *bc, int header_num,
			int entry_num, int *next_header_num,
			int file_num, struct context *context)
{
	BootCatalogValidationEntry *ValidationEntry =
		(BootCatalogValidationEntry *)&bc[header_num];
	BootCatalogSectionHeaderEntry *SectionHeaderEntry =
		(BootCatalogSectionHeaderEntry *)&bc[header_num];
	unsigned char platform_id;

	switch (ValidationEntry->HeaderIndicator) {
		case ValidationIndicator:
			platform_id = ValidationEntry->PlatformId;
			dumpValidationEntry(ValidationEntry, context);
			break;
		case SectionHeaderIndicator:
		case FinalSectionHeaderIndicator:
			platform_id = SectionHeaderEntry->PlatformId;
			dumpSectionHeaderEntry(SectionHeaderEntry, context);
			break;
		default:
			/* almost always x86 anyway -- if it's broken, it's
			 * probably still x86. */
			platform_id = 0;
			if (context->dumpStdOut) {
				fprintf(stderr,
					"Invalid Header Indicator (0x%04x), skipping\n",
					ValidationEntry->HeaderIndicator);
			}
			return -1;
	}

	if (ValidationEntry->HeaderIndicator == ValidationIndicator) {
		BootCatalogDefaultEntry *DefaultEntry =
			(BootCatalogDefaultEntry *)&bc[entry_num];

		uint16_t loadseg;
		uint16_t sectors;
		uint32_t lba;
		char bmtype[64];

		snprintBootMediaType(bmtype, 63, DefaultEntry->BootMediaType);

		memcpy(&loadseg, &DefaultEntry->LoadSegment, sizeof(loadseg));
		loadseg = iso721_to_cpu16(loadseg);

		memcpy(&sectors, &DefaultEntry->SectorCount, sizeof(sectors));
		sectors = iso721_to_cpu16(sectors);

		memcpy(&lba, &DefaultEntry->LoadLBA, sizeof(lba));
		lba = iso731_to_cpu32(lba);

		if (context->dumpStdOut) {
			uint16_t loadseg;
			uint16_t sectors;
			uint32_t lba;
			char bmtype[64];

			snprintBootMediaType(bmtype, 63, DefaultEntry->BootMediaType);

			memcpy(&loadseg, &DefaultEntry->LoadSegment, sizeof(loadseg));
			loadseg = iso721_to_cpu16(loadseg);

			memcpy(&sectors, &DefaultEntry->SectorCount, sizeof(sectors));
			sectors = iso721_to_cpu16(sectors);

			memcpy(&lba, &DefaultEntry->LoadLBA, sizeof(lba));
			lba = iso731_to_cpu32(lba);

			printf("Boot Catalog Default Entry:\n");

			if (context->dumpHex)
				dumpHex(DefaultEntry, sizeof(*DefaultEntry));

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

			printf("\tBoot Media emulation type: %s\n", bmtype);

			switch (platform_id) {
				case x86:
					printf("\tMedia load segment: ");
					if (loadseg == 0)
						printf("0x0 (0000:7c00)\n");
					else
						printf("0x%04x (%04x:0000)\n", loadseg,
							loadseg);
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

			printf("\tLoad Sectors: %d (0x%04x)\n", sectors, sectors);

			printf("\tLoad LBA: %d (0x%08x)\n", lba, lba);

		} else if (context->dumpXml) {
			xmlTextWriterStartElement(context->writer,
				BAD_CAST "BootCatalogDefaultEntry");

			xmlTextWriterWriteFormatElement(context->writer,
				BAD_CAST "Bootable", "%s",
				DefaultEntry->BootIndicator ? "True" : "False");

			xmlTextWriterWriteFormatElement(context->writer,
				BAD_CAST "BootMediaEmulationType", "%s", bmtype);

			xmlTextWriterStartElement(context->writer,
				BAD_CAST "MediaLoadSegment");
			switch (platform_id) {
				case x86:
					xmlTextWriterWriteFormatAttribute(context->writer,
						BAD_CAST "RawValue",
						"0x%04x", loadseg);
					if (loadseg == 0) {
						xmlTextWriterWriteFormatAttribute(
							context->writer,
							BAD_CAST "Value",
							"0000:7c00");
					} else {
						xmlTextWriterWriteFormatAttribute(
							context->writer,
							BAD_CAST "Value",
							"%04x:0000", loadseg);
					}
					xmlTextWriterWriteFormatAttribute(
						context->writer, BAD_CAST "ValueType",
						"Interpreted");
					break;
				case ppc:
				case m68kmac:
				case efi:
					xmlTextWriterWriteFormatAttribute(context->writer,
						BAD_CAST "RawValue",
						"0x%04x", loadseg);
					xmlTextWriterWriteFormatAttribute(context->writer,
						BAD_CAST "Value",
						"0x%04x", loadseg * 0x10);
					xmlTextWriterWriteFormatAttribute(
						context->writer, BAD_CAST "ValueType",
						"Interpreted");
					break;
				default:
					xmlTextWriterWriteFormatAttribute(context->writer,
						BAD_CAST "RawValue",
						"0x%04x", loadseg);
					xmlTextWriterWriteFormatAttribute(context->writer,
						BAD_CAST "Value",
						"0x%04x", loadseg);
					xmlTextWriterWriteFormatAttribute(
						context->writer, BAD_CAST "ValueType",
						"Raw");
					break;
			}
			xmlTextWriterEndElement(context->writer); /* end MediaLoadSegment */

			xmlTextWriterWriteFormatElement(context->writer,
				BAD_CAST "SystemType", "0x%02x",
				DefaultEntry->SystemType);

			xmlTextWriterWriteFormatElement(context->writer,
				BAD_CAST "LoadSectors", "0x%04x", sectors);

			xmlTextWriterWriteFormatElement(context->writer,
				BAD_CAST "LoadLBA", "0x%08x", lba);
		}

		if (context->dumpDiskImage)
			dumpBootImage(context, lba, sectors, file_num++);

		if (context->dumpXml)
			xmlTextWriterEndElement(context->writer); /* end BootCatalogDefaultEntry */

		*next_header_num = entry_num + 1;
	} else {
		int i;

		for (i = 0; i < SectionHeaderEntry->SectionEntryCount; i++) {
			BootCatalogSectionEntry *SectionEntry =
				(BootCatalogSectionEntry *)&bc[entry_num + i];

			uint16_t loadseg;
			uint16_t sectors;
			uint32_t lba;
			char bmtype[64];

			snprintBootMediaType(bmtype, 63, SectionEntry->BootMediaType);

			memcpy(&loadseg, &SectionEntry->LoadSegment, sizeof(loadseg));
			loadseg = iso721_to_cpu16(loadseg);

			memcpy(&sectors, &SectionEntry->SectorCount, sizeof(sectors));
			sectors = iso721_to_cpu16(sectors);

			memcpy(&lba, &SectionEntry->LoadLBA, sizeof(lba));
			lba = iso731_to_cpu32(lba);

			if (context->dumpStdOut) {
				printf("Boot Catalog Section Entry:\n");

				if (context->dumpHex)
					dumpHex(SectionEntry, sizeof(*SectionEntry));

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
				printf("\tBoot Media emulation type: %s\n", bmtype);

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

				printf("\tSystem type: %d (0x%02x)\n", SectionEntry->SystemType,
					SectionEntry->SystemType);
				printf("\tLoad Sectors: %d (0x%04x)\n", sectors, sectors);
				printf("\tLoad LBA: %d (0x%08x)\n", lba, lba);

			} else if (context->dumpXml) {
				xmlTextWriterStartElement(context->writer,
					BAD_CAST "BootCatalogSectionEntry");

				xmlTextWriterWriteFormatElement(context->writer,
					BAD_CAST "Bootable", "%s",
					SectionEntry->BootIndicator ? "True" : "False");

				xmlTextWriterWriteFormatElement(context->writer,
					BAD_CAST "BootMediaEmulationType", "%s", bmtype);

				xmlTextWriterStartElement(context->writer,
					BAD_CAST "MediaLoadSegment");
				switch (platform_id) {
					case x86:
						xmlTextWriterWriteFormatAttribute(context->writer,
							BAD_CAST "RawValue",
							"0x%04x", loadseg);
						if (loadseg == 0) {
							xmlTextWriterWriteFormatAttribute(
								context->writer,
								BAD_CAST "Value",
								"0000:7c00");
						} else {
							xmlTextWriterWriteFormatAttribute(
								context->writer,
								BAD_CAST "Value",
								"%04x:0000", loadseg);
						}
						xmlTextWriterWriteFormatAttribute(
							context->writer, BAD_CAST "ValueType",
							"Interpreted");
						break;
					case ppc:
					case m68kmac:
					case efi:
						xmlTextWriterWriteFormatAttribute(context->writer,
							BAD_CAST "RawValue",
							"0x%04x", loadseg);
						xmlTextWriterWriteFormatAttribute(context->writer,
							BAD_CAST "Value",
							"0x%04x", loadseg * 0x10);
						xmlTextWriterWriteFormatAttribute(
							context->writer, BAD_CAST "ValueType",
							"Interpreted");
						break;
					default:
						xmlTextWriterWriteFormatAttribute(context->writer,
							BAD_CAST "RawValue",
							"0x%04x", loadseg);
						xmlTextWriterWriteFormatAttribute(context->writer,
							BAD_CAST "Value",
							"0x%04x", loadseg);
						xmlTextWriterWriteFormatAttribute(
							context->writer, BAD_CAST "ValueType",
							"Raw");
						break;
				}
				xmlTextWriterEndElement(context->writer); /* end MediaLoadSegment */

				xmlTextWriterWriteFormatElement(context->writer,
					BAD_CAST "SystemType", "0x%02x",
					SectionEntry->SystemType);

				xmlTextWriterWriteFormatElement(context->writer,
					BAD_CAST "LoadSectors", "0x%04x", sectors);

				xmlTextWriterWriteFormatElement(context->writer,
					BAD_CAST "LoadLBA", "0x%08x", lba);
			}

			if (context->dumpDiskImage)
				dumpBootImage(context, lba, sectors, file_num++);

			if (context->dumpXml)
				xmlTextWriterEndElement(context->writer); /* end BootCatalogSectionEntry */
		}
		*next_header_num = entry_num + i;
	}

	if (context->dumpXml) {
		/* Either A ValidationEntry or a SectionHeaderEntry is open here */
		xmlTextWriterEndElement(context->writer);
	}

	return 0;
}

static int dumpet(struct context *context)
{
	BootCatalog bc;
	uint32_t bootCatLba;
	int filenum = 0;
	int next_header_num = 0;
	int rc;

	bootCatLba = dump_boot_record(context);

	rc = read_sector(context->iso, bootCatLba, (Sector *)&bc);
	if (rc < 0)
		exit(4);

	rc = checkValidationEntry(context, &bc.Catalog[0].ValidationEntry);
	if (rc < 0) {
		if (context->dumpStdOut)
			printf("Validation Entry Checksum is incorrect\n");
		return -1;
	}
	rc = dumpEntry(context->iso, &bc.Catalog[0],
			next_header_num, next_header_num+1, &next_header_num,
			filenum++, context);

	while (1) {
		BootCatalogSectionHeaderEntry *SectionHeader =
			(BootCatalogSectionHeaderEntry *)&bc.Catalog[next_header_num];

		if (SectionHeader->HeaderIndicator == SectionHeaderIndicator ||
				SectionHeader->HeaderIndicator == FinalSectionHeaderIndicator) {
			rc = dumpEntry(context->iso, &bc.Catalog[0], next_header_num, next_header_num+1,
					&next_header_num, filenum++, context);
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
	                 "       dumpet -i <file> [-d] [-h|-x]\n");
	exit(error);
}

int main(int argc, char *argv[])
{
	int rc;

	int help = 0;
	struct context context = { 0 };
	xmlBufferPtr xml = NULL;

	poptContext optCon;
	struct poptOption optionTable[] = {
		{ "help", '?', POPT_ARG_NONE, &help, 0, NULL, "help"},
		{ "dumpdisks", 'd', POPT_ARG_NONE, &context.dumpDiskImage, 0, NULL, "dump each El Torito boot image into a file"},
		{ "dumphex", 'h', POPT_ARG_NONE, &context.dumpHex, 0, NULL, "dump each El Torito structure in hex"},
		{ "iso", 'i', POPT_ARG_STRING, &context.filename, 0, NULL, "input ISO image"},
		{ "xml", 'x', POPT_ARG_NONE, &context.dumpXml, 0, NULL, "dump the El Torito structure as an XML document"},
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
	else if (!context.filename)
		usage(3);

	context.iso = fopen(context.filename, "r");
	if (!context.iso) {
		fprintf(stderr, "Could not open \"%s\": %m\n", context.filename);
		exit(2);
	}

	if (context.dumpXml) {
		xml = xmlBufferCreate();
		if (!xml) {
			fprintf(stderr, "Error creating XML buffer: %m\n");
			exit(3);
		}
		context.writer = xmlNewTextWriterMemory(xml, 0);
		if (!context.writer) {
			fprintf(stderr, "Error creating XML writer\n");
			exit(3);
		}
		rc = xmlTextWriterStartDocument(context.writer, NULL, "UTF-8", NULL);
		if (rc < 0) {
			fprintf(stderr, "Error starting new XML document\n");
			exit(3);
		}
		rc = xmlTextWriterStartElement(context.writer, BAD_CAST "ElToritoBootCatalog");
		if (rc < 0) {
			fprintf(stderr, "Error creating element \"El-Torito\"\n");
			exit(3);
		}
	} else {
		context.dumpStdOut = 1;
	}

	rc = dumpet(&context);
	
	fclose(context.iso);
	free(context.filename);

	poptFreeContext(optCon);

	if (context.dumpXml) {
		rc = xmlTextWriterEndElement(context.writer);
		rc = xmlTextWriterEndDocument(context.writer);
		xmlFreeTextWriter(context.writer);
		printf("%s", xml->content);
		xmlBufferFree(xml);
	}

	return rc;
}

/* vim:set shiftwidth=8 softtabstop=8: */
