#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "buzzer_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

int buzzer_show_problem();
int play_do();
int play_re();
int play_mi();
int play_fa();
int play_sol();
int play_la();
int play_si();
int play_high_do();

#ifdef __cplusplus
}
#endif
