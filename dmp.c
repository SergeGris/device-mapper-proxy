/*
 * Device mapper proxy kernel module implementation.
 */

#include <linux/average.h>
#include <linux/bio.h>
#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>

DECLARE_EWMA(avg_size, 4, 4);

static ssize_t statistics_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);

/*
 * Structure for controlling statistics for every device.
 */
static struct {
	struct kobject *obj;
	struct kobj_attribute attr;
	unsigned long read_req;
	unsigned long write_req;
	unsigned long total_req;
	struct ewma_avg_size read_avg_blocksize;
	struct ewma_avg_size write_avg_blocksize;
	struct ewma_avg_size total_avg_blocksize;
} statistics = { .attr = __ATTR(volumes, S_IRUGO, statistics_show, NULL) };

static struct target_type dmp;

static ssize_t statistics_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return snprintf(buf,
			PAGE_SIZE,
			"read:\n"
			" reqs: %lu\n"
			" avg size: %lu\n"
			"write:\n"
			" reqs: %lu\n"
			" avg size: %lu\n"
			"total:\n"
			" reqs: %lu\n"
			" avg size: %lu\n",
			statistics.read_req,  (unsigned long)ewma_avg_size_read(&statistics.read_avg_blocksize),
			statistics.write_req, (unsigned long)ewma_avg_size_read(&statistics.write_avg_blocksize),
			statistics.total_req, (unsigned long)ewma_avg_size_read(&statistics.total_avg_blocksize));
}

/*
 * This is initializer function for the kernel module. Called on insmod.
 */
static int dmp_init(void) {
	int result;

	statistics.read_req = 0;
	statistics.write_req = 0;
	statistics.total_req = 0;
	ewma_avg_size_init(&statistics.read_avg_blocksize);
	ewma_avg_size_init(&statistics.write_avg_blocksize);
	ewma_avg_size_init(&statistics.total_avg_blocksize);

	result = dm_register_target(&dmp);

	if (result < 0) {
		printk(KERN_CRIT "\nFailed to register device mapper proxy\n");
		return result;
	}

	statistics.obj = kobject_create_and_add("stat", &THIS_MODULE->mkobj.kobj);

	if (statistics.obj == NULL) {
		printk(KERN_CRIT "\nFailed to create and add object\n");
		dm_unregister_target(&dmp);
		return -ENOMEM;
	}

	result = sysfs_create_file(statistics.obj, &statistics.attr.attr);

	if (result < 0) {
		printk(KERN_CRIT "\nFailed to create sysfs entry\n");
		kobject_put(statistics.obj);
		dm_unregister_target(&dmp);
		return result;
	}

	return 0;
}

/*
 * This is exit function for the kernel module. Called on rmmod or when kernel shutting down.
 */
static void dmp_exit(void) {
	dm_unregister_target(&dmp);
	kobject_put(statistics.obj);
}

/* This is constructor of DMP
 * Constructor gets called when we create some device of type 'DMP'.
 * So it will get called when we execute command 'dmsetup create'
 * This function gets called for each device over which you want to create DMP.
 */
static int dmp_ctr(struct dm_target *ti, unsigned argc, char **argv) {
	struct dm_dev *mpd;
	int result;

	if (argc != 1) {
		printk(KERN_CRIT "\nInvalid count of arguments\n");
		ti->error = "Invalid count of arguments";
		return -EINVAL;
	}

	mpd = kmalloc(sizeof(*mpd), GFP_KERNEL);

	if (mpd == NULL) {
		printk(KERN_CRIT "\nFailed to allocate memory\n");
		ti->error = "Failed to allocate memory";
		return -ENOMEM;
	}

	result = dm_get_device(ti, argv[0], dm_table_get_mode(ti->table), &mpd);

	if (result < 0) {
		printk(KERN_CRIT "\nFailed device lookup\n");
		ti->error = "Failed device lookup";
		kfree(mpd);
		return result;
	}

	ti->private = mpd;
	return 0;
}

/*
 * This is destruction function
 * This gets called when we remove a device of type DMP. The function gets
 * called per device.
 */
static void dmp_dtr(struct dm_target *ti) {
	struct dm_dev *dmpd = ti->private;

	dm_put_device(ti, dmpd);
	kfree(dmpd);
}

/* This is map function of DMP. This function gets called whenever you
 * get a new bio request. The working of map function is to map a particular bio
 * request to the underlying device. The request that we receive is submitted to
 * out device so  bio->bi_bdev points to our device. We should point to the
 * bio-> bi_dev field to bdev of underlying device.
 *
 * Parameters:
 *  ti:  It is the dm_target structure representing our DMP.
 *  bio: The block I/O request from upper layer.
 *
 * Return values from target map function:
 * DM_MAPIO_SUBMITTED: Written to statistics and passed through to underlying device and written to statistics.
 * DM_MAPIO_KILL:      Discard mapping operation and ignored (do not change statistics).
 */
static int dmp_map(struct dm_target *ti, struct bio *bio) {
	struct dm_dev *mpd;

	switch (bio_op(bio)) {
	case REQ_OP_READ:
		pr_debug("bio read request");
		statistics.read_req++;
		ewma_avg_size_add(&statistics.read_avg_blocksize, bio->bi_iter.bi_size);
		break;
	case REQ_OP_WRITE:
		pr_debug("bio write request");
		statistics.write_req++;
		ewma_avg_size_add(&statistics.write_avg_blocksize, bio->bi_iter.bi_size);
		break;
	default:
		pr_debug("invalid operation %d", bio_op(bio));
		return DM_MAPIO_KILL;
	}

	statistics.total_req++;
	ewma_avg_size_add(&statistics.total_avg_blocksize, bio->bi_iter.bi_size);
	mpd = ti->private;
	bio_set_dev(bio, mpd->bdev);
	submit_bio(bio);
	return DM_MAPIO_SUBMITTED;
}

/*
 * This structure is fops for DMP.
 */
static struct target_type dmp = {
	.name = "dmp",
	.version = { 1, 0, 0 },
	.module = THIS_MODULE,
	.ctr = dmp_ctr,
	.dtr = dmp_dtr,
	.map = dmp_map,
};

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_AUTHOR("Sergey Sushilin");
MODULE_DESCRIPTION("Device mapper proxy");
MODULE_LICENSE("GPL");
