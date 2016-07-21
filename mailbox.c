#include "mailbox.h"

Mailslot *mailslot[DEVICE_MAX_NUMBER];
Mailslot *actual_ms[DEVICE_MAX_NUMBER];


static int mailbox_open(struct inode *inode, struct file *filp)
{
	try_module_get(THIS_MODULE);
	int minor = iminor(filp->f_path.dentry->d_inode);
	if( minor < DEVICE_MAX_NUMBER){
		return 0; /* success */
	} 
	else 
	{
		return -EBUSY;
	}
}

static int mailbox_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	return 0; /* success */
}

static ssize_t mailbox_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	if (*f_pos == 0) 
	{
		int minor = iminor(filp->f_path.dentry->d_inode);
		int res = 0;
		Mailslot *tmp_ms;

		spin_lock(&(read_lock[minor]));
		while(atomic_read(&(n_messages[minor])) == 0)
		{
			spin_unlock(&(read_lock[minor]));
			if (filp->f_flags & O_NONBLOCK)
	            return -EAGAIN;
	        if (wait_event_interruptible(read_queue[minor], atomic_read(&(n_messages[minor])) != 0))
	            return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
	        /* otherwise loop, but first reacquire the lock */
	        spin_lock(&(read_lock[minor]));
		}
		// ok data is there
		tmp_ms = mailslot[minor];
		if(count < tmp_ms->buffer_size)
		{
			spin_unlock(&(read_lock[minor]));
			return -(tmp_ms->buffer_size); // return -(size of buffer)
		}
		if(mailslot[minor]->next != NULL)
		{
			mailslot[minor] = mailslot[minor]->next;
		} 
		else 
		{
			mailslot[minor] = NULL;
		}
		spin_unlock(&(read_lock[minor]));
		res = copy_to_user(buf,tmp_ms->memory_buffer,tmp_ms->buffer_size);
		if(atomic_read(&(n_messages[minor])) == atomic_read(&(max_messages[minor])))
		{
			atomic_dec(&(n_messages[minor]));
			wake_up_interruptible(&(write_queue[minor]));
		}
		else 
		{
			atomic_dec(&(n_messages[minor])); 
		}
		kfree(tmp_ms->memory_buffer);
		kfree(tmp_ms);
		*f_pos+=count;
		return count;
	}
	/* if (*f_pos == 0) {	
		int minor = iminor(filp->f_path.dentry->d_inode);
		int res = 0;
		Mailslot *tmp_ms;
		
		// Transfering data to user space 
		read_again:
		spin_lock(&(read_lock[minor]));
		if(!atomic_read(&(is_empty[minor]))) // mailslot[minor] != NULL
		{
			tmp_ms = mailslot[minor];
			if(count < tmp_ms->buffer_size)
			{
				spin_unlock(&(read_lock[minor]));
				return -(tmp_ms->buffer_size); // return -(size of buffer)
			}
			if(mailslot[minor]->next != NULL){
				mailslot[minor] = mailslot[minor]->next;
			} 
			else 
			{
				mailslot[minor] = NULL;
				atomic_set(&(is_empty[minor]), 1);
			}
			spin_unlock(&(read_lock[minor]));
			res = copy_to_user(buf,tmp_ms->memory_buffer,tmp_ms->buffer_size);
			kfree(tmp_ms->memory_buffer);
			kfree(tmp_ms);
			//Changing reading position as best suits 
 			*f_pos+=count;
	 		return count;
		} 
		else if(!(filp->f_flags & O_NONBLOCK)) // atomic_read(&(blocking_flag[minor]))
		{
			spin_unlock(&(read_lock[minor]));
			wait_event_interruptible(read_queue[minor], mailslot[minor] != NULL);
			goto read_again;
		} 
		else 
		{
			spin_unlock(&(read_lock[minor]));
			// Changing reading position as best suits 
 			*f_pos+=count;
	 		return count;
		}
 	} */
 	else 
 	{
	 	return 0;
 	} 
}

