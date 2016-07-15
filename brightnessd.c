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


void get_now();

/****************************************************************
 * Write something useful here for once
 */
int target; // int because overflow checky checky woo yay blah
int now;
FILE *f;


int brightness_within_bounds(int bright, int lower, int upper)
{
	// to do: make a horrible but funny ternary statement
	if (bright < lower)
		return lower;

	if (bright > upper)
		return upper;

	return bright;
}

int main(int argc, char **argv)
{
	char buffer[4]; /* size 4 because max bright is 255, plus null terminator */

	// Open brightness file
	if ((f = fopen(BRIGHT_FILE, "w+")) == NULL)
	{
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	get_now();
	target = now;

	// Open a FIFO
	remove(FIFO_PATH);
	mkfifo(FIFO_PATH, 0666);
	chmod(FIFO_PATH, 0666);

	// FIXME : check return val
	int fifo = open(FIFO_PATH, O_RDWR);
	FILE *fifo_k = fopen(FIFO_PATH, "w");

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
		if (now < target)
			now += STEP;
		else if (now > target)
			now -= STEP;
		else if (now == target)
			delay = -1;

		now = brightness_within_bounds(now, 1, 255);
		fprintf(f, "%d\n", now);
		rewind(f);
	}

	// Do I need this?
	close(fifo);
	remove(FIFO_PATH);
	return 0;
}


void get_now()
{
	char buffer[4096]; // to do: magic constant is icky
	// to do check return value for eror
	fgets(buffer, sizeof(buffer), f);

	now = atoi(buffer);
}
