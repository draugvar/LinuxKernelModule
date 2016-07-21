#define EXPORT_SYMTAB

/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/spinlock.h> /* spinlock */
#include <linux/wait.h> /* wait queue */
#include <linux/sched.h> /*some what */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <asm/atomic.h> /* atomic counter */

#include "m_ioctl.h"

static int mailbox_open(struct inode *, struct file *);
static int mailbox_release(struct inode *, struct file *);
static ssize_t mailbox_write(struct file *, const char *, size_t, loff_t *);
static ssize_t mailbox_read(struct file *, char *, size_t, loff_t *);
static long mailbox_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

#define DEVICE_NAME "mailbox"  /* Device file name in /dev/ - not mandatory  */
#define DEVICE_MAX_NUMBER 256
#define OVER_LIMIT 4096

/* Major number assigned to broadcast device driver */
static int Major;

static spinlock_t write_lock[DEVICE_MAX_NUMBER];
static spinlock_t read_lock[DEVICE_MAX_NUMBER];

static wait_queue_head_t read_queue[DEVICE_MAX_NUMBER];
static wait_queue_head_t write_queue[DEVICE_MAX_NUMBER];

static atomic_t max_messages[DEVICE_MAX_NUMBER];
static atomic_t max_size[DEVICE_MAX_NUMBER];
static atomic_t n_messages[DEVICE_MAX_NUMBER];

/* Buffer to store data */
typedef struct Mailslot{
	char *memory_buffer;
	int buffer_size;
	struct Mailslot *next;
} Mailslot;

static struct file_operations fops = {
	.read = mailbox_read,
	.write = mailbox_write,
	.open = mailbox_open,
	.release = mailbox_release,
	.unlocked_ioctl = mailbox_ioctl
};
