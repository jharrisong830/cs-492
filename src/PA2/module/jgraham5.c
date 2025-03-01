#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/current.h>

static int hello_init(void) {
    printk(KERN_ALERT "Hello World from John Graham jgraham5\n");
    return 0;
}

static void hello_exit(void) {
    printk(KERN_INFO "PID is %i and program name is %s\n", current->pid, current->comm);
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("Dual BSD/GPL");

