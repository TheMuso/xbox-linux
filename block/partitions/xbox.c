/*
 * fs/partitions/xbox.c
 * Xbox disk partition support.
 *
 * Copyright (C) 2002  John Scott Tillman <speedbump@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include "check.h"
#include "xbox.h"


/*
 * The native Xbox kernel makes use of an implicit partitioning
 * scheme. Partition locations and sizes on-disk are hard-coded.
 */
#define XBOX_CONFIG_START	0x00000000L
#define XBOX_CACHE1_START	0x00000400L
#define XBOX_CACHE2_START	0x00177400L
#define XBOX_CACHE3_START	0x002EE400L
#define XBOX_SYSTEM_START	0x00465400L
#define XBOX_DATA_START		0x0055F400L
#define XBOX_EXTEND_START	0x00EE8AB0L

#define XBOX_CONFIG_SIZE	(XBOX_CACHE1_START - XBOX_CONFIG_START)
#define XBOX_CACHE1_SIZE	(XBOX_CACHE2_START - XBOX_CACHE1_START)
#define XBOX_CACHE2_SIZE	(XBOX_CACHE3_START - XBOX_CACHE2_START)
#define XBOX_CACHE3_SIZE	(XBOX_SYSTEM_START - XBOX_CACHE3_START)
#define XBOX_SYSTEM_SIZE	(XBOX_DATA_START - XBOX_SYSTEM_START)
#define XBOX_DATA_SIZE		(XBOX_EXTEND_START - XBOX_DATA_START)

#define XBOX_MAGIC_SECT		3L
#define XBOX_XBPARTITION_SECT	0L

#define XBOX_XBPARTITION_MAGIC	"****PARTINFO****"
#define PE_PARTFLAGS_IN_USE	0x80000000

/* THe names of the system partitions we need to verify. */
#define XBOX_DATA_NAME		"XBOX SHELL      "
#define XBOX_SYSTEM_NAME	"XBOX DATA       "

struct xbox_partition_table_entry {
	unsigned char	name[16]; /* Not NULL-terminated */
	u32		flags;
	u32		lba_start;
	u32		lba_size;
	u32		reserved;
};

struct xbox_partition_table {
	unsigned char				magic[16];
	char					reserved[32];
	struct xbox_partition_table_entry	entries[14];
};

static bool xbox_check_magic(struct block_device *bdev, sector_t at_sect,
		char *magic)
{
	Sector sect;
	char *data;
	bool ret;

	data = read_dev_sector(bdev, at_sect, &sect);
	if (!data)
		return false;

	/* Ick! */
	ret = (*(u32*)data == *(u32*)magic) ? true : false;
	put_dev_sector(sect);

	return ret;
}

static inline bool xbox_drive_detect(struct block_device *bdev)
{
	/** 
	* "BRFR" is apparently the magic number in the config area
	* the others are just paranoid checks to assure the expected
	* "FATX" tags for the other xbox partitions
	*
	* the odds against a non-xbox drive having random data to match is
	* astronomical...but it's possible I guess...you should only include
	* this check if you actually *have* an xbox drive...since it has to
	* be detected first
	*
	* @see check.c
	*/
	return (xbox_check_magic(bdev, XBOX_MAGIC_SECT, "BRFR") &&
		xbox_check_magic(bdev, XBOX_SYSTEM_START, "FATX") &&
		xbox_check_magic(bdev, XBOX_DATA_START, "FATX")) ?
		true : false;
}

