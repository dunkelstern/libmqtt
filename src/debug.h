#ifndef debug_h__included
#define debug_h__included

#include <stdlib.h>

#if DEBUG

void hexdump(char *data, size_t len);

#else /* DEBUG */

#define hexdump(_data, _len) /* */

#endif /* DEBUG */


#endif /* debug_h__included */
