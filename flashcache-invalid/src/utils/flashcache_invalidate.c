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
cachedev_put(int *cache_fd)
{
	int retval = 0;

	if (*cache_fd != -1)
		retval = close(*cache_fd);
	*cache_fd = -1;
	return retval;
}

main(int argc, char **argv)
{
	char *devname;
	char buf[100];
	sector_t start_blk, len;
	int cachedev;
	int cache_fd;
	int count;
	struct flashcache_trim_block_range blk_range;

	printf("argc=%d\n", argc);
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: flashcache_invalidate <devname> [filename | start_blk len]\n");
		exit(1);
	}
	devname = argv[1];
	cache_fd = open(devname, O_RDONLY);
	if (cache_fd < 1)
	{
		fprintf(stderr, "open cache device %s failed\n", devname);
		exit(1);
	}
	if (argc == 4)
	{
		sscanf(argv[2], "%lu", &start_blk);
		sscanf(argv[3], "%lu", &len);
		blk_range.start_dbn = start_blk<<3;
		blk_range.num_sectors = len<<3;
		printf("Invalidating %s range %lu-%lu\n",
		       devname, start_blk, start_blk+len-1);	
		(void)ioctl(cache_fd, FLASHCACHE_TRIM_BLK_RANGE, &blk_range);
	}
	else
	{
		FILE *fp;
		char *filename;

		filename = argv[2];
		fp = fopen(filename, "r");
		if( fp == NULL )
		{
			fprintf(stderr, "open file %s failed\n", filename);
			exit(1);
		}
		while( fgets(buf, 100, fp) )
		{
			struct flashcache_trim_block_range blk_range;
			
			printf("buf is %s\n", buf);
			count = sscanf(buf, "%lu %lu", &start_blk, &len);
			printf("count=%d\n", count);
			if (count==-1 || count==0) break;
			if (count==1) len=1;

			//printf("start_blk = %lu, len = %lu\n", start_blk, len);
			blk_range.start_dbn = start_blk<<3;
			blk_range.num_sectors = len<<3;
			printf("Invalidating %s range %lu-%lu\n",
			       devname, start_blk, start_blk+len-1);
			(void)ioctl(cache_fd, FLASHCACHE_TRIM_BLK_RANGE, &blk_range);
		}	
	}
	cachedev_put(&cache_fd);
}

