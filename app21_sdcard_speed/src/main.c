#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/drivers/disk.h>
#include <ff.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(sdcard_speed, LOG_LEVEL_INF);

#define DISK_DRIVER_NAME   "SD"
#define MOUNT_POINT        "/SD:"
#define TEST_FILE_PATH     "/SD:/speed_test.bin"

/* 测试参数 */
#define CHUNK_SIZE         (32 * 1024)      /* 每次读写 32KB */
#define TOTAL_TEST_SIZE    (2 * 1024 * 1024) /* 总共测试 2MB */
#define CHUNK_COUNT        (TOTAL_TEST_SIZE / CHUNK_SIZE)

/* 计算速度 (KB/s) */
#define BYTES_TO_KBPS(bytes, ms) (((uint32_t)(bytes) * 1000) / ((uint32_t)(ms) * 1024))

/**
 * @brief SD 卡基本信息
 */
static void print_sd_card_info(void)
{
	uint32_t sector_count = 0;
	uint32_t sector_size = 0;
	uint32_t block_size = 0;
	int ret;

	ret = disk_access_ioctl(DISK_DRIVER_NAME,
				DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
	if (ret == 0) {
		disk_access_ioctl(DISK_DRIVER_NAME,
				 DISK_IOCTL_GET_SECTOR_SIZE, &sector_size);
		disk_access_ioctl(DISK_DRIVER_NAME,
				 DISK_IOCTL_GET_ERASE_BLOCK_SZ, &block_size);

		uint32_t total_mb = (uint32_t)(((uint64_t)sector_count * sector_size) / (1024 * 1024));

		printk("\n");
		printk("+-------------------------------------------+\n");
		printk("|         SD Card Information               |\n");
		printk("+-------------------------------------------+\n");
		printk("| Sector count    : %10u sectors      |\n", sector_count);
		printk("| Sector size     : %10u bytes        |\n", sector_size);
		printk("| Erase block size: %10u bytes        |\n", block_size);
		printk("| Total capacity  : %10u MB          |\n", total_mb);
		printk("+-------------------------------------------+\n");
	}
}

/**
 * @brief 文件系统统计信息
 */
static void print_fs_stats(void)
{
	struct fs_statvfs stat;
	int ret;

	ret = fs_statvfs(MOUNT_POINT, &stat);
	if (ret == 0) {
		uint32_t total_kb = (uint32_t)(stat.f_bsize * stat.f_blocks / 1024);
		uint32_t free_kb  = (uint32_t)(stat.f_bsize * stat.f_bfree / 1024);

		printk("\n");
		printk("+-------------------------------------------+\n");
		printk("|       File System Statistics              |\n");
		printk("+-------------------------------------------+\n");
		printk("| Block size      : %10lu bytes        |\n", stat.f_bsize);
		printk("| Total blocks    : %10lu              |\n", stat.f_blocks);
		printk("| Free blocks     : %10lu              |\n", stat.f_bfree);
		printk("| Total space     : %10u KB          |\n", total_kb);
		printk("| Free space      : %10u KB          |\n", free_kb);
		printk("+-------------------------------------------+\n");
	} else {
		LOG_ERR("fs_statvfs failed: %d", ret);
	}
}

/**
 * @brief 填充测试数据缓冲区（递增字节模式）
 */
static void fill_test_buffer(uint8_t *buf, uint32_t size, uint32_t seed)
{
	for (uint32_t i = 0; i < size; i++) {
		buf[i] = (uint8_t)((i + seed) & 0xFF);
	}
}

/**
 * @brief 验证读取数据一致性
 */
static bool verify_buffer(const uint8_t *buf, uint32_t size, uint32_t seed)
{
	for (uint32_t i = 0; i < size; i++) {
		if (buf[i] != (uint8_t)((i + seed) & 0xFF)) {
			LOG_ERR("Data mismatch at offset %u: exp=0x%02X got=0x%02X",
				i, (uint8_t)((i + seed) & 0xFF), buf[i]);
			return false;
		}
	}
	return true;
}

/**
 * @brief 测试写入速度
 */
static int test_write_speed(void)
{
	struct fs_file_t file;
	uint8_t *buf;
	int64_t start_ms, end_ms, elapsed_ms;
	uint32_t total_written = 0;
	int ret;

	buf = k_malloc(CHUNK_SIZE);
	if (!buf) {
		LOG_ERR("Failed to allocate %u bytes for write buffer", CHUNK_SIZE);
		return -ENOMEM;
	}

	fs_file_t_init(&file);
	ret = fs_open(&file, TEST_FILE_PATH, FS_O_WRITE | FS_O_CREATE);
	if (ret < 0) {
		LOG_ERR("Failed to create test file: %d", ret);
		k_free(buf);
		return ret;
	}

	printk("\n");
	printk("+-------------------------------------------+\n");
	printk("|         Write Speed Test                  |\n");
	printk("| Buffer size: %u bytes                     |\n", CHUNK_SIZE);
	printk("| Total data : %u bytes (%u MB)             |\n",
	       TOTAL_TEST_SIZE, TOTAL_TEST_SIZE / (1024 * 1024));
	printk("+-------------------------------------------+\n");

	start_ms = k_uptime_get();

	for (uint32_t i = 0; i < CHUNK_COUNT; i++) {
		fill_test_buffer(buf, CHUNK_SIZE, i);

		ssize_t written = fs_write(&file, buf, CHUNK_SIZE);
		if (written < 0) {
			LOG_ERR("Write failed at chunk %u: %zd", i, written);
			fs_close(&file);
			k_free(buf);
			return written;
		}
		total_written += written;
	}

	/* 确保数据刷写到卡 */
	fs_sync(&file);
	end_ms = k_uptime_get();
	elapsed_ms = end_ms - start_ms;

	fs_close(&file);

	if (elapsed_ms == 0) elapsed_ms = 1; /* 避免除零 */

	uint32_t speed_kbps = BYTES_TO_KBPS(total_written, elapsed_ms);
	uint32_t speed_mbps = speed_kbps / 1024;
	uint32_t speed_kbps_frac = speed_kbps % 1024;

	printk("\n");
	printk("  Write completed!\n");
	printk("  Total written : %u bytes\n", total_written);
	printk("  Elapsed time  : %lld ms\n", elapsed_ms);
	printk("  Write speed   : %u KB/s", speed_kbps);
	if (speed_mbps > 0) {
		printk(" (~%u.%02u MB/s)", speed_mbps,
		       (speed_kbps_frac * 100) / 1024);
	}
	printk("\n");

	k_free(buf);
	return 0;
}

/**
 * @brief 测试读取速度
 */
static int test_read_speed(void)
{
	struct fs_file_t file;
	uint8_t *buf;
	int64_t start_ms, end_ms, elapsed_ms;
	uint32_t total_read = 0;
	bool verify_ok = true;
	int ret;

	buf = k_malloc(CHUNK_SIZE);
	if (!buf) {
		LOG_ERR("Failed to allocate %u bytes for read buffer", CHUNK_SIZE);
		return -ENOMEM;
	}

	fs_file_t_init(&file);
	ret = fs_open(&file, TEST_FILE_PATH, FS_O_READ);
	if (ret < 0) {
		LOG_ERR("Failed to open test file for reading: %d", ret);
		k_free(buf);
		return ret;
	}

	printk("\n");
	printk("+-------------------------------------------+\n");
	printk("|          Read Speed Test                  |\n");
	printk("+-------------------------------------------+\n");

	start_ms = k_uptime_get();

	for (uint32_t i = 0; i < CHUNK_COUNT; i++) {
		ssize_t n = fs_read(&file, buf, CHUNK_SIZE);
		if (n < 0) {
			LOG_ERR("Read failed at chunk %u: %zd", i, n);
			verify_ok = false;
			break;
		}
		total_read += n;

		/* 验证数据 */
		if (!verify_buffer(buf, (uint32_t)n, i)) {
			verify_ok = false;
		}
	}

	end_ms = k_uptime_get();
	elapsed_ms = end_ms - start_ms;

	fs_close(&file);

	if (elapsed_ms == 0) elapsed_ms = 1;

	uint32_t speed_kbps = BYTES_TO_KBPS(total_read, elapsed_ms);
	uint32_t speed_mbps = speed_kbps / 1024;
	uint32_t speed_kbps_frac = speed_kbps % 1024;

	printk("\n");
	printk("  Read completed!\n");
	printk("  Total read    : %u bytes\n", total_read);
	printk("  Elapsed time  : %lld ms\n", elapsed_ms);
	printk("  Read speed    : %u KB/s", speed_kbps);
	if (speed_mbps > 0) {
		printk(" (~%u.%02u MB/s)", speed_mbps,
		       (speed_kbps_frac * 100) / 1024);
	}
	printk("\n");

	if (verify_ok) {
		printk("  Data integrity: PASSED\n");
	} else {
		printk("  Data integrity: FAILED!\n");
	}

	k_free(buf);
	return verify_ok ? 0 : -EIO;
}

/**
 * @brief 清理测试文件
 */
static void cleanup_test_file(void)
{
	int ret = fs_unlink(TEST_FILE_PATH);
	if (ret == 0) {
		LOG_INF("Test file cleaned up.");
	}
}

int main(void)
{
	int ret;

	printk("\n");
	printk("=============================================\n");
	printk("   STM32H743 SD Card Speed Test\n");
	printk("   Zephyr RTOS v4.4 | FAT32\n");
	printk("=============================================\n");

	/* ---- 步骤 1: 初始化 SD 卡 ---- */
	LOG_INF("Initializing SD card...");
	ret = disk_access_init(DISK_DRIVER_NAME);
	if (ret != 0) {
		LOG_ERR("SD card init failed: %d", ret);
		return -1;
	}
	LOG_INF("SD card initialized.");

	/* ---- 步骤 2: 挂载文件系统 ---- */
	LOG_INF("Mounting FAT filesystem...");
	static FATFS fat_fs;
	static struct fs_mount_t fat_mount = {
		.type = FS_FATFS,
		.mnt_point = MOUNT_POINT,
		.fs_data = &fat_fs,
	};
	ret = fs_mount(&fat_mount);
	if (ret < 0) {
		LOG_ERR("Mount failed: %d", ret);
		return -1;
	}

	/* ---- 步骤 3: 打印 SD 卡基本信息 ---- */
	print_sd_card_info();
	print_fs_stats();

	/* ---- 步骤 4: 写入速度测试 ---- */
	ret = test_write_speed();
	if (ret < 0) {
		LOG_ERR("Write speed test failed!");
		return -1;
	}

	/* ---- 步骤 5: 读取速度测试 ---- */
	ret = test_read_speed();
	if (ret < 0) {
		LOG_ERR("Read speed test failed!");
		return -1;
	}

	/* ---- 步骤 6: 清理 ---- */
	cleanup_test_file();

	printk("\n");
	printk("=============================================\n");
	printk("   Speed Test Completed!\n");
	printk("=============================================\n");

	return 0;
}
