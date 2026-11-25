#define _POSIX_C_SOURCE 200809L  /* for getopt */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "linebuf.h"
#include "record.h"
#include "report.h"
#include "stats.h"

enum { EXIT_USAGE = 1, EXIT_FILE = 2, EXIT_MEMORY = 3 };

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] <logfile | ->\n"
        "\n"
        "Parses a telecom log and prints a summary report.\n"
        "Use '-' to read from standard input.\n"
        "\n"
        "Options:\n"
        "  -t PCT    flag cells whose dropped+failed rate exceeds PCT (default 10)\n"
        "  -n N      number of top callers to list (default 5)\n"
        "  -o FILE   write the report to FILE instead of the screen\n"
        "  -v        print every malformed line and the reason to stderr\n"
        "  -h        show this help\n"
        "\n"
        "Log format (one record per line):\n"
        "  YYYY-MM-DD HH:MM:SS|CALL|SMS|DATA|CELL###|caller|callee|seconds|OK|DROPPED|FAILED\n",
        prog);
}

int main(int argc, char *argv[])
{
    double threshold = 10.0;
    size_t top_n = 5;
    const char *out_path = NULL;
    int verbose = 0;
    int opt;

    while ((opt = getopt(argc, argv, "t:n:o:vh")) != -1) {
        char *end;
        switch (opt) {
        case 't':
            threshold = strtod(optarg, &end);
            if (*end != '\0' || threshold < 0 || threshold > 100) {
                fprintf(stderr, "error: -t needs a number from 0 to 100\n");
                return EXIT_USAGE;
            }
            break;
        case 'n': {
            long n = strtol(optarg, &end, 10);
            if (*end != '\0' || n < 0) {
                fprintf(stderr, "error: -n needs a non-negative whole number\n");
                return EXIT_USAGE;
            }
            top_n = (size_t)n;
            break;
        }
        case 'o': out_path = optarg; break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return EXIT_USAGE;
        }
    }
    if (optind != argc - 1) {
        usage(argv[0]);
        return EXIT_USAGE;
    }

    const char *in_path = argv[optind];
    FILE *in = strcmp(in_path, "-") == 0 ? stdin : fopen(in_path, "r");
    if (!in) {
        fprintf(stderr, "error: cannot open '%s': %s\n", in_path, strerror(errno));
        return EXIT_FILE;
    }

    Stats stats;
    stats_init(&stats);
    LineBuf lb;
    linebuf_init(&lb);
    int status = 0;
    long len;
    char reason[128];

    while ((len = linebuf_read(&lb, in)) >= 0) {
        stats.lines++;

        /* skip blank lines and # comments */
        char *p = lb.buf;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || *p == '\0' || *p == '#') {
            stats.blank++;
            continue;
        }

        LogRecord rec;
        if (!parse_record(lb.buf, &rec, reason, sizeof reason)) {
            stats.malformed++;
            if (verbose)
                fprintf(stderr, "line %lu: %s\n", stats.lines, reason);
            continue;
        }
        if (stats_add(&stats, &rec) != 0) {
            len = LINEBUF_NOMEM;
            break;
        }
    }

    if (len == LINEBUF_NOMEM) {
        fprintf(stderr, "error: out of memory at line %lu\n", stats.lines);
        status = EXIT_MEMORY;
    } else if (ferror(in)) {
        fprintf(stderr, "error: failed while reading '%s'\n", in_path);
        status = EXIT_FILE;
    }
    if (in != stdin)
        fclose(in);

    if (status == 0) {
        FILE *out = out_path ? fopen(out_path, "w") : stdout;
        if (!out) {
            fprintf(stderr, "error: cannot write '%s': %s\n", out_path, strerror(errno));
            status = EXIT_FILE;
        } else {
            ReportOptions ropt = {
                .source = strcmp(in_path, "-") == 0 ? "(stdin)" : in_path,
                .fail_threshold = threshold,
                .top_n = top_n,
                .peak_line_buf = lb.cap,
            };
            write_report(out, &stats, &ropt);
            if (out != stdout) {
                fclose(out);
                fprintf(stderr, "Report written to %s\n", out_path);
            }
        }
    }

    linebuf_free(&lb);
    stats_free(&stats);
    return status;
}
