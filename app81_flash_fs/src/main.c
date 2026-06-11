#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(app81_flash_fs, LOG_LEVEL_INF);

#define MAX_PATH_LEN 255
#define STORAGE_PARTITION_ID 1

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage);

static struct fs_mount_t lfs_storage_mnt = {
	.type = FS_LITTLEFS,
	.fs_data = &storage,
	.storage_dev = (void *)STORAGE_PARTITION_ID,
	.mnt_point = "/lfs1",
};

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]", path, res);
		return res;
	}

	LOG_PRINTK("\nListing dir %s ...\n", path);
	for (;;) {
		res = fs_readdir(&dirp, &entry);

		if (res || entry.name[0] == 0) {
			if (res < 0) {
				LOG_ERR("Error reading dir [%d]", res);
			}
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			LOG_PRINTK("[DIR ] %s\n", entry.name);
		} else {
			LOG_PRINTK("[FILE] %s (size = %zu)\n",
				   entry.name, entry.size);
		}
	}

	fs_closedir(&dirp);
	return res;
}

static int write_file(const char *path, const char *data, size_t len)
{
	struct fs_file_t file;
	int rc, ret;

	fs_file_t_init(&file);

	rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (rc < 0) {
		LOG_ERR("FAIL: open %s: %d", path, rc);
		return rc;
	}

	rc = fs_write(&file, data, len);
	if (rc < 0) {
		LOG_ERR("FAIL: write %s: %d", path, rc);
		goto out;
	}

	LOG_PRINTK("Written %d bytes to %s\n", rc, path);

out:
	ret = fs_close(&file);
	return ret < 0 ? ret : rc;
}

static int read_file(const char *path, char *buf, size_t buf_size)
{
	struct fs_file_t file;
	int rc, ret;

	fs_file_t_init(&file);

	rc = fs_open(&file, path, FS_O_READ);
	if (rc < 0) {
		LOG_ERR("FAIL: open %s: %d", path, rc);
		return rc;
	}

	rc = fs_read(&file, buf, buf_size - 1);
	if (rc < 0) {
		LOG_ERR("FAIL: read %s: %d", path, rc);
		goto out;
	}

	buf[rc] = '\0';
	LOG_PRINTK("Read %d bytes from %s: \"%s\"\n", rc, path, buf);

out:
	ret = fs_close(&file);
	return ret < 0 ? ret : rc;
}

static int append_file(const char *path, const char *data, size_t len)
{
	struct fs_file_t file;
	int rc, ret;

	fs_file_t_init(&file);

	rc = fs_open(&file, path, FS_O_WRITE | FS_O_APPEND);
	if (rc < 0) {
		LOG_ERR("FAIL: open %s: %d", path, rc);
		return rc;
	}

	rc = fs_write(&file, data, len);
	if (rc < 0) {
		LOG_ERR("FAIL: write %s: %d", path, rc);
		goto out;
	}

	LOG_PRINTK("Appended %d bytes to %s\n", rc, path);

out:
	ret = fs_close(&file);
	return ret < 0 ? ret : rc;
}

int main(void)
{
	char buf[128];
	struct fs_statvfs sbuf;
	int rc;

	LOG_PRINTK("app81_flash_fs - LittleFS on internal flash (last 256KB)\n");
	LOG_PRINTK("Version: %s\n", APP_VERSION);

	const struct flash_area *pfa;
	rc = flash_area_open(STORAGE_PARTITION_ID, &pfa);
	if (rc == 0) {
		LOG_PRINTK("Storage partition at 0x%x, size %u bytes\n",
			   (unsigned int)pfa->fa_off, (unsigned int)pfa->fa_size);
		flash_area_close(pfa);
	}

	rc = fs_mount(&lfs_storage_mnt);
	if (rc < 0) {
		LOG_PRINTK("Mount failed: %d\n", rc);
		return rc;
	}
	LOG_PRINTK("Mounted at %s\n", lfs_storage_mnt.mnt_point);

	rc = fs_statvfs(lfs_storage_mnt.mnt_point, &sbuf);
	if (rc == 0) {
		LOG_PRINTK("bsize=%lu frsize=%lu blocks=%lu bfree=%lu\n",
			   sbuf.f_bsize, sbuf.f_frsize,
			   sbuf.f_blocks, sbuf.f_bfree);
	}

	lsdir(lfs_storage_mnt.mnt_point);

	char filepath[64];
	snprintf(filepath, sizeof(filepath), "%s/hello.txt", lfs_storage_mnt.mnt_point);

	rc = write_file(filepath, "Hello from STM32H743!\n", 22);
	if (rc < 0) {
		goto out;
	}

	rc = read_file(filepath, buf, sizeof(buf));
	if (rc < 0) {
		goto out;
	}

	rc = append_file(filepath, "LittleFS works!\n", 17);
	if (rc < 0) {
		goto out;
	}

	rc = read_file(filepath, buf, sizeof(buf));
	if (rc < 0) {
		goto out;
	}

	char filepath2[64];
	snprintf(filepath2, sizeof(filepath2), "%s/data.bin", lfs_storage_mnt.mnt_point);

	uint8_t bin_data[64];
	for (int i = 0; i < sizeof(bin_data); i++) {
		bin_data[i] = (uint8_t)i;
	}
	rc = write_file(filepath2, (const char *)bin_data, sizeof(bin_data));
	if (rc < 0) {
		goto out;
	}

	lsdir(lfs_storage_mnt.mnt_point);

out:
	rc = fs_unmount(&lfs_storage_mnt);
	LOG_PRINTK("Unmount: %d\n", rc);

	return 0;
}
