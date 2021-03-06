#include "scullBuffer.h"

/* Parameters which can be set at load time */
int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_size = SCULL_SIZE;/* number of scull Buffer items */

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_size, int, S_IRUGO);

MODULE_AUTHOR("Piyush");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_buffer scullBufferDevice;/* instance of the scullBuffer structure */

/* file ops struct indicating which method is called for which io operation */
struct file_operations scullBuffer_fops = {
  .owner =    THIS_MODULE,
  .read =     scullBuffer_read,
  .write =    scullBuffer_write,
  .open =     scullBuffer_open,
  .release =  scullBuffer_release,
};

/*
 * Method used to allocate resources and set things up when the module
 * is being loaded. 
 */
int scull_init_module(void)
{
  int result = 0;
  dev_t dev = 0;

  /* first check if someone has passed a major number */
  if (scull_major) {
    dev = MKDEV(scull_major, scull_minor);
    result = register_chrdev_region(dev, SCULL_NR_DEVS, "scullBuffer");
  } else {
    /* we need a dynamically allocated major number..*/
    result = alloc_chrdev_region(&dev, scull_minor, SCULL_NR_DEVS,
				 "scullBuffer");
    scull_major = MAJOR(dev);
  }
  if (result < 0) {
    printk(KERN_WARNING "scullBuffer: can't get major %d\n", scull_major);
    return result;
  }

  /* alloc space for the buffer (scull_size bytes) */
  scullBufferDevice.bufferPtr = kmalloc( scull_size*516 , GFP_KERNEL);
  if(scullBufferDevice.bufferPtr == NULL)
    {
      scull_cleanup_module();
      return -ENOMEM;
    }

  /* Init the count vars */
  scullBufferDevice.readerCnt = 0;
  scullBufferDevice.writerCnt = 0;
  scullBufferDevice.front = 0;
  scullBufferDevice.nextEmpty = 0;
  scullBufferDevice.size = 0;

  /* Initialize the semaphores*/
  sema_init(&scullBufferDevice.sem, 1);
  sema_init(&scullBufferDevice.empty, scull_size);
  sema_init(&scullBufferDevice.full, 0);

  /* Finally, set up the c dev. Now we can start accepting calls! */
  scull_setup_cdev(&scullBufferDevice);
  printk(KERN_DEBUG "scullBuffer: Done with init module ready for requests buffer size= %d\n",scull_size);
  return result;
}

/*
 * Set up the char_dev structure for this device.
 * inputs: dev: Pointer to the device struct which holds the cdev
 */
static void scull_setup_cdev(struct scull_buffer *dev)
{
  int err, devno = MKDEV(scull_major, scull_minor);

  cdev_init(&dev->cdev, &scullBuffer_fops);
  dev->cdev.owner = THIS_MODULE;
  dev->cdev.ops = &scullBuffer_fops;
  err = cdev_add (&dev->cdev, devno, 1);
  /* Fail gracefully if need be */
  if (err)
    printk(KERN_NOTICE "Error %d adding scullBuffer\n", err);
}

/*
 * Method used to cleanup the resources when the module is being unloaded
 * or when there was an initialization error
 */
void scull_cleanup_module(void)
{
  dev_t devno = MKDEV(scull_major, scull_minor);

  /* if buffer was allocated, get rid of it */
  if(scullBufferDevice.bufferPtr != NULL)
    kfree(scullBufferDevice.bufferPtr);

  /* Get rid of our char dev entries */
  cdev_del(&scullBufferDevice.cdev);

  /* cleanup_module is never called if registering failed */
  unregister_chrdev_region(devno, SCULL_NR_DEVS);

  printk(KERN_DEBUG "scullBuffer: Done with cleanup module \n");
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);

/*
 * Implementation of the open system call
 */
