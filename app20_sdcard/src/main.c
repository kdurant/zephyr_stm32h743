#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/drivers/disk.h>
#include <ff.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(app20_sdcard, LOG_LEVEL_INF);

/* SD 卡设备名称 */
#define DISK_DRIVER_NAME "SD"

/* 测试文件路径 */
#define TEST_FILE_PATH   "/SD:/test.txt"
#define TEST_DIR_PATH    "/SD:/test_dir"
#define TEST_FILE_IN_DIR "/SD:/test_dir/nested_file.txt"

/* 测试数据 */
#define TEST_DATA_SIZE 256
static const char test_write_data[] =
    "========================================\n"
    "  Zephyr SD Card Test - FAT32 File System\n"
    "========================================\n"
    "\n"
    "This is a test file written to SD card.\n"
    "The SD card is formatted as FAT32.\n"
    "\n"
    "Test Data:\n";

/**
 * @brief 打印文件系统统计信息
 */
static void print_fs_stats(void)
{
    struct fs_statvfs stat;
    int               ret;

    ret = fs_statvfs("/SD:", &stat);
    if(ret == 0)
    {
        LOG_INF("File System Statistics:");
        LOG_INF("  Block size: %lu bytes", stat.f_bsize);
        LOG_INF("  Total blocks: %lu", stat.f_blocks);
        LOG_INF("  Free blocks: %lu", stat.f_bfree);
        LOG_INF("  Total space: %lu KB", (stat.f_blocks * stat.f_bsize) / 1024);
        LOG_INF("  Free space: %lu KB", (stat.f_bfree * stat.f_bsize) / 1024);
    }
    else
    {
        LOG_ERR("Failed to get filesystem stats: %d", ret);
    }
}

/**
 * @brief 测试文件写入
 */
static int test_file_write(const char* path, const char* data, size_t len)
{
    struct fs_file_t file;
    ssize_t          ret;
    int              err;

    fs_file_t_init(&file);

    /* 打开文件（创建或覆盖） */
    err = fs_open(&file, path, FS_O_WRITE | FS_O_CREATE);
    if(err < 0)
    {
        LOG_ERR("Failed to open file for writing: %d", err);
        return err;
    }

    /* 写入数据 */
    ret = fs_write(&file, data, len);
    if(ret < 0)
    {
        LOG_ERR("Failed to write to file: %zd", ret);
        fs_close(&file);
        return ret;
    }

    LOG_INF("Successfully wrote %zd bytes to %s", ret, path);

    /* 关闭文件 */
    err = fs_close(&file);
    if(err < 0)
    {
        LOG_ERR("Failed to close file: %d", err);
        return err;
    }

    return 0;
}

/**
 * @brief 测试文件读取
 */
static int test_file_read(const char* path, char* buf, size_t buf_size)
{
    struct fs_file_t file;
    ssize_t          ret;
    int              err;

    fs_file_t_init(&file);

    /* 打开文件（只读） */
    err = fs_open(&file, path, FS_O_READ);
    if(err < 0)
    {
        LOG_ERR("Failed to open file for reading: %d", err);
        return err;
    }

    /* 读取数据 */
    ret = fs_read(&file, buf, buf_size - 1);
    if(ret < 0)
    {
        LOG_ERR("Failed to read from file: %zd", ret);
        fs_close(&file);
        return ret;
    }

    buf[ret] = '\0'; /* 添加字符串结束符 */
    LOG_INF("Successfully read %zd bytes from %s", ret, path);

    /* 关闭文件 */
    err = fs_close(&file);
    if(err < 0)
    {
        LOG_ERR("Failed to close file: %d", err);
        return err;
    }

    return ret;
}

/**
 * @brief 测试目录创建
 */
static int test_dir_create(const char* path)
{
    int ret;

    ret = fs_mkdir(path);
    if(ret < 0 && ret != -EEXIST)
    {
        LOG_ERR("Failed to create directory %s: %d", path, ret);
        return ret;
    }

    if(ret == 0)
    {
        LOG_INF("Directory created: %s", path);
    }
    else
    {
        LOG_INF("Directory already exists: %s", path);
    }

    return 0;
}

/**
 * @brief 列出目录内容
 */
static int test_dir_list(const char* path)
{
    struct fs_dir_t  dir;
    struct fs_dirent entry;
    int              ret;

    fs_dir_t_init(&dir);

    ret = fs_opendir(&dir, path);
    if(ret < 0)
    {
        LOG_ERR("Failed to open directory %s: %d", path, ret);
        return ret;
    }

    LOG_INF("Directory contents of %s:", path);

    while(1)
    {
        ret = fs_readdir(&dir, &entry);
        if(ret < 0)
        {
            LOG_ERR("Failed to read directory: %d", ret);
            break;
        }

        if(entry.name[0] == 0)
        {
            break; /* 没有更多条目 */
        }

        if(entry.type == FS_DIR_ENTRY_DIR)
        {
            LOG_INF("  [DIR]  %s", entry.name);
        }
        else
        {
            LOG_INF("  [FILE] %s (%zu bytes)", entry.name, entry.size);
        }
    }

    ret = fs_closedir(&dir);
    if(ret < 0)
    {
        LOG_ERR("Failed to close directory: %d", ret);
        return ret;
    }

    return 0;
}

/**
 * @brief 验证读写数据一致性
 */
