#define UERR_SUCCESS 0 /* No error */

#define UERR_MEMORY_ALLOCATE_FAILURE -1 /* Failed to allocate memory */

#define UERR_CURL_FAILURE -2 /* Cannot open file for read/write */
#define UERR_PTHREAD_FAILURE -3 /* Cannot read contents of file */
#define UERR_ASPRINTF_FAILURE -4 /* Cannot write contents to file */
#define UERR_STRSTR_FAILURE -5 /* Cannot move file */

#define UERR_JSON_CANNOT_PARSE -6 /* Cannot parse JSON tree */
#define UERR_JSON_MISSING_REQUIRED_KEY -7 /* Missing required key in JSON tree */
#define UERR_JSON_NON_MATCHING_TYPE -8 /* JSON object does not match the required type */