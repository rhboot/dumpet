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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <popt.h>

typedef char Sector[0x800];

static inline off_t get_sector_offset(int sector_number)
{
	Sector sector;
	return sector_number * sizeof(sector);
}

static int dumpet(const char *filename, FILE *iso)
{
	Sector sector;
	size_t n;
	fseek(iso, get_sector_offset(16), SEEK_SET);

	n = fread(sector, sizeof(sector), 1, iso);

	if (n != 1) {
		fprintf(stderr, "dumpet: Error reading iso: %m\n");
		exit(3);
	}

	write(STDOUT_FILENO, sector, sizeof(sector));

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