static int verify_data_consistency(const char* write_data, const char* read_data, size_t len)
{
    if(memcmp(write_data, read_data, len) == 0)
    {
        LOG_INF("✓ Data verification PASSED!");
        return 0;
    }
    else
    {
        LOG_ERR("✗ Data verification FAILED!");

        /* 找出不一致的位置 */
        for(size_t i = 0; i < len; i++)
        {
            if(write_data[i] != read_data[i])
            {
                LOG_ERR("  Mismatch at offset %zu: write=0x%02X, read=0x%02X",
                        i, write_data[i], read_data[i]);
                break;
            }
        }
        return -1;
    }
}

int main(void)
{
    int      ret;
    char     read_buf[512];
    char     full_write_data[TEST_DATA_SIZE];
    uint32_t sector_count;
    uint32_t sector_size;

    printk("\n========================================\n");
    printk("  Starting app20_sdcard Test\n");
    printk("========================================\n\n");

    /* 步骤 1: 检查 SD 卡是否就绪 */
    LOG_INF("Step 1: Checking SD card status...");
    ret = disk_access_init(DISK_DRIVER_NAME);
    if(ret != 0)
    {
        LOG_ERR("SD card initialization failed: %d", ret);
        LOG_ERR("Please ensure SD card is inserted and properly formatted as FAT32");
        return -1;
    }
    LOG_INF("SD card initialized successfully");

    /* 获取磁盘信息 */
    ret = disk_access_ioctl(DISK_DRIVER_NAME, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
    if(ret == 0)
    {
        disk_access_ioctl(DISK_DRIVER_NAME, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size);
        LOG_INF("SD Card Info:");
        LOG_INF("  Sector count: %u", sector_count);
        LOG_INF("  Sector size: %u bytes", sector_size);
        LOG_INF("  Total size: %u KB", (sector_count / 1024) * sector_size);
    }

    /* 挂载 FAT 文件系统 */
    LOG_INF("Mounting FAT filesystem...");
    static FATFS fat_fs;
    static struct fs_mount_t fat_mount = {
        .type = FS_FATFS,
        .mnt_point = "/SD:",
        .fs_data = &fat_fs,
    };
    ret = fs_mount(&fat_mount);
    if(ret < 0)
    {
        LOG_ERR("Failed to mount FAT filesystem: %d", ret);
        return -1;
    }
    LOG_INF("FAT filesystem mounted at /SD:");

    /* 步骤 2: 打印文件系统统计信息 */
    LOG_INF("\nStep 2: Getting filesystem statistics...");
    print_fs_stats();

    /* 步骤 3: 准备测试数据 */
    LOG_INF("\nStep 3: Preparing test data...");
    snprintf(full_write_data, sizeof(full_write_data), "%s", test_write_data);

    /* 添加时间戳和序列号 */
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "Timestamp: %llu ms\nSequence: %u\n",
             k_uptime_get(), (unsigned int)k_uptime_get_32());
    strncat(full_write_data, timestamp, sizeof(full_write_data) - strlen(full_write_data) - 1);

    LOG_INF("Test data prepared (%zu bytes)", strlen(full_write_data));

    /* 步骤 4: 测试根目录文件写入 */
    LOG_INF("\nStep 4: Writing test file to root directory...");
    ret = test_file_write(TEST_FILE_PATH, full_write_data, strlen(full_write_data));
    if(ret < 0)
    {
        LOG_ERR("File write test failed");
        return ret;
    }

    /* 步骤 5: 测试文件读取 */
    LOG_INF("\nStep 5: Reading test file...");
    memset(read_buf, 0, sizeof(read_buf));
    ret = test_file_read(TEST_FILE_PATH, read_buf, sizeof(read_buf));
    if(ret < 0)
    {
        LOG_ERR("File read test failed");
        return ret;
    }

    /* 步骤 6: 验证数据一致性 */
    LOG_INF("\nStep 6: Verifying data consistency...");
    ret = verify_data_consistency(full_write_data, read_buf, strlen(full_write_data));
    if(ret < 0)
    {
        return ret;
    }

    /* 步骤 7: 测试目录创建 */
    LOG_INF("\nStep 7: Creating test directory...");
    ret = test_dir_create(TEST_DIR_PATH);
    if(ret < 0)
    {
        return ret;
    }

    /* 步骤 8: 在子目录中创建文件 */
    LOG_INF("\nStep 8: Writing file in subdirectory...");
    const char* nested_data = "This is a nested file in test_dir\n";
    ret                     = test_file_write(TEST_FILE_IN_DIR, nested_data, strlen(nested_data));
    if(ret < 0)
    {
        return ret;
    }

    /* 步骤 9: 列出根目录内容 */
    LOG_INF("\nStep 9: Listing root directory...");
    test_dir_list("/SD:");

    /* 步骤 10: 列出子目录内容 */
    LOG_INF("\nStep 10: Listing subdirectory...");
    test_dir_list(TEST_DIR_PATH);

    /* 步骤 11: 再次打印文件系统统计 */
    LOG_INF("\nStep 11: Final filesystem statistics...");
    print_fs_stats();

    /* 打印读取的文件内容 */
    LOG_INF("\n========================================");
    LOG_INF("  File Content Preview:");
    LOG_INF("========================================");
    printk("%s\n", read_buf);

    printk("\n========================================\n");
    printk("  SD Card Test Completed Successfully!\n");
    printk("========================================\n");

    return 0;
}
