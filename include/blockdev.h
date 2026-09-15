#ifndef ORION_BLOCKDEV_H
#define ORION_BLOCKDEV_H

#include <stddef.h>
#include <stdint.h>

typedef struct orion_blockdev orion_blockdev_t;
typedef int (*orion_block_read_fn)(orion_blockdev_t *dev, uint64_t lba, uint32_t count, void *buffer);
typedef int (*orion_block_write_fn)(orion_blockdev_t *dev, uint64_t lba, uint32_t count, const void *buffer);

struct orion_blockdev {
    const char *name;
    uint32_t block_size;
    uint64_t block_count;
    orion_block_read_fn read;
    orion_block_write_fn write;
    void *context;
};

int blockdev_validate(const orion_blockdev_t *dev);
int blockdev_read(orion_blockdev_t *dev, uint64_t lba, uint32_t count, void *buffer);
int blockdev_write(orion_blockdev_t *dev, uint64_t lba, uint32_t count, const void *buffer);

#endif
