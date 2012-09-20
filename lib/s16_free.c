#include "fresample.h"
#include <stdlib.h>

void
lfr_s16_free(struct lfr_s16 *fp)
{
    free(fp);
}
