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
static int scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

MODULE_AUTHOR("jgraham5");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */

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
 * initialize list head and mutex
 */
struct scull_node {
    int pid;
    int tgid;
    struct list_head list;
};

static DEFINE_MUTEX(mtx);
static LIST_HEAD(scull_list);



/*
 * linked list functions
 */

/*
 * adds a scull_node struct to the linked list
 */
int append_scull(int pid, int tgid) {
    struct scull_node* n = kmalloc(sizeof(struct scull_node), GFP_KERNEL);
    if (n == NULL) { // allocate space above, check for error
        printk(KERN_ERR "Error allocating space for new node: pid = %d, tgid = %d\n", pid, tgid);
        return -1;
    }

    n->pid = pid; // set fields
    n->tgid = tgid;

    // CRITICAL SECTION
    mutex_lock(&mtx);
    list_add_tail(&(n->list), &scull_list); // get address for list inside struct, add to main list at the tail (this is confusing as fuuuuuuuuuuuuuck)
    // list_add_tail adds the new node BEFORE the head you pass, so passing the initial scull_list is the same as adding to end of list 
    mutex_unlock(&mtx);
    // CRITICAL SECTION END
    return 0;
}


/*
 * returns a pointer to object with pid, tgid if it exists, null otherwise
 */
struct scull_node* is_member(int pid, int tgid) {
    // CRITICAL SECTION
    mutex_lock(&mtx);
    struct scull_node* i;
    list_for_each_entry(i, &scull_list, list) { // iterate through the main list (elements stored in i pointer)
        if (i->pid == pid && i->tgid == tgid) { // return i (pointer to node) if pid and tgid match
            mutex_unlock(&mtx);        
            // CRITICAL SECTION END (return #1)
            return i;
        }
    }
    mutex_unlock(&mtx);
    // CRITICAL SECTION END (return #2)
    return NULL;
}


/*
 * given a pointer to a node, removes that node from the linked list
 */
void remove_scull(struct scull_node* n) {
    // CRITICAL SECTION
    mutex_lock(&mtx);
    list_del(&(n->list)); // delete!
    kfree(n); // free from memory
    mutex_unlock(&mtx);
    // CRITICAL SECTION END
}











/*
 * The ioctl() implementation
 */

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	int retval = 0;

    struct task_info return_info; // initialize task_info struct
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {

	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		break;
        
	case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;

    case SCULL_IOCIQUANTUM: /* new info command */
        return_info.__state = current->__state; // state from current
        return_info.cpu = current->thread_info.cpu; // set cpu from the thread_info struct
        return_info.prio = current->prio; // rest of field are also from current...
        return_info.pid = current->pid;
        return_info.tgid = current->tgid;
        return_info.nvcsw = current->nvcsw;
        return_info.nivcsw = current->nivcsw;

        // add pid/tgid to linked list
        if (is_member(return_info.pid, return_info.tgid) == NULL) { // if not a duplicate, then add to the linked list!
            int res;
            if ((res = append_scull(return_info.pid, return_info.tgid)) != 0) { // error, message already printed, break
                return res;
            }
        }

        retval = __put_user(return_info.__state, &((struct task_info __user *)arg)->__state); // copy the struct back to user space
		retval = __put_user(return_info.cpu, &((struct task_info __user *)arg)->cpu);
		retval = __put_user(return_info.prio, &((struct task_info __user *)arg)->prio);
		retval = __put_user(return_info.pid, &((struct task_info __user *)arg)->pid);
		retval = __put_user(return_info.tgid, &((struct task_info __user *)arg)->tgid);
		retval = __put_user(return_info.nvcsw, &((struct task_info __user *)arg)->nvcsw);
		retval = __put_user(return_info.nivcsw, &((struct task_info __user *)arg)->nivcsw);
        break;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	.release =  scull_release,
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
    // kernel messages to print when module is unloaded
    struct scull_node* i;
    struct scull_node* temp; // for safe delete
    int count = 1;
    list_for_each_entry_safe(i, temp, &scull_list, list) {
        printk(KERN_INFO "Task %d: PID %d, TGID %d\n", count, i->pid, i->tgid);
        list_del(&(i->list)); // delete and free the node from memory
        kfree(i);
        count++;
    }



	dev_t devno = MKDEV(scull_major, scull_minor);

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

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
