#include <string.h>

#include <curl/curl.h>

#include "curl.h"
#include "utils.h"
#include "symbols.h"
#include "fstream.h"
#include "errors.h"

static const char CA_CERT_FILENAME[] = 
	PATH_SEPARATOR
	"etc"
	PATH_SEPARATOR
	"tls"
	PATH_SEPARATOR
	"cert.pem";

static const char HTTP_DEFAULT_USER_AGENT[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36";

static char CURL_ERROR_MESSAGE[CURL_ERROR_SIZE] = {'\0'};

static CURL* curl_easy_global = NULL;
static CURLM* curl_multi_global = NULL;
static struct curl_blob curl_blob_global = {0};

static void globals_destroy(void) {
	
	curl_multi_cleanup(curl_multi_global);
	curl_multi_global = NULL;
	
	curl_easy_cleanup(curl_easy_global);
	curl_easy_global = NULL;
	
	if (curl_blob_global.data != NULL) {
		free(curl_blob_global.data);
		curl_blob_global.data = NULL;
		
		curl_blob_global.len = 0;
	}
	
}

static void globals_initialize(void) {
	
	if (!(curl_easy_global == NULL && curl_multi_global == NULL)) {
		return;
	}
	
	curl_global_init(CURL_GLOBAL_ALL);
	
	atexit(globals_destroy);
	
}

static int curl_set_options(CURL* handle) {
	
	curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, 30L);
	curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 15L);
	//curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, HTTP_DEFAULT_USER_AGENT);
	curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	if (curl_blob_global.data == NULL) {
		char app_filename[PATH_MAX];
		get_app_filename(app_filename, sizeof(app_filename));
		
		char app_root_directory[PATH_MAX];
		get_parent_directory(app_filename, app_root_directory, 2);
		
		char ca_bundle[strlen(app_root_directory) + strlen(CA_CERT_FILENAME) + 1];
		strcpy(ca_bundle, app_root_directory);
		strcat(ca_bundle, CA_CERT_FILENAME);
			
		const char* const certificate_paths[] = {
			// Debian, Ubuntu, Arch: maintained by update-ca-certificates, SUSE, Gentoo
			// NetBSD (security/mozilla-rootcerts)
			// SLES10/SLES11, https://golang.org/issue/12139
			"/etc/ssl/certs/ca-certificates.crt",
			// OpenSUSE
			"/etc/ssl/ca-bundle.pem",
			// Red Hat 5+, Fedora, Centos
			"/etc/pki/tls/certs/ca-bundle.crt",
			// Red Hat 4
			"/usr/share/ssl/certs/ca-bundle.crt",
			// FreeBSD (security/ca-root-nss package)
			"/usr/local/share/certs/ca-root-nss.crt",
			// CentOS/RHEL 7
			"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
			// OpenBSD, FreeBSD (optional symlink)
			"/etc/ssl/cert.pem",
			// Android (Termux)
			"/data/data/com.termux/files/usr/etc/tls/cert.pem",
			// MacOS
			"/System/Library/OpenSSL/certs/cert.pem",
			// SparkleC
			ca_bundle
		};
		
		for (size_t index = 0; index < sizeof(certificate_paths) / sizeof(*certificate_paths); index++) {
			const char* const filename = certificate_paths[index];
			
			const long long file_size = get_file_size(filename);
			
			if (file_size < 1) {
				continue;
			}
			
			struct FStream* const stream = fstream_open(filename, "r");
			
			if (stream == NULL) {
				return UERR_FSTREAM_FAILURE;
			}
			
			curl_blob_global.data = malloc((size_t) file_size);
			
			if (curl_blob_global.data == NULL) {
				return UERR_MEMORY_ALLOCATE_FAILURE;
			}
			
			const ssize_t rsize = fstream_read(stream, curl_blob_global.data, (size_t) file_size);
			
			if (rsize != (ssize_t) file_size) {
				return UERR_FSTREAM_FAILURE;
			}
			
			fstream_close(stream);
			
			curl_blob_global.len = (size_t) rsize;
			
			break;
		}
	}
	
	curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &curl_blob_global);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(handle, CURLOPT_CAINFO, NULL);
	curl_easy_setopt(handle, CURLOPT_CAPATH, NULL);
	curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, &curl_blob_global);
	
	return UERR_SUCCESS;
	
}

CURL* get_global_curl_easy(void) {
	
	globals_initialize();
	
	if (curl_easy_global != NULL) {
		return curl_easy_global;
	}
	
	curl_easy_global = curl_easy_new();
	
	if (curl_easy_global == NULL) {
		return NULL;
	}
	
	curl_easy_setopt(curl_easy_global, CURLOPT_ERRORBUFFER, CURL_ERROR_MESSAGE);
	
	return curl_easy_global;
	
}

CURL* curl_easy_new(void) {
	
	globals_initialize();
	
	CURL* handle = curl_easy_init();
	
	if (handle == NULL) {
		return NULL;
	}
	
	curl_set_options(handle);
	
	return handle;
	
}

CURLM* get_global_curl_multi(void) {
	
	globals_initialize();
	
	if (curl_multi_global != NULL) {
		return curl_multi_global;
	}
	
	curl_multi_global = curl_multi_init();
	
	if (curl_multi_global == NULL) {
		return NULL;
	}
	
	curl_multi_setopt(curl_multi_global, CURLMOPT_MAX_HOST_CONNECTIONS, 30L);
	curl_multi_setopt(curl_multi_global, CURLMOPT_MAX_TOTAL_CONNECTIONS, 30L);
	
	return curl_multi_global;
	
}

const char* get_global_curl_error(void) {
	
	return CURL_ERROR_MESSAGE;
	
}