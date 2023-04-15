#include "credentials.h"

#define MAX_INPUT_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

char* input(const char* const prompt, char* const answer);

#ifdef __cplusplus
}
#endif

int input_integer(const int min, const int max);
