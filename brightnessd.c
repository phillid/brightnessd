#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#define FIFO_PATH   "/tmp/brightnessd-fifo"
#define BRIGHT_FILE	"/sys/class/backlight/radeon_bl0/brightness"
#define DELAY		1*5
#define STEP		1
#define BIG_STEP	5


void get_now();

/****************************************************************
 * Write something useful here for once
 */
volatile unsigned char target; // int because overflow checky checky woo yay blah
volatile unsigned char now;
FILE *f;


unsigned char brightness_within_bounds(int bright, int lower, int upper)
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
		printf("polling, %d ====> %d\n", now, target);
		poll(fds, 1, delay);
		if (fds[0].revents & POLLIN)
		{
			printf("polling wooorked\n");
			delay = DELAY;
			read(fifo, buffer, sizeof(buffer));
			printf("%s",buffer);
			switch(buffer[0])
			{
				case '+':
					printf("UP!\n");
					target += BIG_STEP;
					break;
				case '-':
					printf("DOWN\n");
					target -= BIG_STEP;
					break;
				default:
					target = atoi(buffer);
					printf("FOO %d\n", target);
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

void brightness_thread()
{
	char buffer[4096]; // to do: magic constant is icky

	while(1)
	{
		// Wait for the target to be different from the current brightness
		while (now == target)
			usleep(DELAY*50);

		// Adjust the brightness to match target
		while(now != target)
		{
			if (now > target)
				now -= STEP;

			if (now < target)
				now += STEP;

			fprintf(f, "%d\n", now);
			rewind(f);
			usleep(DELAY);
		}
	}
}
