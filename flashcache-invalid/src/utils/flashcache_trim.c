#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mntent.h>
#include "flashcache_ioctl.h"

/* Flashcache helper functions */

static int
cachedev_get(char *filename)
{
	FILE *mounts;
	struct mntent *ent;
	int cache_fd;
	pid_t pid = getpid();
	struct stat st, st_mnt;

	/*
	 * The file may not exist yet. If it does not exist, statfs the
	 * directory instead. If a relative path without a '/' was specified,
	 * use the cwd.
	 */
	if (stat(filename, &st) < 0) {
		fprintf(stderr, "flashcache_trim_unlink: nonexistent file %s\n", filename);
		exit(1);
	}
	mounts = setmntent("/etc/mtab", "r");
	while ((ent = getmntent(mounts)) != NULL ) {
		stat(ent->mnt_dir, &st_mnt);
		if (memcmp(&st.st_dev, &st_mnt.st_dev, sizeof(dev_t)) == 0)
			break;
	}
	endmntent(mounts);
	if (ent == NULL)
		return -1;
	cache_fd = open(ent->mnt_fsname, O_RDONLY);
	if (cache_fd < 1)
		return -1;
	if (ioctl(cache_fd, FLASHCACHEADDNCPID, &pid) < 0) {
		close(cache_fd);
		return -1;
	} else
		(void)ioctl(cache_fd, FLASHCACHEDELNCPID, &pid);
	fprintf(stderr, "Found cache fd for fsname %s\n", ent->mnt_fsname);
	return cache_fd;
}

static int
cachedev_put(int *cache_fd)
{
	int retval = 0;

	if (*cache_fd != -1)
		retval = close(*cache_fd);
	*cache_fd = -1;
	return retval;
}


static int
cachedev_ioctl(int cache_fd, int request)
{
	pid_t pid = getpid();
	int retval = 0;

	if (cache_fd != -1)
		retval = ioctl(cache_fd, request, &pid);
	return retval;
}

main(int argc, char **argv)
{
	FILE *popen_fp;
	char buf[1024*1024];
	char cmd[1024];
	char *filename;
	int r;
	int i;
	FILE *proc_fp;
	int cachedev;

	if (argc != 2) {
		fprintf(stderr, "usage: flashcache_trim_unlink <filename>\n");
		exit(1);
	}
	filename = argv[1];
	cachedev = cachedev_get(filename);
	if (cachedev < 0)
		exit(1);
	strcpy(cmd, "xfs_bmap ");
	strcat(cmd, filename);
	popen_fp = popen(cmd, "r");
	if (popen_fp == NULL) {
		printf("popen failed %s\n", strerror(errno));
		exit(1);
	}
	fgets(buf, 1024*1024, popen_fp);
	while (fgets(buf, 1024*1024, popen_fp)) {
		struct flashcache_trim_block_range blk_range;
		char *s = buf;
		sector_t start_dbn, end_dbn;

		/* Parse output of xfs_bmap here */
		s = strrchr(s, ':');
		if (s == NULL)
			continue;
		s++;
		sscanf(s, "%lu", &start_dbn);
		s = strrchr(s, '.');
		if (s == NULL)
			continue;
		s++;
		sscanf(s, "%lu", &end_dbn);
		printf("start_dbn = %lu, end_dbn = %lu\n",
		       start_dbn, end_dbn);
		blk_range.start_dbn = start_dbn;
		blk_range.num_sectors = end_dbn - start_dbn + 1;
		printf("Trimming %s:%lu-%lu\n",
		       filename, blk_range.start_dbn,
		       blk_range.start_dbn + blk_range.num_sectors);
		(void)ioctl(cachedev, FLASHCACHE_TRIM_BLK_RANGE, &blk_range);
	}
	pclose(popen_fp);
	cachedev_put(&cachedev);
}
