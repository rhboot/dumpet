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

#include <popt.h>

static void usage(int error)
{
	FILE *outfile = error ? stderr : stdout;

	fprintf(outfile, "usage: dumpet <file>\n");
	exit(error);
}

typedef char Sector[0x800];

static int dumpet(const char *filename, FILE *iso)
{
	Sector sector;
	fseek(iso, 16 * sizeof(sector), SEEK_SET);

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *iso = NULL;
	int rc;

	int help = 0;
	int dumpDiskImage = 0;

	poptContext optCon;
	struct poptOption optionTable[] = {
		{ "help", '?', POPT_ARG_NONE, &help, 0, NULL, NULL},
		{ "dumpdisks", 'd', POPT_ARG_NONE, &dumpDiskImage, 0, NULL, NULL},
		{0}
	};

	optCon = poptGetContext(NULL, argc, (const char **)argv, optionTable, 0);

	if ((rc = poptGetNextOpt(optCon)) < -1) {
		fprintf(stderr, "dumpet: bad option \"%s\": %s\n",
			poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
			poptStrerror(rc));
		exit(2);
	}

	if (argc != 2)
		usage(1);
	if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-?"))
		usage(0);

	iso = fopen(argv[1], "r");
	if (!iso) {
		fprintf(stderr, "Could not open \"%s\": %m\n", argv[1]);
		exit(2);
	}

	ret = dumpet(iso);
	
	return ret;
}

/* vim:set shiftwidth=8 softtabstop=8: */
