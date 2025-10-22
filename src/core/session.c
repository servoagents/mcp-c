#include <stdlib.h>
#include <string.h>
#include "mcp_session.h"

mcp_session_t *mcp_session_new(int sock) {
    mcp_session_t *s = (mcp_session_t *) calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->sock = sock;
    return s;
}

void mcp_session_free(mcp_session_t *s) {
    if (!s)
        return;
    free(s);
}
