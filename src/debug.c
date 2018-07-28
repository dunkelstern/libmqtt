#if DEBUG

#include "stdio.h"

#include "debug.h"

void hexdump(char *data, size_t len) {
    for (int i = 0; i < len;) {
        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                fprintf(stdout, "%02x ", data[i + col]);
            } else {
                fprintf(stdout, "   ");
            }
        }

        fprintf(stdout, "  |  ");

        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                char c = data[i + col];
                if ((c > 127) || (c < 32)) c = '.';
                fprintf(stdout, "%c", c);
            } else {
                fprintf(stdout, " ");
            }            
        }

        fprintf(stdout, "\n");
        i += 16;
    }
}

#endif /* DEBUG */
