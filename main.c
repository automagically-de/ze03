#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "ascii.h"

static const char *device = "/dev/ttyS0";
static struct timeval tv1, tv2;
static int ser;

int reconnect(void);
int ser_connect(const char *device);
int cmd_send(int ser, const char *cmd, int checksum);
int cmd_recv(int ser, int timeout);
int brute_force(int ser, unsigned int cmdlen);
float seconds_elapsed(void);

int main(int argc, char *argv[])
{
	int i;
#if 0
	char c, buf[BUFSIZ];
#endif

	if(!reconnect())
		return EXIT_FAILURE;

#if 1
	for(i = 4; i < 8; i ++)
		brute_force(ser, i);
#endif

	/*cmd_send(ser, "SS12345678901234567890", 1);*/
	/*cmd_send(ser, "SU00421105220109", 1); */
#if 0
	cmd_send(ser, "L01", 0);
	cmd_recv(ser, 2);
#endif
#if 0
	buf[1] = '\0';
	for(c = 'A'; c <= 'Z'; c ++) {
		buf[0] = c;
		cmd_send(ser, buf, 1);
		cmd_recv(ser, 2);
	}
#endif
	close(ser);
	return EXIT_SUCCESS;
}

int brute_force(int ser, unsigned int cmdlen)
{
	char *cmdbuf;
	int i, cnt = 0;
	float t;

	printf("I: starting brute force run with buffer length %d\n", cmdlen);

	cmdbuf = malloc(cmdlen + 1);
	cmdbuf[cmdlen] = 0;
	for(i = 0; i < cmdlen; i ++)
		cmdbuf[i] = '0';
	while(1) {
		cnt ++;
		/* next one */
		for(i = 0; i < cmdlen; i ++) {
			cmdbuf[i] ++;
			if(cmdbuf[i] == ('Z' + 1)) {
				cmdbuf[i] = '0';
				if(i == (cmdlen - 1)) {
					free(cmdbuf);
					return 0;
				}
			} else {
				if(cmdbuf[i] == ('9' + 1))
					cmdbuf[i] = 'A';
				break;
			}
		}
		/* test it */
		if(cmdbuf[0] == 'H');
		else if(strncmp(cmdbuf, "CR", 2) == 0);
		else {
			cmd_send(ser, cmdbuf, 0);
			if((cmd_recv(ser, 10) != 0) && (cmdbuf[0] != 'H'))
				printf("cmd: %s (w/o checksum)\n", cmdbuf);
			t = seconds_elapsed();
			if(t > 1.0) {
				printf("cmd: %s (%.4f seconds, w/o checksum) - resetting...\n",
					cmdbuf, t);
				reconnect();
			}

#if 0
			cmd_send(ser, cmdbuf, 1);
			if((cmd_recv(ser, 10) != 0) && (cmdbuf[0] != 'H'))
				printf("cmd: %s (with checksum)\n", cmdbuf);
			t = seconds_elapsed();
			if(t > 1.0)
				printf("cmd: %s (%.4f seconds, with checksum)\n", cmdbuf, t);
#endif
			fflush(stdout);
		}
		if(cnt >= 10000) {
			printf("I: 10000 checks done (%s), resetting...\n", cmdbuf);
			reconnect();
			cnt = 0;
		}
	}
	free(cmdbuf);
	return 0;
}

int reconnect(void)
{
	if(ser != -1)
		close(ser);
	sleep(1);

	ser = ser_connect(device);
	if(ser < 0)
		return 0;

	cmd_send(ser, "H", 0);
	cmd_recv(ser, 2);
	return 1;
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

	if(cfsetispeed(&tios, B57600) < 0) {
		perror(device);
		return -1;
	}

	tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | ICRNL | INLCR);
	tios.c_iflag &= ~(PARENB | INPCK | ISTRIP | IXON | IMAXBEL | IGNCR);
	tios.c_iflag |= IGNBRK;
	tios.c_oflag &= ~(OPOST | ONLCR);
	tios.c_cflag &= ~(ISIG | ICANON | IEXTEN | PARENB);
	tios.c_cflag |= (CS8);

	tios.c_cc[VMIN] = 1;
	tios.c_cc[VTIME] = 5;

	if(tcsetattr(ser, TCSANOW, &tios) < 0) {
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
	fflush(stdout);
}

static unsigned char cmd_crc(unsigned char *cmd, unsigned int len)
{
	unsigned int sum = 0;
	int i;

	for(i = 0; i < len; i ++)
		sum += cmd[i];
	return sum & 0xFF;
}

float seconds_elapsed(void)
{
	return ((float)tv2.tv_sec + (float)tv2.tv_usec / 1000000.0) -
			((float)tv1.tv_sec + (float)tv1.tv_usec / 1000000.0);
}

int cmd_send(int ser, const char *cmd, int checksum)
{
	unsigned int len, cmdlen, pos;
	unsigned char *cmdbuf;
	int rv = 0;

	gettimeofday(&tv1, NULL);
	memcpy(&tv2, &tv1, sizeof(tv1));

	cmdlen = strlen(cmd);
	len = cmdlen + 2 + (checksum ? 2 : 0);
	cmdbuf = malloc(len * sizeof(unsigned char));
	cmdbuf[0] = C_STX;
	memcpy(cmdbuf + 1, cmd, len);
	pos = cmdlen + 1;
	if(checksum) {
		sprintf((char *)(cmdbuf + pos), "%02X", cmd_crc(cmdbuf + 1, cmdlen));
		pos += 2;
	}
	cmdbuf[pos] = C_EOT;

#if 0
	dump(cmdbuf, len, "Q");
#endif
	rv = write(ser, cmdbuf, len);

	free(cmdbuf);
	return rv;
}

int cmd_recv(int ser, int timeout)
{
	unsigned char buf[BUFSIZ + 1], *bufp, *bufp2, t = timeout * 50;
	ssize_t r, n = 0;

	memset(buf, 0, BUFSIZ);
	bufp = buf;
	do {
		r = read(ser, bufp, BUFSIZ - n);
		gettimeofday(&tv2, NULL);
		if(r > 0) {
			n += r;
#if 1
			if(strncmp((char *)buf, "\x02\x15\x04", 3) == 0)
				return 0;
#endif
			bufp2 = bufp;
			while(*bufp2 != '\0') {
				if(*bufp2 == C_EOT) {
					dump(buf, n, "A");
					return n;
				}
				bufp2 ++;
			}
			bufp += r;
		} else if(r < 0) {
			if(errno != EAGAIN) {
				perror("cmd_recv");
				return -1;
			}
		}
		usleep(20000);
	} while(-- t);
	return 0;
}
