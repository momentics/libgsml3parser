/* Copyright 2026 momentics <momentics@gmail.com>
 * Copyright libgsml3parser contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* C89 acceptance check of the C ABI (gsml3parser_c.h).
 *
 * This file must compile as strict C (MSVC /TC /W4 /WX, GCC
 * -std=c89 -pedantic -Wall -Wextra -Werror) and link against the library.
 * It exercises: version, hex parse of an RR Channel Release, metadata,
 * hex round-trip, error path, and free. Exit code 0 iff all checks pass.
 *
 * C90 constraint: all declarations in a block precede any statement
 * (no mixed declarations and code).
 */

#include <gsml3parser/gsml3parser_c.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s (%s)\n", what, gsml3_last_error());
        failures++;
    }
}

int main(void)
{
    const char* v;
    gsml3_message* msg;
    gsml3_message* bad;

    v = gsml3_version();
    check(v != NULL && strlen(v) > 0, "version");

    /* RR Channel Release: PD=0x06, MTI=0x0D. */
    msg = gsml3_parse_l3_hex("60 0D 00", NULL);
    check(msg != NULL, "parse 60 0D 00");
    if (msg) {
        const char* name;
        uint8_t buf[256];
        size_t n;
        char* hex;

        name = gsml3_message_name(msg);
        check(name != NULL && strlen(name) > 0, "name");
        check(gsml3_message_pd(msg) == 6, "pd == 6");
        check(gsml3_message_mti(msg) == 13, "mti == 13");
        check(gsml3_message_ti(msg) == 0, "ti == 0");

        n = gsml3_message_write(msg, buf, sizeof(buf));
        check(n == 3, "write 3 bytes");
        check(buf[0] == 0x60 && buf[1] == 0x0D && buf[2] == 0x00, "wire bytes");

        hex = gsml3_message_hex(msg);
        check(hex != NULL && strcmp(hex, "600d00") == 0, "hex round-trip");
        if (hex) gsml3_free(hex);

        /* Reparse into the same handle (hot path). */
        check(gsml3_parse_l3_into(msg, buf, n, NULL) == GSML3_OK, "parse_into");
        check(gsml3_message_mti(msg) == 13, "parse_into content");

        gsml3_message_free(msg);
    }

    /* Error path: truncated input (RR ChannelRelease without its cause).
     * A single byte would NOT fail here: one-octet frames parse as RR
     * ChannelRequest short messages. */
    bad = gsml3_parse_l3_hex("60 0D", NULL);
    check(bad == NULL, "truncated parse fails");
    check(strlen(gsml3_last_error()) > 0, "last_error non-empty");

    /* NULL safety. */
    check(gsml3_message_name(NULL) != NULL, "name(NULL)");
    check(gsml3_message_pd(NULL) == -1, "pd(NULL)");
    gsml3_message_free(NULL);
    gsml3_free(NULL);

    if (failures == 0) {
        printf("c_api_c89_check: OK\n");
        return 0;
    }
    printf("c_api_c89_check: %d failure(s)\n", failures);
    return 1;
}