static ssize_t mailbox_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	int res;
	Mailslot *tmp_ms;

	// checks if write overflow the buffer
  	if( count - 1 > atomic_read(&(max_size[minor])) )
  	{	
  		return -1;
  	}

  	spin_lock(&(write_lock[minor]));
  	while(atomic_read(&(n_messages[minor])) == atomic_read(&(max_messages[minor])))
  	{
  		spin_unlock(&(write_lock[minor]));
  		if (filp->f_flags & O_NONBLOCK)
	        return -EAGAIN;
  		if (wait_event_interruptible(write_queue[minor], atomic_read(&(n_messages[minor])) != atomic_read(&(max_messages[minor]))))
	    	return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
	    spin_lock(&(write_lock[minor]));
  	}
  	
	/* allocating struct Mailslot */
	tmp_ms = kmalloc(sizeof(Mailslot), GFP_KERNEL);
  	tmp_ms->memory_buffer = kmalloc(count, GFP_KERNEL);
  	tmp_ms->buffer_size = count;
	tmp_ms->next = NULL;

	if (atomic_read(&(n_messages[minor])) == 0) // mailslot[minor] == NULL
	{
		mailslot[minor] = tmp_ms;
		actual_ms[minor] = mailslot[minor];
		spin_unlock(&(write_lock[minor]));
		//if(!(filp->f_flags & O_NONBLOCK)) 
		//{
			wake_up_interruptible(&(read_queue[minor]));
		//}
	} 
	else 
	{
		actual_ms[minor]->next = tmp_ms;
  		actual_ms[minor] = actual_ms[minor]->next;
  		spin_unlock(&(write_lock[minor]));
	}
	res = copy_from_user(tmp_ms->memory_buffer, buf, count);
	atomic_inc(&(n_messages[minor])); //atomic increment

	return count;
}

static long mailbox_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	int res, s, new_size;

	switch (cmd)
    {
        case MAILBOX_GET_MAX_BUFFER_SIZE:
        	s = atomic_read(&(max_size[minor]));
            res = copy_to_user((int *) arg, &s , sizeof(int));
            printk(KERN_INFO "Maximum buffer size: %d", s);
            break;
        case MAILBOX_SET_MAX_BUFFER_SIZE:
        	res = copy_from_user(&new_size, (int *) arg, sizeof(int));
        	if( new_size > OVER_LIMIT ){
        		return -EINVAL;
        	}
        	atomic_set(&(max_size[minor]), new_size);
        	
            printk(KERN_INFO "Maximum buffer size set to: %d", new_size);
            break;
        case MAILBOX_GET_MAX_MESSAGES:
        	s = atomic_read(&(max_messages[minor]));
            res = copy_to_user((int *) arg, &s , sizeof(int));
            
            break;
        case MAILBOX_SET_MAX_MESSAGES:
        	res = copy_from_user(&new_size, (int *) arg, sizeof(int));
        	if( new_size > OVER_LIMIT ){
        		return -EINVAL;
        	}
        	atomic_set(&(max_messages[minor]), new_size);
        	
            break;
        case MAILBOX_SET_BLOCKING:
        	filp->f_flags &= ~(O_NONBLOCK);
        	break;
        case MAILBOX_RESET_BLOCKING:
        	filp->f_flags |= O_NONBLOCK;
        	break;
        default:
            return -EINVAL;
    }
	return 0;
}

int init_module(void)
{
	int i;
	for(i = 0; i < DEVICE_MAX_NUMBER; i++){
		spin_lock_init(&(read_lock[i]));
		spin_lock_init(&(write_lock[i]));
		init_waitqueue_head(&(read_queue[i]));
		init_waitqueue_head(&(write_queue[i]));
		atomic_set(&(max_messages[i]), 10);	/* setting max size of each queue to 10 atomically */
		atomic_set(&(max_size[i]), 10);		/* setting max size of a message to 10 atomically */
		atomic_set(&(n_messages[i]), 0);	/* setting numbers of messages to 0 atomically */
	}

	Major = register_chrdev(0, DEVICE_NAME, &fops);
	if (Major < 0) 
	{
	  	printk(KERN_INFO "Registering noiser device failed\n");
		return Major;
	}
	printk(KERN_INFO "Mailbox device registered, it is assigned major number %d\n", Major);
  	return 0;
}

void cleanup_module(void)
{
	int i;
	Mailslot *tmp;
	unregister_chrdev(Major, DEVICE_NAME);
	printk(KERN_INFO "Mailbox device unregistered, it was assigned major number %d\n", Major);

	/* Freeing buffer memory */
	for(i = 0; i < DEVICE_MAX_NUMBER; i++){
		while(mailslot[i] != NULL){
			kfree(mailslot[i]->memory_buffer);
			tmp = mailslot[i]->next;
			kfree(mailslot[i]);
			mailslot[i] = tmp;
		}
	}
  	printk(KERN_INFO "Removing memory module\n");
}
