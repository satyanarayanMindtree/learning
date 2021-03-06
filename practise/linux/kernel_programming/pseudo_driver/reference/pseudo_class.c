/*
 * plp_kmem.c - Minimal example kernel module.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>

#define PLP_KMEM_BUFSIZE (1024*1024) /* 1MB internal buffer */

/* global variables */

int ndevices = 1;

static char *plp_kmem_buffer;

static struct class *pseudo_class;	/* pretend /sys/class */
static dev_t plp_kmem_dev;		/* dynamically assigned char device */
static struct cdev *plp_kmem_cdev;	/* dynamically allocated at runtime. */

/* function prototypes */

static int __init plp_kmem_init(void);
static void __exit plp_kmem_exit(void);

static int plp_kmem_open(struct inode *inode, struct file *file);
static int plp_kmem_release(struct inode *inode, struct file *file);
static ssize_t plp_kmem_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos);
static ssize_t plp_kmem_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos);

/* file_operations */
typedef struct priv_obj1{
	struct list_head list;
	struct cdev cdev;
}C_OBJ;
static struct file_operations plp_kmem_fops = {
	.read		= plp_kmem_read,
	.write		= plp_kmem_write,
	.open		= plp_kmem_open,
	.release	= plp_kmem_release,
	.owner		= THIS_MODULE,//this is a must for the system
                                      //ptr to this module's object/structure
};

LIST_HEAD(dev_list);

/*
 * plp_kmem_open: Open the kmem device
 */

static int plp_kmem_open(struct inode *inode, struct file *file)
{

  C_OBJ *obj;
  obj = container_of(inode->i_cdev,C_OBJ, cdev);

  file->private_data = obj;

  dump_stack(); 


//here the open is dummy - not always
#ifdef PLP_DEBUG
	printk(KERN_DEBUG "plp_kmem: opened device.\n");
#endif

	return 0;
}

/*
 * plp_kmem_release: Close the kmem device.
 */

static int plp_kmem_release(struct inode *inode, struct file *file)
{

//dummy here - not always 

#ifdef PLP_DEBUG
	printk(KERN_DEBUG "plp_kmem: device closed.\n");
#endif

	return 0;
}

/*
 * plp_kmem_read: Read from the device.
 */

static ssize_t plp_kmem_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	size_t bytes = count;
	loff_t fpos = *ppos;
	char *data;

       //fetching the current device object/context

        //struct pseudo_dev_obj *obj = file->private_data ; 

	if (fpos >= PLP_KMEM_BUFSIZE)
		return 0;
	
	if (fpos+bytes >= PLP_KMEM_BUFSIZE)
		bytes = PLP_KMEM_BUFSIZE-fpos;

	if (0 == (data = kmalloc(bytes, GFP_KERNEL)))
		return -ENOMEM;

#ifdef PLP_DEBUG
	printk(KERN_DEBUG "plp_kmem: read %d bytes from device, offset %d.\n",
		bytes,(int)fpos);
#endif

	memcpy(data,plp_kmem_buffer+fpos,bytes);
	
	if (copy_to_user((void __user *)buf, data, bytes)) {
		printk(KERN_ERR "plp_kmem: cannot write data.\n");
		kfree(data);
		return -EFAULT;
	}
	
	*ppos = fpos+bytes;

	kfree(data);
	return bytes;
}

/*
 * plp_kmem_write: Write to the device.
 */

static ssize_t plp_kmem_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
        

        //fetching the current device object/context

        struct C_DEV *obj = file->private_data ;  

        //write to the kfifo, as per the standard rules
        //and return appropriately

        //the special case of blocking

        //all operations are based on the private object 

        

	size_t bytes = count;
	loff_t fpos = *ppos;
	char *data;

	if (fpos >= PLP_KMEM_BUFSIZE)
		return -ENOSPC;

	if (fpos+bytes >= PLP_KMEM_BUFSIZE)
		bytes = PLP_KMEM_BUFSIZE-fpos;

	if (0 == (data = kmalloc(bytes, GFP_KERNEL)))
		return -ENOMEM;
        //this will copy data from user-space to system-space
        //this will also verify  that the user-space buffer is
        //really user-space buffer 
	if (copy_from_user((void *)data, (const void __user *)buf, bytes)) {
		printk(KERN_ERR "plp_kmem: cannot read data.\n");
		kfree(data);
		return -EFAULT;
	}

