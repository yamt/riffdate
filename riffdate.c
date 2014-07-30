
/*-
 * Copyright (c)2013 YAMAMOTO Takashi,
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
 */

/*
 * riffdate: try to extract create time of a RIFF file
 */

/*
 * tested with AVI files produced by the following cameras.
 *    SIGMA DP2
 *	IDIT: THU FEB 13 11:03:15 2011
 *	IDIT-exiftime: 2011:02:13 11:03:15
 *    SIGMA DP2 Merrill
 *	IDIT: FRI SEP 07 23:37:15 2012
 *	IDIT-exiftime: 2012:09:07 23:37:15
 *    Nikon COOLPIX S30
 *	ntcg-DateTimeOriginal: 2013:06:11 13:30:25
 *	ntcg-CreateDate: 2013:06:11 13:30:25
 */

/*
 * AVI RIFF File Reference
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd318189(v=vs.85).aspx
 */

#include <sys/cdefs.h>

#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__arraycount)
#define	__arraycount(a)	(sizeof(a) / sizeof(*a))
#endif

bool show_tree = false;  /* a knob for debug */

static void
read_bytes(void *buf, size_t size, FILE *fp)
{

	if (fread(buf, size, 1, fp) != 0) {
		if (ferror(fp)) {
			err(EXIT_FAILURE, "fread");
		}
		if (feof(fp)) {
			errx(EXIT_FAILURE, "EOF");
		}
	}
}

static void
read4(void *buf, FILE *fp)
{

	read_bytes(buf, 4, fp);
}

static void *
save(size_t size, FILE *fp)
{
	char *buf;

	buf = malloc(size + 1);
	read_bytes(buf, size, fp);
	buf[size] = 0; /* NUL terminate for safety */
	return buf;
}


static void
skip(size_t size, FILE *fp)
{

	if (fseeko(fp, (off_t)size, SEEK_CUR) == -1) {
		err(EXIT_FAILURE, "fseeko");
	}
}

static void
read_fcc(char *buf, FILE *fp)
{

	read4(buf, fp);
}

static void
read_u16(uint16_t *sz, FILE *fp)
{
	unsigned char buf[2];

	read_bytes(buf, 2, fp);
	*sz = (uint32_t)buf[0]
	    + ((uint32_t)buf[1] << 8);
}

static void
read_u32(uint32_t *sz, FILE *fp)
{
	unsigned char buf[4];

	read4(buf, fp);
	*sz = (uint32_t)buf[0]
	    + ((uint32_t)buf[1] << 8)
	    + ((uint32_t)buf[2] << 16)
	    + ((uint32_t)buf[3] << 24);
}

struct hdr {
	char fcc[4];
	uint32_t size;
};

static void
read_hdr(struct hdr *hdr, FILE *fp)
{

	read_fcc(hdr->fcc, fp);
	read_u32(&hdr->size, fp);
}

static uint32_t
size_pad(uint32_t sz)
{

	/* WORD boundary */
	return (sz + 1) / 2 * 2;
}

struct ctx {
	unsigned int indent;
	char *idit;
	char *iprd;
	char *isft;
};

#include <stdarg.h>

