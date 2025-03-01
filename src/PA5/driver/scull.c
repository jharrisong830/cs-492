/*
 * scull.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>

#include <linux/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */


/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_fifo_elemsz = SCULL_FIFO_ELEMSZ_DEFAULT; /* ELEMSZ */
static int scull_fifo_size   = SCULL_FIFO_SIZE_DEFAULT;   /* N      */


static char* msg_queue = NULL; // declare message queue and start/end pointers as global vars
static char* qstart = NULL;    // so they can be accessed by all functions
static char* qend = NULL;


static DEFINE_MUTEX(qmutex); // mutex for accessing the message queue

static DEFINE_SEMAPHORE(qfull); // semaphores for producers/consumers, initialized with module
static DEFINE_SEMAPHORE(qempty);


module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_fifo_size, int, S_IRUGO);
module_param(scull_fifo_elemsz, int, S_IRUGO);

MODULE_AUTHOR("jgraham5");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */


/**
 * prints the entire contents of the queue
 */
void print_queue(char* q) {
    printk(KERN_INFO "PRINTING QUEUE START\n---\n");

    for (unsigned int i = 0; i < scull_fifo_size; i++) {
        unsigned int curr_pos = i * (sizeof(unsigned int) + scull_fifo_elemsz);
        printk(KERN_INFO "index = %u\n", i);
        if (q + curr_pos == qstart) {
            printk(KERN_INFO "*start\n");
        }
        if (q + curr_pos == qend) {
            printk(KERN_INFO "*end\n");
        }

        unsigned int curr_len = *(unsigned int*)(q + curr_pos);
        printk(KERN_INFO "length = %u\n", curr_len);
        for (unsigned int j = 0; j < curr_len; j++) {
            printk(KERN_INFO "%c", *(q + curr_pos + sizeof(unsigned int) + j));
        }
        printk(KERN_INFO "\n---\n");
    }
    printk(KERN_INFO "PRINTING QUEUE END\n");
}




/*
 * Open and close
 */

static int scull_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;
}

/*
 * Read and Write
 */
static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    if (down_interruptible(&qfull) != 0) { // acquire perms to consume (blocks if queue is empty)
        printk(KERN_ERR "Error: process interrupted.\n");
        return -1;
    }
    if (mutex_lock_interruptible(&qmutex) != 0) { // interruptible versions for lock/down, returns with error if there's an interrupt
        printk(KERN_ERR "Error: process interrupted.\n");
        return -1;
    }
    // CRITICAL SECTION

    unsigned int curr_len = *(unsigned int*)(qstart); // for the current position in the buffer, cast to unsigned int* and dereference to get length
    if (count > curr_len) count = curr_len; // overwrite count with curr_len if count is greater than the length of the message (can't read more than we have)

    if (copy_to_user(buf, qstart + sizeof(unsigned int), count) != 0) { // copy from qstart, offset by the size of the length stored before the data
        printk(KERN_ERR "Error: failed to read message from the queue.\n");
        return -1;
    }

    char* next_start = qstart + sizeof(unsigned int) + scull_fifo_elemsz; // advance the start pointer (curr start + len + elemSize)
    if (next_start > (msg_queue + ((sizeof(unsigned int) + scull_fifo_elemsz) * scull_fifo_size))) {
        next_start = msg_queue; // wrap around if we have passed the space allocated for the queue
    }
    qstart = next_start;

    // print_queue(msg_queue);

    // CRITICAL SECTION END
    mutex_unlock(&qmutex);
    up(&qempty); // release perms for producer (a space has been freed!)


	printk(KERN_INFO "scull read\n");
    return count; // return total bytes read
}


static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    if (down_interruptible(&qempty) != 0) { // acquire perms to produce (blocks if queue is full and cant accept messages)
        printk(KERN_ERR "Error: process interrupted.\n");
        return -1;
    }
    if (mutex_lock_interruptible(&qmutex) != 0) { // interruptible versions for lock/down, returns with error if there's an interrupt
        printk(KERN_ERR "Error: process interrupted.\n");
        return -1;
    }

    // CRITICAL SECTION

    if (count > scull_fifo_elemsz) count = scull_fifo_elemsz; // overwrite count if it is greater than the total allowed elemSize, ignore all bytes beyond that
    *(unsigned int*)(qend) = count; // set the length in the queue

    if (copy_from_user(qend + sizeof(unsigned int), buf, count)) { // copy from buff to qend, offset by the size of the length stored before the data
        printk(KERN_ERR "Error: failed to write message to the queue.\n");
        return -1;
    }

    char* next_end = qend + sizeof(unsigned int) + scull_fifo_elemsz; // advance the end pointer (curr end + len + elemSize)
    if (next_end > (msg_queue + ((sizeof(unsigned int) + scull_fifo_elemsz) * scull_fifo_size))) { 
        next_end = msg_queue; // wrap around if we have passed the space allocated for the queue
    }
    qend = next_end;

    // print_queue(msg_queue);

    // CRITICAL SECTION END
    up(&qfull); // release perms for consumer (a space has been filled!)
    mutex_unlock(&qmutex);


	printk(KERN_INFO "scull write\n");
	return count; // return total bytes written
}

/*
 * The ioctl() implementation
 */
static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
	case SCULL_IOCGETELEMSZ:
		return scull_fifo_elemsz;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}

struct file_operations scull_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open 		= scull_open,
	.release	= scull_release,
	.read 		= scull_read,
	.write 		= scull_write,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);


    kfree(msg_queue); // free the queue from memory


	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}


    sema_init(&qfull, 0); // initialize full buffer with 0 (no spots taken up by a message at start)
    sema_init(&qempty, scull_fifo_size); // initialize empty with queue size (all spots free)

    msg_queue = kmalloc( // create a flat array, size = (sizeof(len) + elemSize) * queueSize
        (sizeof(unsigned int) + scull_fifo_elemsz) * scull_fifo_size, GFP_KERNEL); 
    if (msg_queue == NULL) {
        printk(KERN_ERR "Error: could not initialize queue buffer\n");
        return -1;
    }

    // printk(KERN_INFO "Total queue size allocated = %u\n", (sizeof(unsigned int) + scull_fifo_elemsz) * scull_fifo_size);

    qstart = msg_queue; // set start and end to be the start of the array (same value -> empty)
    qend = msg_queue;

    // print_queue(msg_queue);


	printk(KERN_INFO "scull: FIFO SIZE=%u, ELEMSZ=%u\n", scull_fifo_size, scull_fifo_elemsz);

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
