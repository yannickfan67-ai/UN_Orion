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
    orion_fat16_dirent_t found;
    uint8_t root[BLOCK_SIZE];
    uint8_t cluster_data[BLOCK_SIZE];
    uint16_t next_cluster;
    uint8_t *boot = disk + VOLUME_START * BLOCK_SIZE;
    uint8_t *fat = disk + 2u * BLOCK_SIZE;
    uint8_t *root_disk = disk + 42u * BLOCK_SIZE;
    uint8_t *cluster7_disk = disk + 79u * BLOCK_SIZE;
    static const uint8_t notes_name[11] = {'N','O','T','E','S',' ',' ',' ','T','X','T'};
    static const uint8_t missing_name[11] = {'M','I','S','S','I','N','G',' ','T','X','T'};

    memset(disk, 0, sizeof(disk));
    make_valid_fat16_boot(boot);
    memcpy(root_disk, "VOLUME     ", 11);
    root_disk[11] = 0x08;
    memset(root_disk + 32, 0xe5, 32);
    memcpy(root_disk + 64, "LONGNA~1TXT", 11);
    root_disk[64 + 11] = 0x0f;
    memcpy(root_disk + 96, notes_name, 11);
    root_disk[96 + 11] = 0x20;
    put16(root_disk + 96 + 26, 7);
    put32(root_disk + 96 + 28, 1234);
    put16(fat + 7u * 2u, 8);
    put16(fat + 8u * 2u, 0xffff);
    memcpy(cluster7_disk, "cluster-seven", 13);

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
    assert(root[0] == 'V');
    assert(fat16_read_root_sector(&fs, fs.root_dir_sectors, root) != 0);

    assert(fat16_find_root(&fs, notes_name, &found) == 0);
    assert(memcmp(found.name, notes_name, 11) == 0);
    assert(found.attributes == 0x20);
    assert(found.first_cluster == 7);
    assert(found.size == 1234);
    assert(fat16_find_root(&fs, missing_name, &found) == 1);
    assert(fat16_find_root(&fs, 0, &found) != 0);

    memset(cluster_data, 0, sizeof(cluster_data));
    assert(fat16_read_cluster(&fs, 7, cluster_data) == 0);
    assert(memcmp(cluster_data, "cluster-seven", 13) == 0);
    assert(fat16_read_cluster(&fs, 1, cluster_data) != 0);
    assert(fat16_read_cluster(&fs, (uint16_t)(fs.data_cluster_count + 2u), cluster_data) != 0);
    assert(fat16_next_cluster(&fs, 7, &next_cluster) == 0);
    assert(next_cluster == 8);
    assert(fat16_next_cluster(&fs, 8, &next_cluster) == 1);
    assert(next_cluster == 0);
    put16(fat + 7u * 2u, 0xfff7);
    assert(fat16_next_cluster(&fs, 7, &next_cluster) != 0);
    put16(fat + 7u * 2u, 8);

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
