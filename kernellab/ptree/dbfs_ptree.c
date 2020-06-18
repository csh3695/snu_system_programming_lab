#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>

#define MAXLEN 1000

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

char* output;
ssize_t output_size;

typedef struct node{
    char* info;
    struct list_head list;
} node;

struct list_head task_list;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
    pid_t input_pid;
    sscanf(user_buffer, "%u", &input_pid);

    /* Task of the input pid */
    if(!(curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID))) return -ESRCH;

    LIST_HEAD(task_list);

    /* Find parent process until pid==0 */
    while(curr->pid){
        node* nd = kmalloc(sizeof(struct node), GFP_KERNEL);
        nd->info = kcalloc(32, sizeof(char), GFP_KERNEL);
        output_size += sprintf((nd->info), "%s (%d)\n", curr->comm, curr->pid);
        list_add(&nd->list, &task_list);   
        curr = curr->parent;
    }

    /* Write info to output buffer */
    output = krealloc(output, output_size, GFP_KERNEL);
    memset(output, 0, output_size);

    struct list_head *ptr, *ptrn;
    struct node *nd;
    list_for_each_safe(ptr, ptrn, &task_list) {
        nd = list_entry(ptr, struct node, list);
        strcat(output, nd->info);
    }
    return output_size;
}

static ssize_t read_from_ptree(struct file *fp, 
                                char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{   
    /* copy output to user_buffer */
    ssize_t size = simple_read_from_buffer(user_buffer, length, position, output, output_size);
    output = krealloc(output, 0, GFP_KERNEL);
    return size;
}

static const struct file_operations dbfs_fops = {
    .write = write_pid_to_input,
    .read = read_from_ptree,
};


static int __init dbfs_module_init(void)
{
    /* Create ptree directory at /sys/kernel/debug */
    if (!(dir = debugfs_create_dir("ptree", NULL))) {
        printk("Cannot create ptree dir\n");
        return -1;
    }
    /* Create input file at /sys/kernel/debug/ptree */
    if (!(inputdir = debugfs_create_file("input", 700, dir, NULL, &dbfs_fops))) {
        printk("Cannot create input file\n");
        return -1;
    }
    /* Create ptree file at /sys/kernel/debug/ptree */
    if (!(ptreedir = debugfs_create_file("ptree", 700, dir, NULL, &dbfs_fops))) {
        printk("Cannot create ptree file\n");
        return -1;
    }

	printk("dbfs_ptree module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
