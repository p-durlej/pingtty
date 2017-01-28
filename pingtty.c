/* Copyright (c) 2008, 2009, 2016, 2017, Piotr Durlej
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>

static void echo(int ch)
{
	if (isprint(ch))
	{
		putchar(ch);
		return;
	}
	if (iscntrl(ch))
	{
		putchar('^');
		putchar('@' + (ch & 31));
		return;
	}
	putchar('.');
}

static long ping_tty(void)
{
	struct termios tio0, tio1;
	struct timeval tv0, tv1;
	char buf[64];
	long d;
	int fl;
	int cnt;
	int i;
	
	if (tcgetattr(0, &tio0))
		err(1,"tcgetattr");
	tio1 = tio0;
	tio1.c_lflag &= ~(ECHO | ICANON);
	tio1.c_lflag = 0;
	if (tcsetattr(1, TCSANOW, &tio1))
		err(1, "tcsetattr");
	
	gettimeofday(&tv0, NULL);
	write(1, "\033[c", 3);
	while (read(0, buf, sizeof buf) <= 0);
	gettimeofday(&tv1, NULL);
	sleep(1);
	
	fl = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, fl | O_NDELAY);
	while (cnt = read(0, buf, sizeof buf), cnt > 0)
		for (i = 0; i < cnt; i++)
			if (buf[i] == tio1.c_cc[VINTR])
			{
				echo(buf[i]);
				d = -1;
				goto fini;
			}
	fcntl(0, F_SETFL, fl);
	
	d  = (tv1.tv_sec  - tv0.tv_sec) * 1000000;
	d +=  tv1.tv_usec - tv0.tv_usec;
	
fini:
	if (tcsetattr(0, TCSANOW, &tio0))
		err(1, "tcsetattr");
	return d;
}

static void pr_stat(const char *ttyn, long min, long max, long avg)
{
	printf("--- %s ping statistics ---\n"
	       "rtt min/avg/max = %li.%03li/%li.%03li/%li.%03li ms\n",
		ttyn,
		min / 1000,
		min % 1000,
		avg / 1000,
		avg % 1000,
		max / 1000,
		max % 1000);
}

int main(int argc, char **argv)
{
	struct winsize wsz;
	const char *ttyn;
	char buf[64];
	long acc = 0;
	long min = LONG_MAX;
	long max = 0;
	long t;
	int cnt = 3;
	int pos;
	int ttynl;
	int len;
	int i, n;
	
	if (argc > 1)
		cnt = atoi(argv[1]);
	ttyn = ttyname(0);
	if (ttyn == NULL)
		errx(1, "stdin: Not a tty");
	
	ioctl(0, TIOCGWINSZ, &wsz);
	ttynl = strlen(ttyn);
	
	if (!wsz.ws_col)
		wsz.ws_col = 80;
	
	printf("%s: ", ttyn);
	pos = ttynl + 2;
	for (i = 0; i < cnt; i++)
	{
		t = ping_tty();
		if (t < 0)
		{
			putchar('\n');
			cnt = i;
			goto fini;
		}
		acc += t;
		if (t < min)
			min = t;
		if (t > max)
			max = t;
		snprintf(buf, sizeof buf - 1, "%3i ms ", (int)(t + 500) / 1000);
		len = strlen(buf);
		if (pos + len > wsz.ws_col)
		{
			putchar('\n');
			for (n = -2; n < ttynl; n++)
				putchar(' ');
			fflush(stdout);
			pos = strlen(ttyn) + 2;
		}
		fputs(buf, stdout);
		fflush(stdout);
		pos += len;
	}
	printf("\n");
fini:
	if (cnt)
		pr_stat(ttyn, min, max, acc / cnt);
	return 0;
}
