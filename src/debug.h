#ifndef debug_h__included
#define debug_h__included

#include <stdlib.h>
#include <stdio.h>


#if DEBUG

static inline void hexdump(char *data, size_t len, int indent) {
    for (int i = 0; i < len;) {

        // indent
        for (int col = 0; col < indent; col++) {
            fprintf(stdout, " ");
        }

        // address
        fprintf(stdout, "0x%04x: ", i);

        // hex field
        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                fprintf(stdout, "%02x ", (uint8_t)data[i + col]);
            } else {
                fprintf(stdout, "   ");
            }
        }

        // separator
        fprintf(stdout, "  |  ");

        // ascii field
        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                char c = data[i + col];
                if ((c > 127) || (c < 32)) c = '.';
                fprintf(stdout, "%c", (uint8_t)c);
            } else {
                fprintf(stdout, " ");
            }            
        }

        fprintf(stdout, "\n");
        i += 16;
    }
}

#else /* DEBUG */

#define hexdump(_data, _len) /* */

#endif /* DEBUG */


#endif /* debug_h__included */
