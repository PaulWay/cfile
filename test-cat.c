/*
 * test-cat.c
 * This file is part of The PaulWay Libraries
 *
 * Copyright (C) 2006 - Paul Wayper (paulway@mabula.net)
 * Copyright (C) 2012 Peter Miller
 *
 * The PaulWay Libraries are free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * The PaulWay Libraries are distributed in the hope that they will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/** \file test-cat.c
 * \brief A 'cat' analogue which uses the cfile library.
 *
 * test-cat is a 'cat' analogue which uses the cfile library.  It's used
 * as a partial test of the file reading routines provided by cfile.  To
 * test it, simply run
 * <tt>'test-cat $compressed_file | zdiff - $compressed_file'</tt>
 * (or whatever your local compressed-file-reading-diff variant is).
 * If the output is different, then obviously the cfile library is wrong!
 */

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "cfile.h"

static void *context = NULL;

static cfile *out = NULL;

void write_file (const char *name);

void write_file (const char *name) {
    /* write_file - read the named file and write it to stdout
     */
    cfile *in = cfile_open(name, "r");
    if (! in) {
        perror(name);
        exit(EXIT_FAILURE);
    }
    char *line = NULL;
    int linelen = 0;
    for (;;) {
        line = cfgetline(in, line, &linelen);
        if (! line)
            break;
        cfprintf(out, "%s", line);
    }
    if (cfclose(in))
    {
        /* e.g. ENOSPC */
        perror(name);
        exit(EXIT_FAILURE);
    }
}

static void usage(void) {
    fprintf(stderr, "Usage: test-cat [ <option>...][ <filename>... ]\n");
    exit(EXIT_FAILURE);
}


int main (int argc, char *argv[])
{
    int report_flag = 0;
    talloc_enable_leak_report();
    context = talloc_init("main test-cat context");
    cfile_set_context(context);
    for (;;)
    {
        static const struct option options[] =
        {
            { "output", 1, 0, 'o' },
            { "talloc-leak-report", 0, 0, 'r' },
            { 0, 0, 0, 0 }
        };
        int c = getopt_long(argc, argv, "o:r", options, 0);
        if (c == -1)
            break;
        switch (c)
        {
        case 'o':
            if (out)
                usage();
            out = cfile_open(optarg, "w");
            if (!out)
            {
                perror(optarg);
                exit(EXIT_FAILURE);
            }
            break;

        case 'r':
            report_flag = 1;
            break;

        default:
            usage();
        }
    }
    if (!out)
    {
        out = cfile_dopen(1, "w");
        if (!out)
        {
            perror("cfdopen");
            talloc_free(context);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        write_file("-");
    }
    else
    {
        for (;;)
        {
            write_file(argv[optind++]);
            if (optind >= argc)
                break;
        }
    }

    if (cfclose(out))
    {
        perror("cfclose");
        exit(EXIT_FAILURE);
    }
    talloc_free(context);

    if (report_flag)
    {
        /*
         * List what the talloc tree looks like, from the top level.
         */
        talloc_report_full(NULL, stderr);
    }
    return EXIT_SUCCESS;
}


/* vim: set ts=8 sw=4 et : */
