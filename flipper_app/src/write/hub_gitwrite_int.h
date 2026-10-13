#pragma once

/* Object writer: buffer sizes and the writer state.
 * Internal to hub_gitwrite.c and its parts; not for other modules. */

#define OBUF_SIZE 512
#define BLOCK_MAX 65535u

struct HubGitWriter {
    const HubGitWriteIo* io;
    void* file;
    HubSha1 sha;
    uint32_t adler_a, adler_b;
    uint64_t total; /* header + content bytes the deflate stream carries */
    uint64_t fed; /* of those, taken so far */
    uint32_t block_left;
    size_t size; /* content size given at begin */
    bool failed;
    uint16_t on; /* bytes in obuf */
    uint8_t obuf[OBUF_SIZE];
    char repo[HUB_GW_REPO_MAX + 1];
    char tmp[HUB_GW_REPO_MAX + 40];
    char fin[HUB_GW_REPO_MAX + 60];
};
