#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/atomic.h>
#include <linux/gpio.h>


#define PWM_FULL_INIT 2000
#define PWM_ON_INIT 1000
#define PWM0_GPIO 17
#define PWM1_GPIO 18
#define LOW 0
#define HIGH 1


struct my_attr 
{
    struct attribute attr;
    atomic_t value;
};

struct my_pwm
{
    struct my_attr *pwm_full;
    struct my_attr *pwm_on;
    unsigned gpio;
};

static struct my_attr pwm0_full = {
    .attr.name = "pwm0_full",
    .attr.mode = 0666,
};

static struct my_attr pwm0_on = {
    .attr.name = "pwm0_on",
    .attr.mode = 0666,
};

static struct my_attr pwm1_full = {
    .attr.name = "pwm1_full",
    .attr.mode = 0666,
};

static struct my_attr pwm1_on = {
    .attr.name = "pwm1_on",
    .attr.mode = 0666,
};

static struct my_pwm pwm0 = {
    .pwm_full = &pwm0_full,
    .pwm_on = &pwm0_on,
    .gpio = PWM0_GPIO,
};

static struct my_pwm pwm1 = {
    .pwm_full = &pwm1_full,
    .pwm_on = &pwm1_on,
    .gpio = PWM1_GPIO,
};


static struct attribute * myattr[] = {
    &pwm0_full.attr,
    &pwm0_on.attr,
    &pwm1_full.attr,
    &pwm1_on.attr,
    NULL
};

static struct task_struct *mythread0;
static struct task_struct *mythread1;

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
    return len;
}

static int pwm_fun(void *arg)
{
    struct my_pwm * pwm = (struct my_pwm *) arg;
    int pwm_full;
    int pwm_on;
    int err = 0;
    printk(KERN_INFO "Thread started\n");
    printk(KERN_INFO "Address pwm 0x%p\n", pwm);
    printk(KERN_INFO "Address pwm_full 0x%p\n",pwm->pwm_full);
    printk(KERN_INFO "Address pwm_on 0x%p\n",pwm->pwm_on);
    if ((err = gpio_request_one(pwm->gpio, GPIOF_OUT_INIT_HIGH, "soft_pwm")))
    {
        printk(KERN_ERR "Error in gpio_request_one\n");
        goto exit;
    }
    while(!kthread_should_stop())
    {
        pwm_full = atomic_read(&pwm->pwm_full->value);
        pwm_on = atomic_read(&pwm->pwm_on->value);
        gpio_set_value(pwm->gpio, LOW);
        udelay(pwm_on);
        gpio_set_value(pwm->gpio, HIGH);
        udelay(pwm_full - pwm_on);
    }
    gpio_free(pwm->gpio);
exit:
    printk(KERN_INFO "Thread stoped\n");
    return err;
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

static int __init soft_pwm_module_init(void)
{
    int err = 0;
    printk(KERN_INFO "Address mythread0 0x%p\n", mythread0);
    printk(KERN_INFO "Address mythread1 0x%p\n", mythread1);
    atomic_set(&pwm0.pwm_full->value, PWM_FULL_INIT);
    atomic_set(&pwm0.pwm_on->value, PWM_ON_INIT);
    atomic_set(&pwm1.pwm_full->value, PWM_FULL_INIT);
    atomic_set(&pwm1.pwm_on->value, PWM_ON_INIT);
    mykobj = kzalloc(sizeof(*mykobj), GFP_KERNEL);
    if (!mykobj)
    {
        printk(KERN_ERR "Error allocate memory\n");
        return -ENOMEM;
    }
    kobject_init(mykobj, &mytype);
    if ((err = kobject_add(mykobj, NULL, "%s", "soft_pwm"))) 
    {
        printk(KERN_ERR "Sysfs creation failed\n");
        goto free_kobj;
    }
    printk(KERN_INFO "Address pwm0 0x%p\n", &pwm0);
    printk(KERN_INFO "Address pwm0_full 0x%p\n", pwm0.pwm_full);
    printk(KERN_INFO "Address pwm0_on 0x%p\n", pwm0.pwm_on);
    mythread0 = kthread_run(pwm_fun, &pwm0, "pwm_fun0" );
    if ((err = IS_ERR(mythread0)))
    {
        printk(KERN_ERR "Error create kernel thread\n");
        goto free_kobj;
    }
    printk(KERN_INFO "Address pwm1 0x%p\n", &pwm1);
    printk(KERN_INFO "Address pwm1_full 0x%p\n", pwm1.pwm_full);
    printk(KERN_INFO "Address pwm1_on 0x%p\n", pwm1.pwm_on);
    mythread1 = kthread_run(pwm_fun, &pwm1, "pwm_fun1" );
    if ((err = IS_ERR(mythread1)))
    {
        printk(KERN_ERR "Error create kernel thread\n");
        goto stop_thread;
    }
    return 0;
stop_thread:
    if (mythread0 && !mythread0->state)
        kthread_stop(mythread0);
free_kobj:
    kobject_put(mykobj);
    kfree(mykobj);
    mykobj = NULL;
    return err;
}

static void __exit soft_pwm_module_exit(void)
{
    if (mythread0 && !mythread0->state)
        kthread_stop(mythread0);
    if (mythread1 && !mythread1->state)
        kthread_stop(mythread1);
    if (mykobj) 
    {
        kobject_put(mykobj);
        kfree(mykobj);
    }
}

module_init(soft_pwm_module_init);
module_exit(soft_pwm_module_exit);
MODULE_LICENSE("GPL");
