#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#define FIFO_PATH   "/tmp/brightnessd-fifo"
#define BRIGHT_FILE	"/sys/class/backlight/radeon_bl0/brightness"
#define DELAY		5
#define STEP		1
#define BIG_STEP	10

int get_now()
{
	char buffer[4096]; // FIXME: magic constant is icky
	// FIXME: check return value for eror
	fgets(buffer, sizeof(buffer), f);

	return atoi(buffer);
}

int brightness_within_bounds(int bright, int lower, int upper)
{
	// FIXME: make a horrible but funny ternary statement
	if (bright < lower)
		return lower;

	if (bright > upper)
		return upper;

	return bright;
}

int main(int argc, char **argv)
{
	int target = 0;
	int now = 0;
	FILE *f = NULL;
	char buffer[4]; /* size 4 because max bright is 255, plus null terminator */

	// Open brightness file
	if ((f = fopen(BRIGHT_FILE, "w+")) == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	now = get_now();
	target = now;

	// Open a FIFO
	remove(FIFO_PATH);
	mkfifo(FIFO_PATH, 0666);

	// FIXME : check return val
	int fifo = open(FIFO_PATH, O_RDWR);

	struct pollfd fds[1];

	fds[0].fd = fifo;
	fds[0].events = POLLIN;

	int delay = -1;

	while(1)
	{
		poll(fds, 1, delay);
		if (fds[0].revents & POLLIN)
		{
			delay = DELAY;
			read(fifo, buffer, sizeof(buffer));
			switch(buffer[0])
			{
				case '+':
					target += BIG_STEP;
					break;
				case '-':
					target -= BIG_STEP;
					break;
				default:
					target = atoi(buffer);
					break;
			}
			target = brightness_within_bounds(target, 1, 255);
		}
		if (now < target) {
			now += STEP;
			if (now > target)
				now = target;
		} else if (now > target) {
			now -= STEP;
			if (now < target)
				now = target;
		} else if (now == target) {
			delay = -1;
		}
		fprintf(f, "%d\n", now);
		rewind(f);
	}

	close(fifo);
	remove(FIFO_PATH);
	return 0;
}


