#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#define MAXLEN 1016

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

unsigned char kernel_buffer[MAXLEN];

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    int i;
    pid_t pid = 0;
    struct mm_struct *mm;
    unsigned long int va = 0;
    unsigned long int pa = 0;

    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    if(copy_from_user(kernel_buffer, user_buffer, length)) return -EFAULT;

    /* Read PID : 4B */ 
    for(i=0; i<4; i++) pid += ((long)kernel_buffer[i] << (i<<3));

    /* Read V.ADDR : 48b = 36b vpn + 12b p.offset */
    for(i=0; i<6; i++) va += (((long)kernel_buffer[8+i]) << (i<<3));

    /* Get task of given pid */
    if(!(task = pid_task(find_get_pid(pid), PIDTYPE_PID))) return -ESRCH;


    /* Page Walk
     * pgd -> p4d -> pud -> pmd -> pte -> ppn
     * if any ptr is invalid, return EINVAL
     */
    mm = task->mm;

    pgd = pgd_offset(mm, va);
    if(pgd_none(*pgd) || pgd_bad(*pgd)) return -EINVAL;

    p4d = p4d_offset(pgd, va);
    if(p4d_none(*p4d) || p4d_bad(*p4d)) return -EINVAL;

    pud = pud_offset(p4d, va);
    if(pud_none(*pud) || pud_bad(*pud)) return -EINVAL;

    pmd = pmd_offset(pud, va);
    if(pmd_none(*pmd) || pmd_bad(*pmd)) return -EINVAL;

    pte = pte_offset_kernel(pmd, va);
    if(pte_none(*pte) || !pte_present(*pte)) return -EINVAL;

    /* P.ADDR */
    pa = (unsigned long)pte_pfn(*pte);
    

    /* lower 12b is p.offset : copy
     * write P.ADDR from the 4th byte of output's space for <paddr>
     */
    for(i=16; i<MAXLEN; i++){
        if(i == 16){
            /* write the lowest 8b of p.offset */
            kernel_buffer[i] = (unsigned char)(va&0xff);
        }
        else if(i == 17){
            /* write highest 4b of p.offset */
            kernel_buffer[i] = (unsigned char)((va>>8)&0xf);
            /* write the lowest 4b of P.ADDR */
            kernel_buffer[i] += (unsigned char)(((pa)&0xf) << 4);
        }
        else{
            /* write the P.ADDR by 8b */
            kernel_buffer[i] = (unsigned char)((pa >> (((i-17)<<3) - 4))&0xff);
        }
    }

    return simple_read_from_buffer(user_buffer, length, position, kernel_buffer, MAXLEN);
}

static const struct file_operations dbfs_fops = {
    .read = read_output,
};

static int __init dbfs_module_init(void)
{
    /* Create paddr directory at /sys/kernel/debug */    
    if (!(dir = debugfs_create_dir("paddr", NULL))) {
        printk("Cannot create paddr dir\n");
        return -1;
    }
    /* Create output file at /sys/kernel/debug/paddr */
    if(!(output = debugfs_create_file("output", 700, dir, NULL, &dbfs_fops))){
        printk("Cannot create output file\n");
        return -1;
    }

	printk("dbfs_paddr module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir); 
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
