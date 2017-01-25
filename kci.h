#ifndef KCI_H
#define KCI_H

#include <linux/ioctl.h>


/* The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. */
#define MAJOR_NUM 245 // checked major was avaliable with command: head /proc/devices
#define MAX 1024 // for buffer

/* Generates ioctl commands numbers, used to write data to the driver */
#define IOCTL_SET_PID _IOW(MAJOR_NUM, 0, unsigned long)
#define IOCTL_SET_FD _IOW(MAJOR_NUM, 1, unsigned long)
#define IOCTL_CIPHER _IOW(MAJOR_NUM, 2, unsigned long)

#define LOF_FILE_PATH "/sys/kernel/debug/kcikmod/calls"
#define DEVICE_RANGE_NAME "kci_dev"
#define KCIKMOD "kcikmod"
#define CALLS "calls"


///// define commands: /////
#define INIT_STR "-init"
#define PID_STR "-pid"
#define FD_STR "-fd"
#define START_STR "-start"
#define STOP_STR "-stop"
#define RM_STR "-rm"
////////////////////////////

#define SUCCESS 0


#endif
