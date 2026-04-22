#include "led_api.h"

#define DEV_NAME "/dev/myled"

int led_show_problem() 
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("led_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("led_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("led_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
	ret = ioctl(fd, SHOW_PROBLEM);
	printf("led_api: return value is %d\n", ret);

	close(fd);
	printf("led_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int led_correct()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("led_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("led_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("led_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
	ret = ioctl(fd, CORRECT);
	printf("led_api: return value is %d\n", ret);

	close(fd);
	printf("led_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}
