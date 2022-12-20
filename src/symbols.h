static const char DOT[] = ".";
static const char SLASH[] = "/";
static const char BACKSLASH[] = "\\";
static const char SPACE[] = " ";
static const char COLON[] = ":";
static const char QUOTATION_MARK[] = "\"";
static const char AND[] = "&";
static const char EQUAL[] = "=";
static const char LF[] = "\n";
static const char COMMA[] = ",";
static const char HASHTAG[] = "#";
static const char GREATER_THAN[] = ">";

#ifdef _WIN32
	#define PATH_SEPARATOR "\\"
#else
	#define PATH_SEPARATOR "/"
#endif

static const char HTTP_HEADER_SEPARATOR[] = ": ";
static const char HTTPS_SCHEME[] = "https://";
static const char FILE_SCHEME[] = "file://";

static const char M3U8_FILE_EXTENSION[] = "m3u8";
static const char MP4_FILE_EXTENSION[] = "mp4";
static const char AAC_FILE_EXTENSION[] = "aac";
static const char TS_FILE_EXTENSION[] = "ts";
static const char KEY_FILE_EXTENSION[] = "key";
static const char HTML_FILE_EXTENSION[] = "html";
static const char JSON_FILE_EXTENSION[] = "json";
static const char PDF_FILE_EXTENSION[] = "pdf";

static const char HTML_HEADER_START[] = 
	"<!DOCTYPE html>"
	"<html lang=\"pt-BR\">"
	"<head>"
	"<title>Hotmart content</title>"
	"<meta charset=\"utf-8\">"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
	"</head>"
	"<body>";

static const char HTML_HEADER_END[] = 
	"</body>"
	"</html>";

static const char HTML_UL_START[] = "<ul>";
static const char HTML_UL_END[] = "</ul>";
static const char HTML_LI_START[] = "<li>";
static const char HTML_LI_END[] = "</li>";

static const char HTML_A_START[] = "<a";
static const char HTML_A_END[] = "</a>";
static const char HTML_HREF_ATTRIBUTE[] = "href";