#ifdef PLP_DEBUG
	printk(KERN_DEBUG "plp_kmem: write %d bytes to device, offset %d.\n",
		bytes,(int)fpos);
#endif
        //user-space data was earlier copies into a system-space 
        //buffer
        //from system-space buffer data is copied to the 
        //pseudo device built using vmalloc() - meaning, a system
        //space buffer is treated as device and managed by this driver
	memcpy(plp_kmem_buffer+fpos,data,bytes);

	*ppos = fpos+bytes;

	kfree(data);
	return bytes;  //must return the no of bytes written to the device
}

/*
 * plp_kmem_init: Load the kernel module into memory
 */
C_OBJ *my_dev;

module_param(ndevices,int,S_IRUGO);

static int __init plp_kmem_init(void)
{
	int ret;

	my_dev = kmalloc(sizeof(C_OBJ),GFP_KERNEL);
	
	list_add_tail(&my_dev->list,&dev_list);
        //the first param. is the storage for the first device no
        //allocated dynamically

        //second param. is the minor no. associated with the first
        //device no. allocated dynamically

        //third param. is the no. of dynamic device ids. requested
        //last param. is a logical name 

        //if this system API succeeds, we will be allocated character
        //device ids in the range plp_kmem_dev - plp_kmem_dev+ndevices-1,
        //with minor no. of plp_kmem_dev set to 0 !!!
	if (alloc_chrdev_region(&plp_kmem_dev, 0, ndevices, "pseudo_driver"))
		goto error;

        //we are requesting for a system defined structure 
        //from a slab allocator in the KMA dedicated for
        //struct cdev 
        //you may ask the system to allocate or you may provide
        //the structure as a global data

	//if (0 == (plp_kmem_cdev = cdev_alloc()))
	//	goto error;

        cdev_init(&my_dev->cdev,&plp_kmem_fops);  
        my_dev->cdev.owner = THIS_MODULE;
	//we are dealing with certain special objects of the I/O subsystem 

        kobject_set_name(&(my_dev->cdev.kobj),"pseudo_dev0");
        //we are passing the file-operations supported by
        //our driver to the system - this is known as 
        //passing hooks - registering our driver's characteristics
        //with the system
	my_dev->cdev.ops = &plp_kmem_fops; /* file up fops */
	//ptr to cdev
        //first device id
        //no of devices to be managed by cdev
        if (cdev_add(&my_dev->cdev, plp_kmem_dev, 1)) {
		kobject_put(&(my_dev->cdev.kobj));
		unregister_chrdev_region(plp_kmem_dev, 1);
		kfree(my_dev);
		goto error;
	}

        //currently, do not include this section - we will do 
        //after understanding the LDD model and sysfs

	pseudo_class = class_create(THIS_MODULE, "pseudo_class");
	if (IS_ERR(pseudo_class)) {
		printk(KERN_ERR "plp_kmem: Error creating class.\n");
		cdev_del(plp_kmem_cdev);
		unregister_chrdev_region(plp_kmem_dev, 1);
                //ADD MORE ERROR HANDLING
		goto error;
	}
	device_create(pseudo_class, NULL, plp_kmem_dev, NULL, "pseudo_dev0");

	printk(KERN_INFO "plp_kmem: loaded.\n");

	return 0;

error:
	printk(KERN_ERR "plp_kmem: cannot register device.\n");

        //return appropriate negative error code
	return 1;
}

/*
 * plp_kmem_exit: Unload the kernel module from memory
 */


static void __exit plp_kmem_exit(void)
{

//      after LDD model and sysfs 
	//device_destroy(pseudo_class, plp_kmem_dev);
	//class_destroy(pseudo_class);

        //removing the registration of my driver
	cdev_del(&(my_dev->cdev));//remove the registration of the driver/device

        //freeing the logical resources - device nos.
	unregister_chrdev_region(plp_kmem_dev,1);

        //freeing the system-space buffer

        kfree(my_dev); 
	printk(KERN_INFO "plp_kmem: unloading.\n");
}

/* declare init/exit functions here */

module_init(plp_kmem_init);
module_exit(plp_kmem_exit);

/* define module meta data */

MODULE_DESCRIPTION("Demonstrate kernel memory allocation");

MODULE_ALIAS("memory_allocation");
MODULE_LICENSE("GPL");
MODULE_VERSION("0:1.0");