static void
iprintf(const struct ctx *ctx, const char *format, ...)
{
	unsigned int i;
	va_list ap;

	if (!show_tree) {
		return;
	}
	for (i = 0; i < ctx->indent; i++) {
		printf(" ");
	}

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

/*
 * SIGMA DP1, DP1s, DP1x, DP2, DP2s, DP2x produce broken IDIT values like
 *    "THU FEB 0= 0;:03:0? 200;\n "
 *    "SUN APR 0> 11:08:03 200=\n "
 *    "SUN APR 0> 11:07:37 200=\n "
 */

static void
fix_sigma_idit_number(char *buf, unsigned int idx, unsigned int n)
{
	unsigned int i;
	unsigned int v;

	v = 0;
	for (i = 0; i < n; i++) {
		v = v * 16 + buf[idx + i] - '0';
	}
	for (i = 0; i < n; i++) {
		buf[idx + n - 1 - i] = '0' + (v % 10);
		v /= 10;
	}
}

static void
fix_sigma_idit(char *buf)
{
	static const unsigned int where[] = {8, 11, 14, 17, 22};
	unsigned int i;

	/*
	 * sanity checks
	 */
	if (strlen(buf) != strlen("SUN APR 0> 11:07:37 200=\n ")) {
		return;
	}
	for (i = 0; i < __arraycount(where); i++) {
		const char ch = buf[where[i]];

		if (ch < '0' || '0' + 16 <= ch) {
			return;
		}
	}

	/*
	 * fix digits
	 */
	for (i = 0; i < __arraycount(where); i++) {
		fix_sigma_idit_number(buf, where[i], 2);
	}
}

static bool
has_sigma_idit_bug(const char *isft)
{
	/*
	 * The following list of affected models and their ISFT values
	 * are kindly provided by SIGMA.
	 */
	static const char * const isft_list[] = {
	    "DP1", "DP1S", "DP1X",
	    "DP2", "DP2S", "DP2X",
	};
	unsigned int i;

	for (i = 0; i < __arraycount(isft_list); i++) {
		if (!strcmp(isft, isft_list[i])) {
			return true;
		}
	}
	return false;
}

/* http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/Nikon.html#AVITags */
static void
nctg(struct ctx *ctx, uint32_t rest, FILE *fp)
{

	ctx->indent++;
	while (rest > 0) {
		uint16_t type;
		uint16_t size;
		char *buf;

		read_u16(&type, fp);
		read_u16(&size, fp);
		if (show_tree) {
			goto show_tree;
		}
		iprintf(ctx, "ntcg %" PRIu16 " %" PRIx16 "\n", type, size);
		switch (type) {
		case 0x0013:
		case 0x0014:
			buf = malloc(size);
			read_bytes(buf, size, fp);
			printf("%s: %s\n",
			    (type == 0x0013) ? "ntcg-DateTimeOriginal" :
			    "ntcg-CreateDate", buf);
			free(buf);
			break;
		default:
		show_tree:
			skip(size, fp);
			break;
		}
		rest -= 4 + size;
	}
	ctx->indent--;
}

static char *
convert_to_exif_time(char *buf, size_t buflen, const char *from)
{
	/*
	 * from:
	 * 	FRI SEP 07 23:37:15 2012
	 * to:
	 * 	YYYY:MM:DD hh:mm:ss
	 */

#if 0
	static const char * const day_of_week[] = {
		"sun", "mon", "tue", "wed", "thu", "fri", "sat"
	};
#endif
	static const char * const months[] = {
		"jan", "feb", "mar", "apr", "may", "jun",
		"jul", "aug", "sep", "oct", "nov", "dec"
	};
	unsigned int i;
	char day_of_week[3];
	char month[3];
	int YYYY;
	int MM;
	int DD;
	int hh;
	int mm;
	int ss;
	int ret;

	ret = sscanf(from, "%3s %3s %2d %2d:%2d:%2d %4d",
	    day_of_week, month, &DD, &hh, &mm, &ss, &YYYY);
	if (ret != 7) {
		return NULL;
	}
	MM = -1;
	for (i = 0; i < __arraycount(months); i++) {
		if (!strcasecmp(months[i], month)) {
			MM = i + 1;
			break;
		}
	}
	if (MM == -1) {
		return NULL;
	}
	ret = snprintf(buf, buflen, "%04d:%02d:%02d %02d:%02d:%02d",
	    YYYY, MM, DD, hh, mm, ss);
	if (ret == -1 || ret >= buflen) {
		return NULL;
	}
	return buf;
}

static void
print_idit(const char *idit)
{
	char buf[100];
	char *exif_time;

	exif_time = convert_to_exif_time(buf, sizeof(buf), idit);
	if (exif_time != NULL) {
		printf("IDIT-exiftime: %s\n", exif_time);
	} else {
		printf("IDIT: %s\n", idit);
	}
}

static void
riff(struct ctx *ctx, uint32_t rest, FILE *fp)
{

	ctx->indent++;
	while (rest > 0) {
		struct hdr h;
		uint32_t chunksize;

		read_hdr(&h, fp);
		if (!memcmp(h.fcc, "LIST", 4)) {
			char type[4];

			read_fcc(type, fp);
			iprintf(ctx, "LIST %" PRIu32 " %4.4s\n", h.size, type);
			if (!show_tree && !memcmp(type, "movi" ,4)) {
				break;
			}
			riff(ctx, h.size - 4, fp);
		} else {
			iprintf(ctx, "%4.4s %" PRIu32 "\n", h.fcc, h.size);
			if (!memcmp(h.fcc, "IPRD", 4)) {
				/* RIFF:AVI/LIST:INFO/IPRD */
				free(ctx->iprd); /* pedantic */
				ctx->iprd = save(h.size, fp);
			} else if (!memcmp(h.fcc, "ISFT", 4)) {
				/* RIFF:AVI/LIST:INFO/ISFT */
				free(ctx->isft); /* pedantic */
				ctx->isft = save(h.size, fp);
			} else if (!show_tree && !memcmp(h.fcc, "IDIT", 4)) {
				/* RIFF:AVI/IDIT */
				/* XXX should be RIFF:AVI/LIST:hdrl/IDIT ??? */
				/* http://www.den4b.com/forum/viewtopic.php?id=723 */
				free(ctx->idit); /* pedantic */
				ctx->idit = save(h.size, fp);
			} else if (!memcmp(h.fcc, "nctg", 4)) {
				nctg(ctx, h.size, fp);
			} else {
				/* skip unknown fcc */
				skip(size_pad(h.size), fp);
			}
			/*
			 * SIGMA DP2 bug workaround
			 */
			if (ctx->iprd != NULL && ctx->isft != NULL &&
			    ctx->idit != NULL) {
				if (!strcmp(ctx->iprd, "SIGMA") &&
				    has_sigma_idit_bug(ctx->isft)) {
					fix_sigma_idit(ctx->idit);
				}
				print_idit(ctx->idit);
				free(ctx->iprd);
				free(ctx->isft);
				free(ctx->idit);
				ctx->iprd = NULL;
				ctx->isft = NULL;
				ctx->idit = NULL;
			}
		}
		chunksize = 8 + size_pad(h.size);
		if (rest < chunksize) {
			iprintf(ctx, "inconsist sizes %" PRIu32 " < %" PRIu32
			    "\n", rest, chunksize);
			break;
		}
		rest -= chunksize;
	}
	ctx->indent--;
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-d] file\n", getprogname());
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	const char *filename;
	FILE *fp;
	struct hdr h;
	char type[4];
	struct ctx ctx;
	int ch;
	extern int optind;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			show_tree = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		usage();
	}
	filename = argv[0];
	fp = fopen(filename, "rb");
	if (fp == NULL) {
		err(EXIT_FAILURE, "open %s", filename);
	}
	read_hdr(&h, fp);
	if (memcmp(h.fcc, "RIFF", 4)) {
		errx(EXIT_FAILURE, "not RIFF");
	}
	read_fcc(type, fp);
	if (show_tree) {
		printf("RIFF %" PRIu32 " %4.4s\n", h.size, type);
	}
	memset(&ctx, 0, sizeof(ctx));
	riff(&ctx, h.size - 4, fp);
	if (ctx.idit != NULL) {
		print_idit(ctx.idit);
		free(ctx.idit);
	}

	exit(EXIT_SUCCESS);
	/*NOTREACHED*/
}
