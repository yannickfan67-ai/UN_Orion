#include "blockdev.h"

static int blockdev_range_valid(const orion_blockdev_t *dev, uint64_t lba, uint32_t count) {
    if (!dev || count == 0) return 0;
    if (lba >= dev->block_count) return 0;
    return (uint64_t)count <= dev->block_count - lba;
}

int blockdev_validate(const orion_blockdev_t *dev) {
    if (!dev || !dev->name || dev->block_size == 0 || dev->block_count == 0 || !dev->read) return -1;
    return 0;
}

int blockdev_read(orion_blockdev_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    if (blockdev_validate(dev) != 0 || !buffer || !blockdev_range_valid(dev, lba, count)) return -1;
    return dev->read(dev, lba, count, buffer);
}

int blockdev_write(orion_blockdev_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    if (blockdev_validate(dev) != 0 || !dev->write || !buffer || !blockdev_range_valid(dev, lba, count)) return -1;
    return dev->write(dev, lba, count, buffer);
}
