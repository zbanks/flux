#ifndef __ERR_H
#define __ERR_H

#include <stdlib.h>
#include <stdio.h>

#define ERROR(x) fprintf(stderr,"ERROR: " __FILE__ " line %d: " x "\n", __LINE__)
#define FAIL(x) {ERROR(x); exit(EXIT_FAILURE);}

#define UNUSED(x) ((void)(x))

#endif
