// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>

#include <dicey/dicey.h>

#include "util/dumper.h"
#include "util/getopt.h"
#include "util/packet-dump.h"
#include "util/packet-json.h"
#include "util/packet-xml.h"

enum load_mode {
    LOAD_MODE_INVALID,
    LOAD_MODE_JSON,
    LOAD_MODE_XML,
};

static void inspector(struct dicey_client *const client, void *const ctx, struct dicey_client_event event) {
    (void) client;
    (void) ctx;

    assert(client);

    switch (event.type) {
    case DICEY_CLIENT_EVENT_CONNECT:
        puts("client connected");
        break;

    case DICEY_CLIENT_EVENT_ERROR:
        fprintf(stderr, "error: [%s] %s\n", dicey_error_msg(event.error.err), event.error.msg);

        if (dicey_client_stop(client) != DICEY_OK) {
            fprintf(stderr, "error: failed to stop client\n");

            exit(EXIT_FAILURE);
        }
        break;

    case DICEY_CLIENT_EVENT_HANDSHAKE_START:
        printf(
            "handshake started, presenting version %" PRId16 "r%" PRId16 "\n",
            event.version.major,
            event.version.revision
        );
        break;

    case DICEY_CLIENT_EVENT_HANDSHAKE_WAITING:
        puts("waiting for server to reply to handshake");
        break;

    case DICEY_CLIENT_EVENT_INIT:
        puts("client initialized");
        break;

    case DICEY_CLIENT_EVENT_MESSAGE_RECEIVING:
        puts("receiving message");
        break;

    case DICEY_CLIENT_EVENT_MESSAGE_SENDING:
        puts("sending message");
        break;

    case DICEY_CLIENT_EVENT_SERVER_BYE:
        puts("server said goodbye");
        break;

    case DICEY_CLIENT_EVENT_QUITTING:
        puts("client quitting");
        break;

    case DICEY_CLIENT_EVENT_QUIT:
        puts("client quit");
        break;

    default:
        abort();
    }
}

static void on_client_event(struct dicey_client *const client, void *const ctx, const struct dicey_packet packet) {
    (void) client;
    (void) ctx;

    assert(client);

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_printlnf(&dumper, "received event:");
    util_dumper_dump_packet(&dumper, packet);
}

static int do_send(char *addr, struct dicey_packet packet) {
    struct dicey_client *client = NULL;

    enum dicey_error err = dicey_client_new(
        &client,
        &(struct dicey_client_args) {
            .inspect_func = &inspector,
            .on_event = &on_client_event,
        }
    );

    if (err) {
        return err;
    }

    err = dicey_client_connect(client, dicey_addr_from_str(addr));
    if (err) {
        dicey_client_delete(client);
        dicey_packet_deinit(&packet);

        return err;
    }

    struct util_dumper dumper = util_dumper_for(stdout);
    util_dumper_printlnf(&dumper, "sending packet:");
    util_dumper_dump_packet(&dumper, packet);

    err = dicey_client_request(client, packet, &packet, 3000); // 3 seconds
    if (err) {
        dicey_client_delete(client);
        dicey_packet_deinit(&packet);

        return err;
    }

    util_dumper_printlnf(&dumper, "received packet:");
    util_dumper_dump_packet(&dumper, packet);

    return DICEY_OK;
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
    "Usage: %s [options...] SOCKET [FILE]\n"                                                                           \
    "  -j  load FILE or stdin as a JSON-encoded packet\n"                                                              \
    "  -h  print this help message and exit\n"                                                                         \
    "  -x  load FILE or stdin as an XML-encoded packet\n"                                                              \
    "\n"                                                                                                               \
    "If not specified, FILE defaults to stdin. The extension is used to probe the contents of the file.\n"             \
    "Any SEQ parameter will be ignored.\n"

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

    return LOAD_MODE_INVALID;
}

static void print_help(const char *const progname, FILE *const out) {
    fprintf(out, HELP_MSG, progname);
}

int main(const int argc, char *const *argv) {
    (void) argc;

    const char *const progname = argv[0];
    const char *fin = NULL;
    char *socket = NULL;
    enum load_mode mode = LOAD_MODE_INVALID;

    int opt = 0;

    while ((opt = getopt(argc, argv, "jhx")) != -1) {
        switch (opt) {
        case 'j':
            mode = LOAD_MODE_JSON;
            break;

        case 'h':
            print_help(progname, stdout);
            return EXIT_SUCCESS;

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

    switch (argc - optind) {
    case 0:
        fputs("error: missing socket or pipe name\n", stderr);
        return EXIT_FAILURE;

    case 2:
        fin = argv[optind + 1];

        if (mode == LOAD_MODE_INVALID) {
            mode = file_probe(fin);
        }

        // fallthrough

    case 1:
        socket = argv[optind];
        break;

    default:
        fputs("error: too many arguments\n", stderr);

        print_help(progname, stderr);
        return EXIT_FAILURE;
    }

    if (mode == LOAD_MODE_INVALID) {
        fputs("error: no input mode specified and no file format can be determined from file name\n", stderr);

        return EXIT_FAILURE;
    }

    FILE *in = stdin;
    if (fin) {
        in = fopen(fin, "r");

        if (!in) {
            perror("fopen");
            return EXIT_FAILURE;
        }
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

    err = do_send(socket, pkt);

    pkt = (struct dicey_packet) { 0 };

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
