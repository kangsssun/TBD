#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "led_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

int led_show_problem();

#ifdef __cplusplus
}
#endif
