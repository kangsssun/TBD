#include "buzzer_api.h"
#include <signal.h>
#include <sys/wait.h>

#define DEV_NAME "/dev/mybuzzer"

int buzzer_show_problem() 
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
	ret = ioctl(fd, SHOW_BUZZER_PROBLEM);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_do()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "AC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_re()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "CC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}
int play_mi()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "EC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_fa()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "FC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_sol()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "HC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_la()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "JC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_si()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "LC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}

int play_high_do()
{
	int fd, ret;

	fd = open(DEV_NAME, O_RDWR);
	if(fd == -1) {
        printf("buzzer_api: %s (%d)\n", strerror(errno), __LINE__);
        return EXIT_FAILURE;
	}
	printf("buzzer_api: %s opened\n", DEV_NAME);
	sleep(1); // to avoid mixing messages

    printf("buzzer_api: call ioctl() with SHOW_PROBLEM\n");
	sleep(1); // to avoid mixing messages
    ret = write(fd, "MC", 2);
	printf("buzzer_api: return value is %d\n", ret);

	close(fd);
	printf("buzzer_api: %s closed\n", DEV_NAME);

	return EXIT_SUCCESS;
}
