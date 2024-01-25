#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dicey/dicey.h>

#include "dicey/builders.h"
#include "dicey/packet.h"
#include "util/dumper.h"

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

static enum dicey_error classroom_dump(const struct classroom *const classroom, struct dicey_value_builder *const tuple) {
    struct dicey_value_builder item = {0};

    enum dicey_error err = dicey_value_builder_tuple_start(tuple);
    if (err != DICEY_OK) {
        return err;
    }

    err = dicey_value_builder_next(tuple, &item);
    if (err != DICEY_OK) {
        return err;
    }

    err = dicey_value_builder_set(&item, (struct dicey_arg) { .type = DICEY_TYPE_STR, .str = classroom->name });
    if (err != DICEY_OK) {
        return err;
    }

    struct dicey_value_builder pupils = {0};
    err = dicey_value_builder_next(tuple, &pupils);
    if (err != DICEY_OK) {
        return err;
    }

    err = dicey_value_builder_array_start(&pupils, DICEY_TYPE_PAIR);
    if (err != DICEY_OK) {
        return err;
    }

    const struct pupil *const end = classroom->pupils + classroom->npupils;
    for (const struct pupil *pupil = classroom->pupils; pupil < end; ++pupil) {
        err = dicey_value_builder_next(&pupils, &item);
        if (err != DICEY_OK) {
            return err;
        }

        err = pupil_dump(pupil, &item);
        if (err != DICEY_OK) {
            return err;
        }
    }

    err = dicey_value_builder_array_end(&pupils);
    if (err != DICEY_OK) {
        return err;
    }

    return dicey_value_builder_tuple_end(tuple);
}

static enum dicey_error classes_dump(const struct classroom *const classes, const size_t nclasses, struct dicey_value_builder *const array) {
    enum dicey_error err = dicey_value_builder_array_start(array, DICEY_TYPE_TUPLE);
    if (err != DICEY_OK) {
        return err;
    }

    const struct classroom *const end = classes + nclasses;
    for (const struct classroom *classroom = classes; classroom < end; ++classroom) {
        struct dicey_value_builder item = {0};
        err = dicey_value_builder_next(array, &item);
        if (err != DICEY_OK) {
            return err;
        }

        err = classroom_dump(classroom, &item);
        if (err != DICEY_OK) {
            return err;
        }
    }

    return dicey_value_builder_array_end(array);
}

int main(const int argc, const char *const argv[]) {
    bool dump_binary = false;

    switch (argc) {
    case 1:
        break;

    case 2:
        if (strcmp(argv[1], "-t") == 0) {
            dump_binary = true;
            break;
        }

        // fallthrough    
    default:
        fprintf(stderr, "usage: %s [-t]\n", argv[0]);

        return EXIT_FAILURE;
    }

    const struct classroom classes[3] = {
        {
                .name = "A",
                .pupils = (struct pupil[]) {
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
                },
                .npupils = 3U,
            },
            {
                .name = "B",
                .pupils = (struct pupil[]) {
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
                },
                .npupils = 3U,
            },
            {
                .name = "C",
                .pupils = (struct pupil[]) {
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
                },
                .npupils = 3U,
            },
    };

    void *dumped_bytes = NULL;

    struct dicey_message_builder msgbuild = {0};

    enum dicey_error err = dicey_message_builder_init(&msgbuild);
    if (err != DICEY_OK) {
        goto fail;
    }

    err = dicey_message_builder_begin(&msgbuild, DICEY_MESSAGE_TYPE_SET);
    if (err != DICEY_OK) {
        goto fail;
    }

    err = dicey_message_builder_set_path(&msgbuild, "/foo/bar/baz");
    if (err != DICEY_OK) {
        goto fail;
    }

    const struct dicey_selector selector = {
        .trait = "dc.Foo",
        .elem = "bar",
    };

    err = dicey_message_builder_set_selector(&msgbuild, selector);
    if (err != DICEY_OK) {
        goto fail;
    }

    struct dicey_value_builder valbuild = {0};

    err = dicey_message_builder_value_start(&msgbuild, &valbuild);
    if (err != DICEY_OK) {
        goto fail;
    }

    err = classes_dump(classes, sizeof classes / sizeof *classes, &valbuild);
    if (err != DICEY_OK) {
        goto fail;
    }

    err = dicey_message_builder_value_end(&msgbuild, &valbuild);
    if (err != DICEY_OK) {
        goto fail;
    }

    struct dicey_packet pkt = {0};

    err = dicey_message_builder_build(&msgbuild, &pkt);

    if (err != DICEY_OK) {
        goto fail;
    }

    const size_t nbytes = pkt.nbytes;
    dumped_bytes = calloc(1, nbytes);
    err = dicey_packet_dump(pkt, &(void*) { dumped_bytes }, &(size_t) { nbytes });

    if (dump_binary) {
        size_t written = 0;

        while (written < nbytes) {
            const unsigned long n = fwrite((char*) dumped_bytes + written, 1, nbytes - written, stdout);
            if (!n) {
                break;
            }

            written += n;
        }
    } else  {
        struct util_dumper dumper = util_dumper_for(stdout);

        util_dumper_dump_hex(&dumper, dumped_bytes, nbytes);
    }

    dicey_packet_deinit(&pkt);

    err = dicey_packet_load(&pkt, &(const void*) { dumped_bytes }, &(size_t) { nbytes });
    if (err != DICEY_OK) {
        goto fail;
    }

    return EXIT_SUCCESS;

fail:
    free(dumped_bytes);
    dicey_message_builder_destroy(&msgbuild);

    fprintf(stderr, "error: %s\n", dicey_strerror(err));

    return err;
}