int scullBuffer_open(struct inode *inode, struct file *filp)
{
  struct scull_buffer *dev;
  /* get and store the container scull_buffer */
  dev = container_of(inode->i_cdev, struct scull_buffer, cdev);
  filp->private_data = dev;

  if (down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  if (filp->f_mode & FMODE_READ)
    dev->readerCnt++;
  if (filp->f_mode & FMODE_WRITE)
    dev->writerCnt++;

  up(&dev->sem);
  return 0;
}

/* 
 * Implementation of the close system call
 */
int scullBuffer_release(struct inode *inode, struct file *filp)
{
  struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;
  if (down_interruptible(&dev->sem) )
    return -ERESTARTSYS;

  if (filp->f_mode & FMODE_READ)
    dev->readerCnt--;
  if (filp->f_mode & FMODE_WRITE)
    dev->writerCnt--;

  up(&dev->sem);
  return 0;
}

/*
 * Implementation of the read system call
 */
ssize_t scullBuffer_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;
  int len = 0;

  /* Atomically check if empty */
  if (down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  /* Check if the buffer is empty */
  if(dev->front >= dev->nextEmpty && dev->writerCnt <= 0) {
    printk(KERN_DEBUG "Buffer is empty\n");
    up(&dev->sem);
    return 0;
  }

  int currFront;
  currFront = dev->front;

  /* Move the front of the buffer up */
  dev->front += 516;
  printk(KERN_DEBUG "scullBuffer: new front: %d\n", dev->front);

  up(&dev->sem);

  /* Get semaphore for reading form buffer */
  if (down_interruptible(&dev->full))
    return -ERESTARTSYS;

  printk(KERN_DEBUG "scullBuffer: read called count= %d\n", count);
  printk(KERN_DEBUG "scullBuffer: front = %d \n", currFront);


  /* Get mutex for accessing buffer */
  if (down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  /* Read the size of the block first */
  int res;
  res = kstrtoint(dev->bufferPtr + (currFront % (scull_size*516)), 10, &len);
  printk(KERN_DEBUG "scullBuffer: Item length is %d\n", len);
  if(res < 0) {
    up(&dev->sem);
    up(&dev->empty);
    return -EINVAL;
  }

  /* Read a max of len bytes */
  if(len > 512) len = 512;
  if(count > len) count = len;

  printk(KERN_DEBUG "scullBuffer: reading %d bytes\n", (int)count);
  /* copy data to user space buffer */
  if (copy_to_user(buf, dev->bufferPtr + (currFront % (scull_size*516)) + 4, count)) {
    up(&dev->sem);
    up(&dev->empty);
    return -EFAULT;
  }

  /* now we're done release the semaphore */
  up(&dev->sem);
  up(&dev->empty);

  return count;
}

/*
 * Implementation of the write system call
 */
ssize_t scullBuffer_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
  struct scull_buffer *dev = (struct scull_buffer *)filp->private_data;

  if (down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  /* Atomically check if the buffer is full */
  if((dev->nextEmpty - dev->front) >= (scull_size*516) && dev->readerCnt <= 0) {
    printk(KERN_DEBUG "scullBuffer: Buffer is full\n");
    up(&dev->sem);
    return 0;
  }

  char kbuf[count];

  // Copy from user can fail if buf is an invalid address (e.g. a segmentation fault).
  // In this case, we want to handle the error without altering the bounded buffer.
  // Further reading: https://stackoverflow.com/questions/23433936/return-value-of-copy-from-user
  if(copy_from_user(kbuf, buf, count)){
    printk(KERN_DEBUG "scullBuffer: write: copy_from_user failed\n");
    up(&dev->sem);
    return -1 * EFAULT;
  }

  int currEmpty;
  currEmpty = dev->nextEmpty;

  /* update the next empty position */
  dev->nextEmpty += 516;

  /* update the size of the device */
  dev->size += 516;

  up(&dev->sem);


  /* Get semaphore for writing to buffer */
  if(down_interruptible(&dev->empty))
    return -ERESTARTSYS;

  printk(KERN_DEBUG "scullBuffer: write called count= %d\n", (int)count);
  printk(KERN_DEBUG "scullBuffer: cur pos= %d, size= %d \n", (int)currEmpty, (int)dev->size);

  /* write a maximum of 512 bytes */
  if(count > 512) count = 512;

  printk(KERN_DEBUG "scullBuffer: writing %d bytes \n", (int)count);

  /* Acquire mutex for accessing buffer */
  if(down_interruptible(&dev->sem))
    return -ERESTARTSYS;

  /* Write the size of the item to the buffer */
  snprintf(dev->bufferPtr + (currEmpty % (scull_size*516)), 5, "%d", (int)count);

  /* write data to the buffer */
  memcpy(dev->bufferPtr + (currEmpty % (scull_size*516)) + 4, kbuf, count);

  up(&dev->sem);
  up(&dev->full);
  return count;
}
