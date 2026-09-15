#include "fat16.h"

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int is_power_of_two_u8(uint8_t value) {
    return value != 0 && (value & (uint8_t)(value - 1u)) == 0;
}

static int name83_equal(const uint8_t a[11], const uint8_t b[11]) {
    uint32_t i;
    for (i = 0; i < 11; ++i) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static int fat16_cluster_valid(const orion_fat16_t *fs, uint16_t cluster) {
    return fs && cluster >= 2u && (uint32_t)(cluster - 2u) < fs->data_cluster_count;
}

int fat16_mount(orion_blockdev_t *dev, uint64_t volume_start_lba, orion_fat16_t *out) {
    uint8_t boot[512];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint16_t sectors_per_fat;
    uint32_t total_sectors_32;
    uint32_t total_sectors;
    uint32_t root_dir_sectors;
    uint64_t overhead_sectors;
    uint64_t data_sectors;
    uint32_t cluster_count;
    orion_fat16_t mounted;

    if (!out || blockdev_validate(dev) != 0 || dev->block_size != sizeof(boot)) return -1;
    if (blockdev_read(dev, volume_start_lba, 1, boot) != 0) return -1;
    if (boot[510] != 0x55 || boot[511] != 0xaa) return -1;

    bytes_per_sector = read_le16(boot + 11);
    sectors_per_cluster = boot[13];
    reserved_sectors = read_le16(boot + 14);
    fat_count = boot[16];
    root_entry_count = read_le16(boot + 17);
    total_sectors_16 = read_le16(boot + 19);
    sectors_per_fat = read_le16(boot + 22);
    total_sectors_32 = read_le32(boot + 32);
    total_sectors = total_sectors_16 ? total_sectors_16 : total_sectors_32;

    if (bytes_per_sector != dev->block_size) return -1;
    if (!is_power_of_two_u8(sectors_per_cluster) || sectors_per_cluster > 128) return -1;
    if (reserved_sectors == 0 || fat_count == 0 || root_entry_count == 0 || sectors_per_fat == 0 || total_sectors == 0) return -1;

    root_dir_sectors = ((uint32_t)root_entry_count * 32u + (uint32_t)bytes_per_sector - 1u) /
                       (uint32_t)bytes_per_sector;
    overhead_sectors = (uint64_t)reserved_sectors +
                       (uint64_t)fat_count * (uint64_t)sectors_per_fat +
                       (uint64_t)root_dir_sectors;
    if ((uint64_t)total_sectors <= overhead_sectors) return -1;

    data_sectors = (uint64_t)total_sectors - overhead_sectors;
    cluster_count = (uint32_t)(data_sectors / sectors_per_cluster);
    if (cluster_count < 4085u || cluster_count >= 65525u) return -1;

    if (volume_start_lba >= dev->block_count) return -1;
    if ((uint64_t)total_sectors > dev->block_count - volume_start_lba) return -1;

    mounted.dev = dev;
    mounted.volume_start_lba = volume_start_lba;
    mounted.total_sectors = total_sectors;
    mounted.bytes_per_sector = bytes_per_sector;
    mounted.sectors_per_cluster = sectors_per_cluster;
    mounted.reserved_sectors = reserved_sectors;
    mounted.fat_count = fat_count;
    mounted.root_entry_count = root_entry_count;
    mounted.sectors_per_fat = sectors_per_fat;
    mounted.root_dir_sectors = root_dir_sectors;
    mounted.data_cluster_count = cluster_count;
    mounted.first_fat_lba = volume_start_lba + reserved_sectors;
    mounted.first_root_lba = mounted.first_fat_lba + (uint64_t)fat_count * sectors_per_fat;
    mounted.first_data_lba = mounted.first_root_lba + root_dir_sectors;

    *out = mounted;
    return 0;
}

int fat16_read_root_sector(orion_fat16_t *fs, uint32_t sector_index, void *buffer) {
    if (!fs || !fs->dev || !buffer || sector_index >= fs->root_dir_sectors) return -1;
    return blockdev_read(fs->dev, fs->first_root_lba + sector_index, 1, buffer);
}

int fat16_find_root(orion_fat16_t *fs, const uint8_t name83[11], orion_fat16_dirent_t *out) {
    uint8_t sector[512];
    uint32_t entries_per_sector;
    uint32_t sector_index;
    uint32_t seen = 0;

    if (!fs || !fs->dev || !name83 || !out || fs->bytes_per_sector != sizeof(sector)) return -1;
    entries_per_sector = fs->bytes_per_sector / 32u;

    for (sector_index = 0; sector_index < fs->root_dir_sectors && seen < fs->root_entry_count; ++sector_index) {
        uint32_t entry_index;
        if (fat16_read_root_sector(fs, sector_index, sector) != 0) return -1;
        for (entry_index = 0; entry_index < entries_per_sector && seen < fs->root_entry_count; ++entry_index, ++seen) {
            const uint8_t *entry = sector + entry_index * 32u;
            uint8_t attributes = entry[11];
            if (entry[0] == 0x00) return 1;
            if (entry[0] == 0xe5 || attributes == 0x0f || (attributes & 0x08u) != 0) continue;
            if (!name83_equal(entry, name83)) continue;

            for (uint32_t i = 0; i < 11; ++i) out->name[i] = entry[i];
            out->attributes = attributes;
            out->first_cluster = read_le16(entry + 26);
            out->size = read_le32(entry + 28);
            return 0;
        }
    }
    return 1;
}

int fat16_read_cluster(orion_fat16_t *fs, uint16_t cluster, void *buffer) {
    uint64_t lba;
    if (!fs || !fs->dev || !buffer || !fat16_cluster_valid(fs, cluster)) return -1;
    lba = fs->first_data_lba + (uint64_t)(cluster - 2u) * fs->sectors_per_cluster;
    return blockdev_read(fs->dev, lba, fs->sectors_per_cluster, buffer);
}

int fat16_next_cluster(orion_fat16_t *fs, uint16_t cluster, uint16_t *next_cluster) {
    uint8_t sector[512];
    uint32_t fat_offset;
    uint32_t sector_index;
    uint32_t entry_offset;
    uint16_t next;

    if (!fs || !fs->dev || !next_cluster || fs->bytes_per_sector != sizeof(sector) || !fat16_cluster_valid(fs, cluster)) return -1;
    fat_offset = (uint32_t)cluster * 2u;
    sector_index = fat_offset / fs->bytes_per_sector;
    entry_offset = fat_offset % fs->bytes_per_sector;
    if (sector_index >= fs->sectors_per_fat || entry_offset + 1u >= fs->bytes_per_sector) return -1;
    if (blockdev_read(fs->dev, fs->first_fat_lba + sector_index, 1, sector) != 0) return -1;

    next = read_le16(sector + entry_offset);
    if (next >= 0xfff8u) {
        *next_cluster = 0;
        return 1;
    }
    if (next == 0xfff7u || !fat16_cluster_valid(fs, next)) return -1;
    *next_cluster = next;
    return 0;
}
