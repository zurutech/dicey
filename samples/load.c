// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/getopt.h"
#include "util/packet-dump.h"
#include "util/packet-json.h"
#include "util/packet-xml.h"

enum load_mode {
    LOAD_MODE_PROBE,
    LOAD_MODE_BINARY,
    LOAD_MODE_JSON,
    LOAD_MODE_XML,
};

static enum load_mode file_probe(const char *const path) {
    const char *const ext = strrchr(path, '.');
    if (ext) {
        if (!strcmp(ext, ".json")) {
            return LOAD_MODE_JSON;
        }

        if (!strcmp(ext, ".xml")) {
            return LOAD_MODE_XML;
        }
    }

    return LOAD_MODE_BINARY;
}

static void print_xml_errors(const struct util_xml_errors *const errs) {
    if (!errs->errors) {
        return;
    }

    const struct util_xml_error *const end = *errs->errors + errs->nerrs;
    for (const struct util_xml_error *err = *errs->errors; err < end; ++err) {
        fputs("error in XML input:", stderr);

        if (err->line) {
            fprintf(stderr, "line %d: ", err->line);

            if (err->col) {
                fprintf(stderr, ", col %d: ", err->col);
            }
        } else {
            fputc(' ', stderr);
        }

        fprintf(stderr, "%s\n", err->message);
    }
}

#define HELP_MSG                                                                                                       \
    "Usage: %s [options...] [FILE]\n"                                                                                  \
    "  -b  load FILE or stdin as a binary packet\n"                                                                    \
    "  -j  load FILE or stdin as a JSON-encoded packet\n"                                                              \
    "  -h  print this help message and exit\n"                                                                         \
    "  -o  dump binary output to FILE (requires -j or -x, implies -q)\n"                                               \
    "  -q  suppress output\n"                                                                                          \
    "  -v  enable extra-verbose output\n"                                                                              \
    "  -x  load FILE or stdin as an XML-encoded packet\n"                                                              \
    "\nIf not specified, FILE defaults to stdin. The extension is used to probe the contents of the file.\n"           \
    "If -q is not specified, a custom representation of the packet is printed to stdout.\n"

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    const char *fin = NULL, *fout = NULL;
    enum load_mode mode = LOAD_MODE_PROBE;
    bool quiet = false, verbose = false;

    int opt = 0;

    while ((opt = getopt(argc, argv, "bjho:qvx")) != -1) {
        switch (opt) {
        case 'b':
            mode = LOAD_MODE_BINARY;
            break;

        case 'j':
            mode = LOAD_MODE_JSON;
            break;

        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

        case 'o':
            fout = optarg;
            quiet = true;
            break;

        case 'q':
            quiet = true;
            break;

        case 'v':
            verbose = true;
            break;

        case 'x':
            mode = LOAD_MODE_XML;
            break;

        case '?':
            if (optopt == 'o') {
                fputs("error: -o requires an argument\n", stderr);
            } else {
                fprintf(stderr, "error: unknown option -%c\n", optopt);
            }

            print_help(progname, stderr);
            return EXIT_FAILURE;

        default:
            abort();
        }
    }

    if (verbose && quiet) {
        fputs("error: -q and -v are mutually exclusive\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    if (optind < argc) {
        fin = argv[optind];

        if (mode == LOAD_MODE_PROBE) {
            mode = file_probe(fin);
        }

        if (optind + 1 < argc) {
            fputs("error: too many arguments\n", stderr);

            print_help(progname, stderr);
            return EXIT_FAILURE;
        }
    }

    if (mode == LOAD_MODE_BINARY && fout) {
        fputs("error: -o requires -j or -x\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    FILE *in = stdin;
    if (fin) {
        in = fopen(fin, mode == LOAD_MODE_BINARY ? "rb" : "r");

        if (!in) {
            perror("fopen");
            return EXIT_FAILURE;
        }
    } else if (mode == LOAD_MODE_PROBE) {
        mode = LOAD_MODE_BINARY; // default to binary on stdin
    }

    uint8_t *dumped_bytes = NULL;
    size_t nbytes = 0, bcap = 0;

    while (!feof(in)) {
        uint8_t buf[4096];
        const size_t n = fread(buf, 1, sizeof buf, in);
        if (!n) {
            break;
        }

        const size_t new_len = nbytes + n;
        if (new_len > bcap) {
            bcap += sizeof buf;
            dumped_bytes = realloc(dumped_bytes, bcap);
            if (!dumped_bytes) {
                abort(); // this silences cppcheck, we don't care about safety in this dummy program
            }
        }

        memcpy(dumped_bytes + nbytes, buf, n);
        nbytes = new_len;
    }

    if (!nbytes || !dumped_bytes) {
        fputs("error: no input\n", stderr);
        return EXIT_FAILURE;
    }

    struct dicey_packet pkt = { 0 };

    enum dicey_error err = DICEY_OK;

    switch (mode) {
    case LOAD_MODE_BINARY:
        err = dicey_packet_load(&pkt, &(const void *) { dumped_bytes }, &(size_t) { nbytes });
        break;

    case LOAD_MODE_JSON:
        err = util_json_to_dicey(&pkt, dumped_bytes, nbytes);
        break;

    case LOAD_MODE_XML:
        {
            struct util_xml_errors errs = util_xml_to_dicey(&pkt, dumped_bytes, nbytes);
            if (errs.nerrs) {
                print_xml_errors(&errs);
                util_xml_errors_deinit(&errs);

                err = DICEY_EINVAL; // kinda sucks?
            }
            break;
        }

    default:
        abort();
    }

    if (err) {
        goto quit;
    }

    if (fout) {
        FILE *out = fopen(fout, "wb");
        if (!out) {
            perror("fopen");
            err = DICEY_EAGAIN; // TODO: replace with an IO-related error as soon as I add one
            goto quit;
        }

        const size_t written = fwrite(pkt.payload, 1, pkt.nbytes, out);
        fclose(out);

        if (written < pkt.nbytes) {
            perror("fwrite");
            err = DICEY_EAGAIN; // TODO: replace with an IO-related error as soon as I add one
            goto quit;
        }
    } else if (!quiet) {
        struct util_dumper dumper = util_dumper_for(stdout);

        if (verbose) {
            util_dumper_printlnf(&dumper, "packet loaded, size = %zu bytes", pkt.nbytes);
            util_dumper_dump_hex(&dumper, pkt.payload, pkt.nbytes);
            util_dumper_printlnf(&dumper, "");
        }

        util_dumper_dump_packet(&dumper, pkt);
    }

quit:
    dicey_packet_deinit(&pkt);
    free(dumped_bytes);
    fclose(in);

    if (err) {
        fprintf(stderr, "error: %s\n", dicey_error_msg(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
