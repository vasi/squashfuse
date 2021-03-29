/* Compare results of our macros vs. functions provided by
 * endian.h, if available. If not available, simply skip
 * tests.
 */
#if defined(HAVE_ENDIAN_H)
#define _DEFAULT_SOURCE
#include <endian.h>
#elif defined(HAVE_MACHINE_ENDIAN_H)
#include <machine/endian.h>
#else
#define SKIP_ENDIAN_TESTS
#endif

#ifndef SKIP_ENDIAN_TESTS
#include "swap.h"

int test16(void) {
	uint16_t val = htole16(0xbabe);
	sqfs_swapin16(&val);
	return val == 0xbabe;
}

int test32(void) {
	uint32_t val = htole32(0xc0ffee01);
	sqfs_swapin32(&val);
	return val == 0xc0ffee01;
}

int test64(void) {
	uint64_t val = htole64(0xf00ddeadbeeff00dUL);
	sqfs_swapin64(&val);
	return val == 0xf00ddeadbeeff00dUL;
}

int main(void) {
	return test16() && test32() && test64() ? 0 : 1;
}

#else /* SKIP_ENDIAN_TESTS */

int main(void) {
	return 0;
}

#endif
