/*-
 * Copyright (c) 2008 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/types.h>
#include <sys/endian.h>

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <printf.h>

/*
 * Specifier '%b' takes two arguments: first is a pointer to data which
 * is to be printer. The second is data's length in bits.
 */
int
__printf_arginfo_BIN(const struct printf_info *pi, size_t n, int *argt)
{

	assert(n >= 2);
	argt[0] = PA_POINTER;
	argt[1] = PA_INT;
	return (2);
}

/*
 * Verilog-alike %b renderer
 */
int
__printf_render_BIN(struct __printf_io *io, const struct printf_info *pi, const void *const *arg)
{
	unsigned char buf[16];
	unsigned char *bitstr;
	unsigned bitleft;
	unsigned bitlen;
	unsigned byten;
	unsigned j;
	int bi, i, k, ret;

	bitstr = NULL;
	bitleft = bitlen = byten = j = 0;
	bi = i = k = ret = 0;

	bitstr = *((unsigned char **)arg[0]);
	bitlen = *((unsigned *)arg[1]);

	if (bitlen == 0)
		return (0);

	assert(bitstr != NULL);

	byten = bitlen / 8;
	bitleft = bitlen % 8;
	if (bitleft != 0)
		byten++;
	assert(byten != 0 && "byten == 0");

	for (k = byten - 1; k >= 0; k--) {
		if (k == byten - 1 && bitleft != 0) {
			bitlen -= bitleft;
			j = bitleft;
		} else
			j = 8;
		memset(buf, 0, sizeof(buf));
		for (i = j - 1, bi = 0; i >= 0; i--, bi++) {
			if (bitstr[k] & (1 << i))
				buf[bi] = '1';
			else
				buf[bi] = '0';
		}
		ret += __printf_puts(io, buf, bi);
		__printf_flush(io);
	}
	return (ret);
}

static void
usage(void)
{

	fprintf(stderr, "Usage: %s <-l | -b>\n", getprogname());
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	char b[256];
	char buf[256];
	char tmpbuf[256];
	
	int e;
	int i;
	int j;
	int m;
	int o;
	int flag_v;
	unsigned int vector;

	char *bp;
	FILE *calc;

	memset(b, 0, sizeof(b));
	memset(buf, 0, sizeof(buf));
	memset(tmpbuf, 0, sizeof(tmpbuf));

	e = -1;
	i = -1;
	j = -1;
	m = -1;
	o = -1;
	flag_v = 0;
	vector = 0;

	bp = NULL;
	calc = NULL;

	while ((o = getopt(argc, argv, "vlb")) != -1)
		switch (o) {
		case 'b':
			e = 'b';
			break;
		case 'l':
			e = 'l';
			break;
		case 'v':
			flag_v++;
			break;
		default:
			fprintf(stderr, "unknown option\n");
			usage();
		}

	if (e != 'b' && e != 'l') {
		fprintf(stderr, "You're not supposed to stress test printf_bin()"
		    " against unkown endianess; pick on with either -b or -l\n");
		usage();
	}
	if (e == 'l')
		le32enc(&vector, 0xdeadbeef);
	else
		be32enc(&vector, 0xdeadbeef);

	i = register_printf_render(
	    'B',
	    __printf_render_BIN,
	    __printf_arginfo_BIN
	);
	assert(i == 0);

	calc = popen("/usr/local/bin/calc", "w+");
	if (calc == NULL) {
		fprintf(stderr, "Can't execute calc(1) in /usr/local/bin/calc");
		exit(EXIT_FAILURE);
	}
	setbuf(calc, NULL);
	setbuf(stdout, NULL);

	/* Initial setting and buffer flush */
	fprintf(calc, "base(2);\n");
	fgets(buf, sizeof(buf), calc);

	for (i = 32; i >= 0; i--) {
		fprintf(calc, "%u\n", vector);
		memset(buf, 0, sizeof(buf));
		j = -1;
		bp = NULL;

		bp = fgets(buf, sizeof(buf), calc);
		assert(bp != NULL);
		bp = strchr(buf, '\n');
		if (bp != NULL)
			*bp = '\0';

		/* Skip initial <spaces>'0b' */
		bp = buf;
		while (isspace(*bp))
			bp++;
		if (bp[0] == '0' && bp[1] == 'b')
			bp += 2;

		/* To see which exactly bits don't match */
		for (j = 0; j < i; j++)
			bp[j] = ' ';
		memset(tmpbuf, 0, sizeof(tmpbuf));
		snprintf(tmpbuf, sizeof(tmpbuf), "%B", &vector, 32 - i);
		snprintf(b, sizeof(b), "%32s", tmpbuf);

		m = strncmp(bp, b, 32);
		if (flag_v > 0 || m != 0) {
			printf("from calc(1) = '%32s'\n", bp);
			printf("     from %%B = '%32s'", b);
			if (m != 0)
				printf("  (mismatch, m=%d)", m);
			printf("\n");
		}
	}
	fprintf(calc, "quit;\n");
	pclose(calc);
	exit(EXIT_SUCCESS);
}
