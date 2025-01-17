/*
 * Copyright (c) 2024-2025 Zuru Tech HK Limited, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// thank you MS, but just no
#define _CRT_SECURE_NO_WARNINGS 1
#define _XOPEN_SOURCE 700

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"

#if defined(DICEY_IS_UNIX)
#include <unistd.h>
#define STDOUT_IS_PIPED() (!isatty(fileno(stdout)))
#else
#define STDOUT_IS_PIPED() false
#endif

struct pupil {
    const char *name;
    uint8_t age;
};

static enum dicey_error pupil_dump(const struct pupil *const pupil, struct dicey_value_builder *const tuple) {
    return dicey_value_builder_set(
        tuple,
        (struct dicey_arg) { .type = DICEY_TYPE_PAIR, .pair = {
            .first = &(struct dicey_arg){ .type = DICEY_TYPE_STR, .str = pupil->name },
            .second = &(struct dicey_arg){ .type = DICEY_TYPE_BYTE, .byte = pupil->age },
        } }
    );
}

struct classroom {
    const char *name;

    struct pupil *pupils;
    size_t npupils;
};

static enum dicey_error classroom_dump(
    const struct classroom *const classroom,
    struct dicey_value_builder *const tuple
) {
    struct dicey_value_builder item = { 0 };

    enum dicey_error err = dicey_value_builder_tuple_start(tuple);
    if (err) {
        return err;
    }

    err = dicey_value_builder_next(tuple, &item);
    if (err) {
        return err;
    }

    err = dicey_value_builder_set(&item, (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = classroom->name });
    if (err) {
        return err;
    }

    struct dicey_value_builder pupils = { 0 };
    err = dicey_value_builder_next(tuple, &pupils);
    if (err) {
        return err;
    }

    err = dicey_value_builder_array_start(&pupils, DICEY_TYPE_PAIR);
    if (err) {
        return err;
    }

    const struct pupil *const end = classroom->pupils + classroom->npupils;
    for (const struct pupil *pupil = classroom->pupils; pupil < end; ++pupil) {
        err = dicey_value_builder_next(&pupils, &item);
        if (err) {
            return err;
        }

        err = pupil_dump(pupil, &item);
        if (err) {
            return err;
        }
    }

    err = dicey_value_builder_array_end(&pupils);
    if (err) {
        return err;
    }

    return dicey_value_builder_tuple_end(tuple);
}

static enum dicey_error classes_dump(
    const struct classroom *const classes,
    const size_t nclasses,
    struct dicey_value_builder *const array
) {
    enum dicey_error err = dicey_value_builder_array_start(array, DICEY_TYPE_TUPLE);
    if (err) {
        return err;
    }

    const struct classroom *const end = classes + nclasses;
    for (const struct classroom *classroom = classes; classroom < end; ++classroom) {
        struct dicey_value_builder item = { 0 };
        err = dicey_value_builder_next(array, &item);
        if (err) {
            return err;
        }

        err = classroom_dump(classroom, &item);
        if (err) {
            return err;
        }
    }

    return dicey_value_builder_array_end(array);
}

static void print_help(const char *const progname) {
    fprintf(stderr, "usage: %s [-bt] [DESTFILE]\n", progname);
}

enum output_fmt_choice {
    OUTPUT_FMT_UNDEF,
    OUTPUT_FMT_BINARY,
    OUTPUT_FMT_TEXT,
};

struct output_fmt_out {
    FILE *f;
    bool is_binary;
};

static bool output_fmt_pick(struct output_fmt_out *const out, const enum output_fmt_choice choice, const char *fout) {
    bool is_binary = choice == OUTPUT_FMT_BINARY;

    // `-` conventionally means stdout
    if (fout && !strcmp(fout, "-")) {
        fout = NULL;
    }

    if (choice == OUTPUT_FMT_UNDEF) {
        // if a file is specified, pick binary if text is not explictlly requested
        // if no file is specified, check if stdout is a pipe and not a TTY - if that's the case, pick binary
        // NOTE: on Windows, we can't check if stdout is a pipe, so we always pick text
        is_binary = fout || STDOUT_IS_PIPED();
    }

    FILE *f = stdout;
    if (fout) {
        f = fopen(fout, is_binary ? "wb" : "w");
        if (!f) {
            return false;
        }
    }

    *out = (struct output_fmt_out) { .f = f, .is_binary = is_binary };

    return true;
}

int main(const int argc, const char *argv[]) {
    (void) argc; // unused

    const char *const progname = argv[0];
    const char *fout = NULL;
    enum output_fmt_choice fmt = OUTPUT_FMT_UNDEF;

    while (*++argv) {
        if (!strcmp(*argv, "-h")) {
            print_help(progname);
            return EXIT_SUCCESS;
        }

        if (!strcmp(*argv, "-b") || !strcmp(*argv, "-t")) {
            if (fmt != OUTPUT_FMT_UNDEF) {
                fprintf(stderr, "error: multiple output format options specified\n");
                return EXIT_FAILURE;
            }

            fmt = OUTPUT_FMT_TEXT;
            continue;
        }

        if (**argv == '-') {
            fprintf(stderr, "error: unknown option '%s'\n", *argv);
            return EXIT_FAILURE;
        }

        if (fout) {
            fprintf(stderr, "error: multiple output files specified\n");
            return EXIT_FAILURE;
        }

        fout = *argv;
    }

    struct output_fmt_out out = { 0 };
    if (!output_fmt_pick(&out, fmt, fout)) {
        fprintf(stderr, "error: failed to open file '%s' for writing\n", fout);
        return EXIT_FAILURE;
    }

    const struct classroom classes[3] = {
        {
         .name = "A",
         .pupils =
                (struct pupil[]) {
                    {
                        .name = "Alice",
                        .age = 10U,
                    },
                    {
                        .name = "Bob",
                        .age = 11U,
                    },
                    {
                        .name = "Charlie",
                        .age = 12U,
                    },
                },                 .npupils = 3U,
         },
        {
         .name = "B",
         .pupils =
                (struct pupil[]) {
                    {
                        .name = "Dave",
                        .age = 10U,
                    },
                    {
                        .name = "Eve",
                        .age = 11U,
                    },
                    {
                        .name = "Frank",
                        .age = 12U,
                    },
                },                                  .npupils = 3U,
         },
        {
         .name = "C",
         .pupils =
                (struct pupil[]) {
                    {
                        .name = "Grace",
                        .age = 10U,
                    },
                    {
                        .name = "Heidi",
                        .age = 11U,
                    },
                    {
                        .name = "Ivan",
                        .age = 12U,
                    },
                },.npupils = 3U,
         },
    };

    void *dumped_bytes = NULL;

    struct dicey_message_builder msgbuild = { 0 };

    enum dicey_error err = dicey_message_builder_init(&msgbuild);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_begin(&msgbuild, DICEY_OP_SET);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&msgbuild, "/foo/bar/baz");
    if (err) {
        goto fail;
    }

    const struct dicey_selector selector = {
        .trait = "dc.Foo",
        .elem = "bar",
    };

    err = dicey_message_builder_set_selector(&msgbuild, selector);
    if (err) {
        goto fail;
    }

    struct dicey_value_builder valbuild = { 0 };

    err = dicey_message_builder_value_start(&msgbuild, &valbuild);
    if (err) {
        goto fail;
    }

    err = classes_dump(classes, sizeof classes / sizeof *classes, &valbuild);
    if (err) {
        goto fail;
    }

    err = dicey_message_builder_value_end(&msgbuild, &valbuild);
    if (err) {
        goto fail;
    }

    struct dicey_packet pkt = { 0 };

    err = dicey_message_builder_build(&msgbuild, &pkt);

    if (err) {
        goto fail;
    }

    const size_t nbytes = pkt.nbytes;
    dumped_bytes = calloc(1, nbytes);
    err = dicey_packet_dump(pkt, &(void *) { dumped_bytes }, &(size_t) { nbytes });
    if (err) {
        goto fail;
    }

    if (out.is_binary) {
        size_t written = 0;

        while (written < nbytes) {
            const size_t n = fwrite((char *) dumped_bytes + written, 1, nbytes - written, out.f);
            if (!n) {
                break;
            }

            written += n;
        }
    } else {
        struct util_dumper dumper = util_dumper_for(out.f);

        util_dumper_dump_hex(&dumper, dumped_bytes, nbytes);
    }

    fclose(out.f);
    free(dumped_bytes);
    dicey_packet_deinit(&pkt);

    return EXIT_SUCCESS;

fail:
    fclose(out.f);
    free(dumped_bytes);
    dicey_message_builder_discard(&msgbuild);

    fprintf(stderr, "error: %s\n", dicey_error_msg(err));

    return err;
}
