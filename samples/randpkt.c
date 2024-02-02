// Copyright (c) 2014-2024 Zuru Tech HK Limited, All rights reserved.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <dicey/dicey.h>

#include <util/dumper.h>
#include <util/packet-gen.h>

int main(void) {
    struct util_dumper dumper = util_dumper_for(stdout);

    uint8_t bytes[4096];

    const ptrdiff_t len = util_random_bytes(bytes, sizeof bytes);
    if (len < 0) {
        fprintf(stderr, "error: %td\n", len);
        return 1;
    }

    util_dumper_dump_hex(&dumper, bytes, len);

    return 0;
}

#if 0

namespace {
    template <typename T>
    concept trivial =
        std::is_trivially_copyable_v<T> && std::is_trivially_assignable_v<T&, T> && std::is_trivially_destructible_v<T>;

    template <trivial T>
    std::optional<T> take_as(std::span<const std::byte>& data) {
        if (std::size(data) < sizeof(T)) {
            return std::nullopt;
        }

        T value{};

        // maybe UB
        std::copy_n(std::data(data), sizeof value, reinterpret_cast<std::byte*>(&value));

        data = data.subspan(sizeof value);

        return value;
    }

    inline bool test(const dicey_error err) {
        assert(err == DICEY_OK);

        return true;
    }

    std::optional<dicey_packet> generate_random_bye(std::span<const std::byte> data) {
        const auto reason_seed { ::take_as<int>(data) };
        const auto seq { ::take_as<std::uint32_t>(data) };

        if (!reason_seed || !seq) {
            return std::nullopt;
        }

        // thank you C++ for still not having a way to get the max value of a contiguous enum
        const auto reason { static_cast<dicey_bye_reason>(*reason_seed % 3U)};

        dicey_packet ret {};
        return dicey_packet_bye(&ret, *seq, reason) == DICEY_OK ? std::make_optional(ret) : std::nullopt;
    }

    std::optional<dicey_packet> generate_random_hello(std::span<const std::byte> data) {
        const auto seq { ::take_as<std::uint32_t>(data) };
        const auto version { ::take_as<dicey_version>(data) };

        if (!seq || !version) {
            return std::nullopt;
        }

        dicey_packet ret {};
        return dicey_packet_hello(&ret, *seq, *version) == DICEY_OK ? std::make_optional(ret) : std::nullopt;
    }

    std::optional<dicey_packet> generate_random_message(std::span<const std::byte> data) {
        const auto op { ::take_as<std::uint8_t>(data) };
        const auto seq { ::take_as<std::uint32_t>(data) };

        const auto path { ::take_as<std::uint32_t>(data) };
        const auto selector { ::take_as<std::string>(data) };
        const auto value { ::take_as<std::string>(data) };

        if (!op || !seq || !path || !selector || !value) {
            return std::nullopt;
        }

        dicey_packet ret {};
        return dicey_packet_message(&ret, *seq, message->c_str()) == DICEY_OK ? std::make_optional(ret) : std::nullopt;
    }

    static std::optional<dicey_packet> generate_random_packet(std::span<const std::byte> data) {
        const auto first { ::take_as<std::uint8_t>(data) };
        if (!first) {
            return std::nullopt;
        }

        switch (*first) {
        case 0U:
            return generate_random_bye(data);
        }
    }
}

void FuzzPackets(const std::vector<std::byte> bytes) {
    if (const auto some_packet { ::generate_random_packet(bytes) }) {
        const auto packet { *some_packet };


    }
}

FUZZ_TEST(DiceyFuzzTest, FuzzPackets);

#endif