static void get_preferred_entry_data(struct block_device *bdev,
		struct xbox_partition_table_entry *entry, u32 start, u32 size,
		u32 *use_start, u32 *use_size)
{
	bool start_matches = false;
	bool size_matches = false;

	if (entry->lba_start == start) {
		start_matches = true;
		*use_start = entry->lba_start;
	}

	if (entry->lba_size == size) {
		size_matches = true;
		*use_size = entry->lba_size;
	}

	if (start_matches && size_matches)
		return;

	/*
	 * It would be nice if we didn't need to worry about a mismatch, however
	 * we cannot be sure that all XBox partition table utilities are written
	 * to ensure that the hard-coded partitions and their equivalent on-disk
	 * table entries match. As such, we check the start sector given in the
	 * partition table, and the start sector for the hard-coded partition,
	 * and trust the partition data for which ever one has a FATX signature
	 * present at the given on-disc location, since we have no reliable way
	 * to determine the correct size of the partition, particularly in the
	 * case of the data partition/E drive, which may have a partition after
	 * it, and that partition may not be a FATX partition.
	 */
	if (xbox_check_magic(bdev, (sector_t)entry->lba_start,
	    "FATX") && !xbox_check_magic(bdev, (sector_t)start,
	    "FATX")) {
		*use_start = entry->lba_start;
		*use_size = entry->lba_size;
	} else if (xbox_check_magic(bdev, (sector_t)start, "FATX") &&
	    !xbox_check_magic(bdev, (sector_t)entry->lba_start,
	    "FATX")) {
		*use_start = start;
		*use_size = size;
	} else {
		*use_start = 0;
		*use_size = 0;
	}

	return;
}

static void xbox_read_xbpartition_table(struct block_device *bdev,
		struct parsed_partitions *state, const char *data)
{
	int slot;
	struct xbox_partition_table *table;
	struct xbox_partition_table_entry *entry;
	u8 i;

	table = (struct xbox_partition_table*)data;

	slot = 50;
	for (i = 0; i < 14; i++) {
		u32 start = 0, size = 0;
		entry = &table->entries[i];

		if (entry->lba_start > 0 && entry->lba_size > 0 &&
		    entry->flags & PE_PARTFLAGS_IN_USE) {
			if (!strncmp(entry->name, XBOX_SYSTEM_NAME, 16)) {
				get_preferred_entry_data(bdev, entry,
					XBOX_SYSTEM_START, XBOX_SYSTEM_SIZE,
					&start, &size);
			}else if (!strncmp(entry->name, XBOX_DATA_NAME, 16)) {
				get_preferred_entry_data(bdev, entry,
					XBOX_DATA_START, XBOX_DATA_SIZE,
					&start, &size);
			} else {
					start = entry->lba_start;
					size = entry->lba_size;
			}

			if (start > 0 && size > 0)
				put_partition(state, slot++, start, size);
		}
	}
}

int xbox_partition(struct parsed_partitions *state, struct block_device *bdev)
{
	int slot;
	sector_t last, size;
	Sector sect;
	char *data;

	/* "BRFR" is apparently the magic in the config area. */
	if (!xbox_check_magic(bdev, XBOX_MAGIC_SECT, "BRFR"))
		return -ENODEV;

	data = read_dev_sector(bdev, XBOX_XBPARTITION_SECT, &sect);
	if (!data)
		return -ENODEV;

	if (!strncmp(data, XBOX_XBPARTITION_MAGIC, 16)) {
		xbox_read_xbpartition_table(bdev, state, data);
		put_dev_sector(sect);
		return 1;
	} else {
		put_dev_sector(sect);
	}

	slot = 50;
	put_partition(state, slot++, XBOX_DATA_START, XBOX_DATA_SIZE);
	put_partition(state, slot++, XBOX_SYSTEM_START, XBOX_SYSTEM_SIZE);
	put_partition(state, slot++, XBOX_CACHE1_START, XBOX_CACHE1_SIZE);
	put_partition(state, slot++, XBOX_CACHE2_START, XBOX_CACHE2_SIZE);
	put_partition(state, slot++, XBOX_CACHE3_START, XBOX_CACHE3_SIZE);

	/*
	 * Xbox HDDs come in two sizes - 8GB and 10GB. The native Xbox kernel
	 * will only acknowledge the first 8GB, regardless of actual disk
	 * size. For disks larger than 8GB, anything above that limit is made
	 * available as a seperate partition.
	 */
	last = get_capacity(bdev->bd_disk);
	if (last == XBOX_EXTEND_START)
		goto out;

	printk(" <");
	size = last - XBOX_EXTEND_START;
	put_partition(state, slot++, XBOX_EXTEND_START, size);

out:
	return 1;
}
