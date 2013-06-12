
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
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd318189(v=vs.85).aspx
 */

#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static void
skip(size_t size, FILE *fp)
{

    	fseek(fp, size, SEEK_CUR);
}

static void
read_fourcc(char *buf, FILE *fp)
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
	char fourcc[4];
	uint32_t size;
};

static void
read_hdr(struct hdr *hdr, FILE *fp)
{

	read_fourcc(hdr->fourcc, fp);
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
};

#include <stdarg.h>

static void
iprintf(const struct ctx *ctx, const char *format, ...)
{
	unsigned int i;
	va_list ap;

	for (i = 0; i < ctx->indent; i++) {
		printf(" ");
	}

	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
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
		switch (type) {
		case 0x0013:
		case 0x0014:
			buf = malloc(size);
			read_bytes(buf, size, fp);
			iprintf(ctx, "%s: %s\n",
			    (type == 0x0013) ? "DateTimeOriginal" :
			    "CreateDate", buf);
			free(buf);
			break;
		default:
			iprintf(ctx, "ntcg %" PRIu16 " %" PRIx16 "\n",
			    type, size);
			skip(size, fp);
			break;
		}
		rest -= 4 + size;
	}
	ctx->indent--;
}

static void
riff(struct ctx *ctx, uint32_t rest, FILE *fp)
{

	ctx->indent++;
	while (rest > 0) {
		struct hdr h;
		uint32_t chunksize;

		read_hdr(&h, fp);
		if (!memcmp(h.fourcc, "LIST", 4)) {
			char type[4];

			read_fourcc(type, fp);
			iprintf(ctx, "LIST %" PRIu32 " %4.4s\n", h.size, type);
			riff(ctx, h.size - 4, fp);
		} else {
			iprintf(ctx, "%4.4s %" PRIu32 "\n", h.fourcc, h.size);
			if (!memcmp(h.fourcc, "nctg", 4)) {
			nctg(ctx, h.size, fp);
			} else {
				/* skip unknown fcc */
				iprintf(ctx, "%4.4s %" PRIu32 "\n", h.fourcc,
				    h.size);
				skip(size_pad(h.size), fp);
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

int
main(int argc, char *argv[])
{
	const char *filename;
	FILE *fp;
	struct hdr h;
	char type[4];
	struct ctx ctx;

	if (argc != 2) {
		exit(EXIT_FAILURE);
	}
	filename = argv[1];
	fp = fopen(filename, "rb");
	if (fp == NULL) {
		err(EXIT_FAILURE, "open %s", filename);
	}
	read_hdr(&h, fp);
	if (memcmp(h.fourcc, "RIFF", 4)) {
		errx(EXIT_FAILURE, "not RIFF");
	}
	read_fourcc(type, fp);
	printf("RIFF %" PRIu32 " %4.4s\n", h.size, type);
	ctx.indent = 0;
	riff(&ctx, h.size - 4, fp);

	exit(EXIT_SUCCESS);
}
