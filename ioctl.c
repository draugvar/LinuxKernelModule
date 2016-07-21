#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
 
#include "m_ioctl.h"
 
void get_buffer_size(int fd)
{
    int res;
    if (ioctl(fd, MAILBOX_GET_MAX_BUFFER_SIZE, &res) == -1)
    {
        perror("mailbox ioctl get buffer size\n");
    }
    else
    {
        printf("Size : %d\n", res);
    }
}

void set_buffer_size(int fd , char *argv)
{
    int res = atoi(argv);

    if (ioctl(fd, MAILBOX_SET_MAX_BUFFER_SIZE, &res) == -1)
    {
        perror("mailbox ioctl set buffer size\n");
    }
    else
    {
        printf("Size set to: %d\n", res);
    }
}

void set_blocking_flag(int fd)
{
    int res;
    if (ioctl(fd, MAILBOX_SET_BLOCKING, &res) == -1)
    {
        perror("mailbox ioctl set blocking flag\n");
    }
    else
    {
        printf("Blocking flag setted\n");
    }
}

void reset_blocking_flag(int fd)
{
    int res;
    if (ioctl(fd, MAILBOX_RESET_BLOCKING, &res) == -1)
    {
        perror("mailbox ioctl reset blocking flag\n");
    }
    else
    {
        printf("Blocking flag resetted\n");
    }
}
 
int main(int argc, char *argv[])
{
    char *file_name = "/dev/mailbox";
    char new_file_name[strlen(file_name) + 1];
    int fd;
    
    enum{
        e_get,
        e_set,
        b_set,
        b_reset
    } option;

    if (argc == 2)
    {
        option = e_get;
    }
    else if (argc == 3)
    {
        if (strcmp(argv[2], "-g") == 0)
        {
            option = e_get;
        }
        else if(strcmp(argv[2], "-sb") == 0)
        {
            option = b_set;
        }
        else if(strcmp(argv[2], "-rb") == 0)
        {
            option = b_reset;
        }
        else
        {
            fprintf(stderr, "Usage: %s minor [-g | -sb | -rb | -s buffer size]\n", argv[0]);
            return 1;
        }
    }
    else if (argc == 4)
    {
        if (strcmp(argv[2], "-s") == 0)
        {
            option = e_set;
        }
        else
        {
            fprintf(stderr, "Usage: %s minor [-g | -sb | -rb | -s buffer size]\n", argv[0]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s minor [-g | -sb | -rb | -s buffer size]\n", argv[0]);
        return 1;
    }

    if ( atoi(argv[1]) < 0 || atoi(argv[1]) > 255)
    {
        fprintf(stderr, "Minor number must be in (0, 255)!\n");
        return 1;
    }

    strcpy(new_file_name, file_name);
    strcat(new_file_name, argv[1]);

    fd = open(new_file_name, O_RDWR);

    if (fd == -1)
    {
        perror("ioctl open\n");
        return 2;
    }
 
    switch (option)
    {
        case e_get:
            get_buffer_size(fd);
            break;
        case b_set:
            set_blocking_flag(fd);
            break;
        case b_reset:
            reset_blocking_flag(fd);
            break;
        default:
            set_buffer_size(fd, argv[3]);
            break;
    }
 
    close (fd);
 
    return 0;
}