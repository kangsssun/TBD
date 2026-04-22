#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC 'k'

#define SHOW_PROBLEM        _IO(MY_IOCTL_MAGIC, 1)
#define CORRECT             _IO(MY_IOCTL_MAGIC, 2)
#define MY_IOCTL_CMD_THREE	_IO(MY_IOCTL_MAGIC, 3)
#define MY_IOCTL_CMD_LED_ON     _IO(MY_IOCTL_MAGIC, 4)
#define MY_IOCTL_CMD_LED_OFF    _IO(MY_IOCTL_MAGIC, 5)

