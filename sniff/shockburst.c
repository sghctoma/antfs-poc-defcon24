/*
The MIT License (MIT)

Original work Copyright (c) 2014 Omri Iluz (omri@il.uz / http://cyberexplorer.me)
Modified work Copyright (c) 2016 Tamas Szakaly (sghctoma@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

CREDITS:
Dmitry Grinberg, CRC and Whiten code for BTLE - http://goo.gl/G9m8Ud
Open Source Mobile Communication, RTL-SDR information - http://sdr.osmocom.org/trac/wiki/rtl-sdr
Steve Markgraf, RTL-SDR Library - https://github.com/steve-m/librtlsdr
NRF24-BTLE-Decoder - https://github.com/omriiluz/NRF24-BTLE-Decoder
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#if defined(WIN32)
	#include <io.h>
	#include <fcntl.h>
#endif /* defined(WIN32) */
#include <stdlib.h> // For exit function
#include <unistd.h>    /* for getopt */

#include "packets.h"

/* Global variables */
int32_t g_threshold; // Quantization threshold
int g_srate; // sample rate downconvert ratio
const char g_json_fmt[] = "{\"sample\": %"PRId32", \"threshold\": %"PRId32", "
						"\"address\": \"0x%08"PRIX64"\", \"payload\": %s}\n";

/* Ring Buffer */
#define RB_SIZE 1000

int rb_head = -1;
int16_t *rb_buf;

/* Init Ring Buffer */
void RB_init(void)
{
	rb_buf = (int16_t *)malloc(RB_SIZE * 2);
}

/* increment Ring Buffer Head */
void RB_inc(void)
{
	rb_head++;
	rb_head=(rb_head) % RB_SIZE;
}
/* Access Ring Buffer location l */
#define RB(l) rb_buf[(rb_head+(l))%RB_SIZE]
/* end Ring Buffer */

/*
 * Quantize sample at location l by checking whether it's over the threshold.
 * Important - takes into account the sample rate downconversion ratio!
 */
static inline bool Quantize(int16_t l)
{
	return RB(l * g_srate) > g_threshold;
}
#define Q(l) Quantize(l)

/* Extract quantization threshold from preamble sequence */
int32_t ExtractThreshold(void)
{
	int32_t threshold = 0;
	int c;
	for (c = 0; c < 8 * g_srate; c++) {
		threshold += (int32_t)RB(c);
	}

	return (int32_t)threshold / (8 * g_srate);
}

/* Identify preamble sequence */
bool DetectPreamble(void)
{
	int transitions = 0;
	int c;

	// preamble sequence is based on the 9th symbol (either 0x55 or 0xAA)
	if (Q(9)) {
		for (c = 0; c < 8; c++) {
			transitions += Q(c) > Q(c + 1);
		}
	} else {
		for (c = 0; c < 8; c++) {
			transitions += Q(c) < Q(c + 1);
		}
	}

	return transitions == 4 && abs(g_threshold) < 15500;
}

/* Extract byte from ring buffer starting location l */
static inline uint8_t ExtractByte(int l)
{
	uint8_t byte = 0;
	int c;
	for (c = 0; c < 8; c++) {
		byte |= Q(l + c) << (7 - c);
	}

	return byte;
}

/* Extract count bytes from ring buffer starting location l into buffer*/
static inline void ExtractBytes(int l, uint8_t* buffer, int count)
{
	int t;
	for (t = 0; t < count; t++) {
		buffer[t] = ExtractByte(l + t * 8);
	}
}

/*
bool DecodePacket(int32_t sample, int srate)
{
	int i;
	g_srate = srate;
	g_threshold = ExtractThreshold();

	if (DetectPreamble()) {
		uint8_t tmp_buf[ADDRESS_LENGTH + PAYLOAD_LENGTH + 2];
		ExtractBytes(8, tmp_buf, sizeof(tmp_buf));
		ShockBurstPacket sbp;
		if (sbp_from_bytes(tmp_buf, &sbp)) {
			char payload[256];
			if (!ant_from_sbp(&sbp, payload, 256)) {
				for (i = 0; i < PAYLOAD_LENGTH; ++i) {
					sprintf(payload + 2 * i, "%02X", sbp.payload[i]);
				}
			}

			printf(g_json_fmt, sample, g_threshold, sbp.address, payload);

			return true;
		}
	}

	return false;
}
*/

bool DecodePacket(int32_t sample, int srate)
{
	int i;
	g_srate = srate;
	g_threshold = ExtractThreshold();

	if (DetectPreamble()) {
		uint8_t tmp_buf[ADDRESS_LENGTH + PAYLOAD_LENGTH + 2];
		ExtractBytes(8, tmp_buf, sizeof(tmp_buf));
		if (is_sbp(tmp_buf)) {
			for (i = 0; i < ADDRESS_LENGTH + PAYLOAD_LENGTH; ++i) {
				printf("%02X ", tmp_buf[i]);
			}
			printf("\n");
			fflush(stdout);

			return true;
		}
	}

	return false;
}

int main (int argc, char** argv)
{
	int16_t cursamp;
	int32_t samples = 0;
	int skip;
	int opt;
	bool optfail = false;
	int srate = 2;
	#if defined(WIN32)
		_setmode(_fileno(stdin), _O_BINARY);
	#endif /* defined(WIN32) */

	RB_init();
	g_threshold = 0;

	skip = 1000;

	while(!feof(stdin)) {
		cursamp  = (int16_t) ( fgetc(stdin) | fgetc(stdin) << 8);
		RB_inc();
		RB(0) = (int)cursamp;
		if (--skip < 1) {
			if (DecodePacket(++samples, srate)) {
				skip=20;
			}
		}
	}

	return 0;
}
