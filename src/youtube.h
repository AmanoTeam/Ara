#include <curl/curl.h>

int youtube_parse(CURL* curl, const char* const uri, struct Media* media);