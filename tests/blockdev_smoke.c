#include "blockdev.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint8_t disk[8 * 512];

static int mem_read(orion_blockdev_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    (void)dev;
    memcpy(buffer, disk + lba * 512u, (size_t)count * 512u);
    return 0;
}

static int mem_write(orion_blockdev_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    (void)dev;
    memcpy(disk + lba * 512u, buffer, (size_t)count * 512u);
    return 0;
}

int main(void) {
    orion_blockdev_t dev = {"mem0", 512, 8, mem_read, mem_write, 0};
    uint8_t in[512], out[512];
    memset(in, 0x5a, sizeof(in));
    memset(out, 0, sizeof(out));

    assert(blockdev_validate(&dev) == 0);
    assert(blockdev_write(&dev, 2, 1, in) == 0);
    assert(blockdev_read(&dev, 2, 1, out) == 0);
    assert(memcmp(in, out, sizeof(in)) == 0);
    assert(blockdev_read(&dev, 8, 1, out) != 0);
    assert(blockdev_read(&dev, 7, 2, out) != 0);
    assert(blockdev_read(&dev, 0, 0, out) != 0);

    dev.write = 0;
    assert(blockdev_validate(&dev) == 0);
    assert(blockdev_write(&dev, 0, 1, in) != 0);
    return 0;
}
