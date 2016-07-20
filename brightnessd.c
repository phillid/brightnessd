#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>

#define FIFO_PATH   "/tmp/brightnessd-fifo"
#define BRIGHT_FILE	"/sys/class/backlight/radeon_bl0/brightness"
#define DELAY		5
#define STEP		1
#define BIG_STEP	10
#define USER        "nobody"
#define BRIGHT_BUFFER_SIZE 4
#define DELAY_INFINITE (-1)

/**
 * create and open fifo, using chmod since mkfifo is affected by umask
 */
int setup_fifo()
{
	struct stat st;
	if (stat(FIFO_PATH, &st) == 0) {
		if (remove(FIFO_PATH)) {
			perror("remove("FIFO_PATH")");
			return 1;
		}
	}
	if (mkfifo(FIFO_PATH, 0666)) {
		perror("mkfifo");
		return 1;
	}
	if (chmod(FIFO_PATH, 0666)) {
		perror("chmod");
		return 1;
	}
	return 0;
}

int drop_priv()
{
	struct passwd *p = NULL;

	/* get group (and more) of USER */
	p = getpwnam(USER);

	if (p == NULL) {
		fprintf(stderr, "Failed to get uid and gid of user \""USER"\", bailing\n");
		return 1;
	}

	if (setgid(p->pw_gid)) {
		fprintf(stderr, "Failed to set gid to %d\n", p->pw_gid);
		perror("setuid");
		return 1;
	}

	if (setuid(p->pw_uid))
	{
		fprintf(stderr, "Failed to set uid to %d\n", p->pw_uid);
		perror("setuid");
		return 1;
	}

	if (!setuid(0) || !setgid(0)) {
		fprintf(stderr, "Got uid 0 or gid 0 back after dropping, bailing\n");
		return 1;
	}
	return 0;
}

int get_now(FILE *f)
{
	char buffer[BRIGHT_BUFFER_SIZE]; /* FIXME: magic constant is icky */
	/* FIXME: check return value for error */
	fgets(buffer, sizeof(buffer), f);

	return atoi(buffer);
}

int brightness_within_bounds(int bright, int lower, int upper)
{
	/* FIXME: make a horrible but funny ternary statement */
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
	int fifo = 0;
	struct pollfd fds;
	int delay = 0;
	int nread = 0;
	char buffer[BRIGHT_BUFFER_SIZE];

	/* Open brightness file */
	if ((f = fopen(BRIGHT_FILE, "w+")) == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}

	if (setup_fifo()) {
		fclose(f);
		return 1;
	}

	if (drop_priv() != 0) {
		fclose(f);
		return 1;
	}

	now = get_now(f);
	target = now;

	/* FIXME : check return val */
	fifo = open(FIFO_PATH, O_RDWR);

	fds.fd = fifo;
	fds.events = POLLIN;

	delay = DELAY_INFINITE;
	while(1) {
		poll(&fds, 1, delay);
		if (fds.revents & POLLIN) {
			delay = DELAY;
			nread = read(fifo, buffer, sizeof(buffer));
			if (nread == 0)
				perror("read");
			switch(buffer[0]) {
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
			delay = DELAY_INFINITE;
		}
		fprintf(f, "%d\n", now);
		rewind(f);
	}

	close(fifo);
	remove(FIFO_PATH);
	return 0;
}


