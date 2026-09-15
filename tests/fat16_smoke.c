#include "fat16.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE 512u
#define BLOCK_COUNT 5001u
#define VOLUME_START 1u

static uint8_t disk[BLOCK_COUNT * BLOCK_SIZE];

static int mem_read(orion_blockdev_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    (void)dev;
    memcpy(buffer, disk + (size_t)lba * BLOCK_SIZE, (size_t)count * BLOCK_SIZE);
    return 0;
}

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void make_valid_fat16_boot(uint8_t *boot) {
    memset(boot, 0, BLOCK_SIZE);
    put16(boot + 11, 512);
    boot[13] = 1;
    put16(boot + 14, 1);
    boot[16] = 2;
    put16(boot + 17, 512);
    put16(boot + 19, 5000);
    boot[21] = 0xf8;
    put16(boot + 22, 20);
    boot[510] = 0x55;
    boot[511] = 0xaa;
}

int main(void) {
    orion_blockdev_t dev = {"mem0", BLOCK_SIZE, BLOCK_COUNT, mem_read, 0, 0};
    orion_fat16_t fs;
    uint8_t root[BLOCK_SIZE];
    uint8_t *boot = disk + VOLUME_START * BLOCK_SIZE;

    memset(disk, 0, sizeof(disk));
    make_valid_fat16_boot(boot);
    memset(disk + 42u * BLOCK_SIZE, 0x5a, BLOCK_SIZE);

    assert(fat16_mount(&dev, VOLUME_START, &fs) == 0);
    assert(fs.total_sectors == 5000);
    assert(fs.bytes_per_sector == 512);
    assert(fs.sectors_per_cluster == 1);
    assert(fs.root_dir_sectors == 32);
    assert(fs.data_cluster_count == 4927);
    assert(fs.first_fat_lba == 2);
    assert(fs.first_root_lba == 42);
    assert(fs.first_data_lba == 74);
    assert(fat16_read_root_sector(&fs, 0, root) == 0);
    assert(root[0] == 0x5a && root[BLOCK_SIZE - 1] == 0x5a);
    assert(fat16_read_root_sector(&fs, fs.root_dir_sectors, root) != 0);

    put16(boot + 19, 0);
    put32(boot + 32, 5000);
    assert(fat16_mount(&dev, VOLUME_START, &fs) == 0);
    assert(fs.total_sectors == 5000);

    boot[511] = 0;
    assert(fat16_mount(&dev, VOLUME_START, &fs) != 0);
    boot[511] = 0xaa;

    put16(boot + 22, 0);
    assert(fat16_mount(&dev, VOLUME_START, &fs) != 0);
    put16(boot + 22, 20);

    put32(boot + 32, 4000);
    assert(fat16_mount(&dev, VOLUME_START, &fs) != 0);
    put32(boot + 32, 5000);

    dev.block_count = 5000;
    assert(fat16_mount(&dev, VOLUME_START, &fs) != 0);
    dev.block_count = BLOCK_COUNT;

    dev.block_size = 4096;
    assert(fat16_mount(&dev, VOLUME_START, &fs) != 0);
    return 0;
}
