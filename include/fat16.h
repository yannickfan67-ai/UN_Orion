#ifndef ORION_FAT16_H
#define ORION_FAT16_H

#include <stdint.h>
#include "blockdev.h"

typedef struct {
    orion_blockdev_t *dev;
    uint64_t volume_start_lba;
    uint32_t total_sectors;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t sectors_per_fat;
    uint32_t root_dir_sectors;
    uint32_t data_cluster_count;
    uint64_t first_fat_lba;
    uint64_t first_root_lba;
    uint64_t first_data_lba;
} orion_fat16_t;

int fat16_mount(orion_blockdev_t *dev, uint64_t volume_start_lba, orion_fat16_t *out);
int fat16_read_root_sector(orion_fat16_t *fs, uint32_t sector_index, void *buffer);

#endif
