// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "util/dumper.h"

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#define IS_PIPED() (!isatty(fileno(stdout)))
#else
#define IS_PIPED() false
#endif

struct pupil {
    const char *name;
    uint8_t     age;
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
    size_t        npupils;
};

static enum dicey_error classroom_dump(
    const struct classroom *const     classroom,
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
    const struct classroom *const     classes,
    const size_t                      nclasses,
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

int main(const int argc, const char *const argv[]) {
    bool dump_binary = IS_PIPED();

    switch (argc) {
    case 1:
        break;

    case 2:
        if (!strcmp(argv[1], "-t") || !strcmp(argv[1], "-b")) {
            dump_binary = argv[1][1] == 'b';
            break;
        }

        // fallthrough
    default:
        fprintf(stderr, "usage: %s [-bt]\n", argv[0]);

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

    if (dump_binary) {
        size_t written = 0;

        while (written < nbytes) {
            const size_t n = fwrite((char *) dumped_bytes + written, 1, nbytes - written, stdout);
            if (!n) {
                break;
            }

            written += n;
        }
    } else {
        struct util_dumper dumper = util_dumper_for(stdout);

        util_dumper_dump_hex(&dumper, dumped_bytes, nbytes);
    }

    free(dumped_bytes);
    dicey_packet_deinit(&pkt);

    return EXIT_SUCCESS;

fail:
    free(dumped_bytes);
    dicey_message_builder_destroy(&msgbuild);

    fprintf(stderr, "error: %s\n", dicey_error_msg(err));

    return err;
}
