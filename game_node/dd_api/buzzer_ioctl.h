#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC 'k'

#define SHOW_BUZZER_PROBLEM        _IO(MY_IOCTL_MAGIC, 1)
#define MY_IOCTL_CMD_THREE	_IO(MY_IOCTL_MAGIC, 2)