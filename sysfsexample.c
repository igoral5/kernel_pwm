#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/atomic.h>


struct my_attr 
{
    struct attribute attr;
    atomic_t value;
};

static struct my_attr my_first = {
    .attr.name="pwm_on",
    .attr.mode = 0644,
};

static struct my_attr my_second = {
    .attr.name="pwm_full",
    .attr.mode = 0644,
};

static struct attribute * myattr[] = {
    &my_first.attr,
    &my_second.attr,
    NULL
};

static struct task_struct *mythread;

static ssize_t default_show(struct kobject *kobj, struct attribute *attr,
        char *buf)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
    int value = atomic_read(&a->value);
    printk(KERN_INFO "Reading attribute %s value %d\n", attr->name, value);
    return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t default_store(struct kobject *kobj, struct attribute *attr,
        const char *buf, size_t len)
{
    struct my_attr *a = container_of(attr, struct my_attr, attr);
    int value;
    sscanf(buf, "%d", &value);
    printk(KERN_INFO "New value attribute %s is %d\n", attr->name, value);
    atomic_set(&a->value, value);
    return strlen(buf);
}

static int pwm_fun(void *arg)
{
    int pwm_full;
    int pwm_on;
    bool on = false;
    printk(KERN_INFO "Thread started\n");
    while( !kthread_should_stop())
    {
        pwm_on = atomic_read(&my_first.value);
        pwm_full = atomic_read(&my_second.value);
        if (on)
        {
            printk(KERN_INFO "Level low\n");
            msleep(pwm_full - pwm_on);
        }
        else
        {
            printk(KERN_INFO "Level high\n");
            msleep(pwm_on);
        }
        on = !on;
    }
    printk(KERN_INFO "Thread stoped\n");
    return 0;
}

static struct sysfs_ops myops = {
    .show = default_show,
    .store = default_store,
};

static struct kobj_type mytype = {
    .sysfs_ops = &myops,
    .default_attrs = myattr,
};

static struct kobject *mykobj;

static int __init sysfsexample_module_init(void)
{
    int err = -1;
    atomic_set(&my_first.value, 1000);
    atomic_set(&my_second.value, 2000);
    mykobj = kzalloc(sizeof(*mykobj), GFP_KERNEL);
    if (mykobj) {
        kobject_init(mykobj, &mytype);
        if (kobject_add(mykobj, NULL, "%s", "sysfs_sample")) {
             err = -1;
             printk("Sysfs creation failed\n");
             kobject_put(mykobj);
             mykobj = NULL;
        }
        err = 0;
    }
    mythread = kthread_run(pwm_fun, NULL, "pwm_fun" );
    if (IS_ERR(mythread))
    {
        printk(KERN_ERR "Error create kernel thread\n");
        err = -1;
    }
    return err;
}

static void __exit sysfsexample_module_exit(void)
{
    kthread_stop(mythread);
    if (mykobj) {
        kobject_put(mykobj);
        kfree(mykobj);
    }
}

module_init(sysfsexample_module_init);
module_exit(sysfsexample_module_exit);
MODULE_LICENSE("GPL");