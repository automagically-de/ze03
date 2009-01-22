#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "ascii.h"

int ser_connect(const char *device);
int cmd_send(int ser, const char *cmd, int checksum);
int cmd_recv(int ser, int timeout);

int main(int argc, char *argv[])
{
	int ser = ser_connect("/dev/ttyS0");
	if(ser < 0) {
		return EXIT_FAILURE;
	}

	cmd_send(ser, "H", 0);
	cmd_recv(ser, 2);

	close(ser);
	return EXIT_SUCCESS;
}

int ser_connect(const char *device)
{
	int ser, linebits;
	struct termios tios;

	ser = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if(ser < 0) {
		perror(device);
		return -1;
	}
	if(tcgetattr(ser, &tios) < 0) {
		perror(device);
		return -1;
	}
	tios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | PARENB);
	tios.c_cflag &= ~(CSIZE | CSTOPB | OPOST);
	tios.c_cflag |= (CS8);

	if(cfsetispeed(&tios, B57600) < 0) {
		perror(device);
		return -1;
	}

	if(tcsetattr(ser, TCSAFLUSH, &tios) < 0) {
		perror(device);
		return -1;
	}

	ioctl(ser, TIOCMGET, &linebits);
	linebits &= ~(TIOCM_DTR | TIOCM_RTS);
	ioctl(ser, TIOCMSET, &linebits);

	return ser;
}

static void dump(unsigned char *cmd, unsigned int len, const char *prefix)
{
	int i;

	printf("%s: ", prefix);
	for(i = 0; i < len; i ++)
		printf("0x%02x ", cmd[i]);
	printf("(");
	for(i = 0; i < len; i ++)
		printf("%c", isprint(cmd[i]) ? cmd[i] : '_');
	printf(")\n");
}

static unsigned char cmd_crc(unsigned char *cmd, unsigned int len)
{
	unsigned int sum = 0;
	int i;

	for(i = 0; i < len; i ++)
		sum += cmd[i];
	return sum & 0xFF;
}

int cmd_send(int ser, const char *cmd, int checksum)
{
	unsigned int len, cmdlen;
	unsigned char *cmdbuf;
	int rv = 0;

	cmdlen = strlen(cmd);
	len = cmdlen + 2 + (checksum ? 1 : 0);
	cmdbuf = malloc(len * sizeof(unsigned char));
	cmdbuf[0] = C_STX;
	memcpy(cmdbuf + 1, cmd, len);
	cmdbuf[cmdlen + 1] = C_EOT;

	dump(cmdbuf, len, "Q");
	rv = write(ser, cmdbuf, len);

	free(cmdbuf);
	return rv;
}

int cmd_recv(int ser, int timeout)
{
	unsigned char buf[BUFSIZ];
	ssize_t r;

	do {
		memset(buf, 0, BUFSIZ);
		r = read(ser, buf, BUFSIZ);
		if(r > 0) {
			dump(buf, r, "A");
			return r;
		} else if(r < 0) {
			if(errno != EAGAIN) {
				perror("cmd_recv");
				return -1;
			}
		}
		sleep(1);
	} while(-- timeout);
	return 0;
}
