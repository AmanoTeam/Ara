#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define A "SparkleC"

#ifdef _WIN32
	#include <stdio.h>
	#include <fcntl.h>
	#include <io.h>
	#include <locale.h>
#else
	#include <sys/resource.h>
#endif

#include <curl/curl.h>
#include <jansson.h>

#include "cleanup.h"
#include "errors.h"
#include "callbacks.h"
#include "m3u8.h"
#include "types.h"
#include "utils.h"
#include "symbols.h"
#include "curl.h"
#include "hotmart.h"
#include "fstream.h"
#include "providers.h"
#include "input.h"

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

static const char LOCAL_ACCOUNTS_FILENAME[] = "accounts.json";

static void curl_poll(struct Download* dqueue, const size_t dcount, size_t* total_done) {
	
	CURLM* curl_multi = get_global_curl_multi();
	
	int still_running = 1;
	
	curl_progress_cb(NULL, (const curl_off_t) dcount, (const curl_off_t) *total_done, 0, 0);
	
	while (still_running) {
		CURLMcode mc = curl_multi_perform(curl_multi, &still_running);
		
		if (still_running) {
			mc = curl_multi_poll(curl_multi, NULL, 0, 1000, NULL);
		}
			
		CURLMsg* msg = NULL;
		int msgs_left = 0;
		
		int should_continue = 0;
		
		while ((msg = curl_multi_info_read(curl_multi, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				struct Download* download = NULL;
				
				for (size_t index = 0; index < dcount; index++) {
					struct Download* const subdownload = &dqueue[index];
					
					if (subdownload->handle == msg->easy_handle) {
						download = subdownload;
						break;
					}
				}
				
				curl_multi_remove_handle(curl_multi, msg->easy_handle);
				
				if (msg->data.result == CURLE_OK) {
					curl_easy_cleanup(msg->easy_handle);
					fstream_close(download->stream);
					
					(*total_done)++;
					curl_progress_cb(NULL, (const curl_off_t) dcount, (const curl_off_t) *total_done, 0, 0);
				} else {
					fstream_seek(download->stream, 0, FSTREAM_SEEK_BEGIN);
					curl_multi_add_handle(curl_multi, msg->easy_handle);
					
					if (!should_continue) {
						should_continue = 1;
					}
				}
			}
		}
		
		if (should_continue) {
			still_running = 1;
			continue;
		}
		
		if (mc) {
			break;
		}
	}
	
}

static int m3u8_download(const char* const url, const char* const output) {
	
	CURLM* const curl_multi = get_global_curl_multi();
	CURL* const curl_easy = get_global_curl_easy();
	
	struct String string __attribute__((__cleanup__(string_free))) = {0};
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, url);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_string_cb);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &string);
	
	if (curl_easy_perform(curl_easy) != CURLE_OK) {
		return UERR_CURL_FAILURE;
	}
	
	struct Tags tags = {0};
	
	if (m3u8_parse(&tags, string.s) != UERR_SUCCESS) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return UERR_FAILURE;
	}
	
	int segment_number = 1;
	
	struct Download dl_queue[tags.offset];
	size_t dl_total = 0;
	size_t dl_done = 0;
	
	char playlist_filename[strlen(output) + strlen(DOT) + strlen(M3U8_FILE_EXTENSION) + 1];
	strcpy(playlist_filename, output);
	strcat(playlist_filename, DOT);
	strcat(playlist_filename, M3U8_FILE_EXTENSION);
	
	CURLU* cu __attribute__((__cleanup__(curlupp_free))) = curl_url();
	curl_url_set(cu, CURLUPART_URL, url, 0);
	
	for (size_t index = 0; index < tags.offset; index++) {
		struct Tag* const tag = &tags.items[index];
		
		if (tag->type == EXT_X_KEY) {
			struct Attribute* const attribute = attributes_get(&tag->attributes, "URI");
			
			curl_url_set(cu, CURLUPART_URL, url, 0);
			curl_url_set(cu, CURLUPART_URL, attribute->value, 0);
			
			char* key_url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
			curl_url_get(cu, CURLUPART_URL, &key_url, 0);
			
			char* filename = malloc(strlen(output) + strlen(DOT) + strlen(KEY_FILE_EXTENSION) + 1);
			strcpy(filename, output);
			strcat(filename, DOT);
			strcat(filename, KEY_FILE_EXTENSION);
			
			attribute_set_value(attribute, filename);
			
			CURL* handle = curl_easy_new();
			
			if (handle == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_URL, key_url);
			
			struct FStream* stream = fstream_open(filename, "wb");
			
			if (stream == NULL && errno == EMFILE) {
				curl_poll(dl_queue, dl_total, &dl_done);
				stream = fstream_open(filename, "wb");
			}
			
			if (stream == NULL) {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, error.message);
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
			curl_multi_add_handle(curl_multi, handle);
			
			struct Download download = {
				.handle = handle,
				.filename = filename,
				.stream = stream
			};
			
			dl_queue[dl_total++] = download;
			
			curl_url_set(cu, CURLUPART_URL, url, 0);
			curl_url_set(cu, CURLUPART_URL, tag->uri, 0);
			
			char* segment_url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
			curl_url_get(cu, CURLUPART_URL, &segment_url, 0);
			
			handle = curl_easy_new();
			
			if (handle == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar inicializar o cliente HTTP!\r\n");
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_URL, segment_url);
			
			char value[intlen(segment_number) + 1];
			snprintf(value, sizeof(value), "%i", segment_number);
			
			char* segment_filename = malloc(strlen(output) + strlen(DOT) + strlen(value) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
			strcpy(segment_filename, output);
			strcat(segment_filename, DOT);
			strcat(segment_filename, value);
			strcat(segment_filename, DOT);
			strcat(segment_filename, TS_FILE_EXTENSION);
			
			tag_set_uri(tag, segment_filename);
			
			stream = fstream_open(segment_filename, "wb");
			
			if (stream == NULL && errno == EMFILE) {
				curl_poll(dl_queue, dl_total, &dl_done);
				stream = fstream_open(segment_filename, "wb");
			}
			
			if (stream == NULL) {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, error.message);
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
			curl_multi_add_handle(curl_multi, handle);
			
			download = (struct Download) {
				.handle = handle,
				.filename = segment_filename,
				.stream = stream
			};
			
			dl_queue[dl_total++] = download;
			
			segment_number++;
		} else if (tag->type == EXTINF && tag->uri != NULL) {
			curl_url_set(cu, CURLUPART_URL, url, 0);
			curl_url_set(cu, CURLUPART_URL, tag->uri, 0);
			
			char* segment_url __attribute__((__cleanup__(curlcharpp_free))) = NULL;
			curl_url_get(cu, CURLUPART_URL, &segment_url, 0);
			
			char value[intlen(segment_number) + 1];
			snprintf(value, sizeof(value), "%i", segment_number);
			
			char* filename = malloc(strlen(output) + strlen(DOT) + strlen(value) + strlen(DOT) + strlen(TS_FILE_EXTENSION) + 1);
			strcpy(filename, output);
			strcat(filename, DOT);
			strcat(filename, value);
			strcat(filename, DOT);
			strcat(filename, TS_FILE_EXTENSION);
			
			tag_set_uri(tag, filename);
			
			CURL* handle = curl_easy_new();
			
			if (handle == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_URL, segment_url);
			
			struct FStream* stream = fstream_open(filename, "wb");
			
			if (stream == NULL && errno == EMFILE) {
				curl_poll(dl_queue, dl_total, &dl_done);
				stream = fstream_open(filename, "wb");
			}
			
			if (stream == NULL) {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, error.message);
				return UERR_FAILURE;
			}
			
			curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*) stream);
			curl_multi_add_handle(curl_multi, handle);
			
			struct Download download = {
				.handle = handle,
				.filename = filename,
				.stream = stream
			};
			
			dl_queue[dl_total++] = download;
			
			segment_number++;
		}
		
	}
	
	curl_poll(dl_queue, dl_total, &dl_done);
	
	printf("+ Exportando lista de reprodução M3U8 para '%s'\r\n", playlist_filename);
	 
	struct FStream* const stream = fstream_open(playlist_filename, "wb");
	
	if (stream == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", playlist_filename, error.message);
		return UERR_FAILURE;
	}
			
	const int ok = tags_dumpf(&tags, stream);
	
	fstream_close(stream);
	m3u8_free(&tags);
	
	if (!ok) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar a lista de reprodução para '%s': %s\r\n", playlist_filename, error.message);
		return UERR_FAILURE;
	}
	
	printf("+ Concatenando seguimentos de mídia baixados para um único arquivo em '%s'\r\n", output);
	
	const char* const command = "ffmpeg -nostdin -nostats -loglevel fatal -allowed_extensions ALL -i \"%s\" -c copy \"%s\"";
	
	const int size = snprintf(NULL, 0, command, playlist_filename, output);
	char shell_command[size + 1];
	snprintf(shell_command, sizeof(shell_command), command, playlist_filename, output);
	
	const int exit_code = execute_shell_command(shell_command);
	
	for (size_t index = 0; index < dl_total; index++) {
		struct Download* const download = &dl_queue[index];
		
		remove_file(download->filename);
		free(download->filename);
	}
	
	remove_file(playlist_filename);
	
	if (exit_code != 0) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar processar a mídia!\r\n");
		return UERR_FAILURE;
	}
	
	curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
	
	return UERR_SUCCESS;
	
}

#if defined(_WIN32) && defined(_UNICODE)
	#define main wmain
	int wmain(void);
#endif

int main(void) {
	
	#ifdef _WIN32
		_setmaxstdio(50);
		
		#ifdef _UNICODE
			_setmode(_fileno(stdout), _O_WTEXT);
			_setmode(_fileno(stderr), _O_WTEXT);
			_setmode(_fileno(stdin), _O_WTEXT);
			
			setlocale(LC_ALL, ".UTF8");
		#endif
	#else
		struct rlimit rlim = {0};
		getrlimit(RLIMIT_NOFILE, &rlim);
		
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_NOFILE, &rlim);
	#endif
	
	if (is_administrator()) {
		fprintf(stderr, "- Você não precisa e nem deve executar este programa com privilégios elevados!\r\n");
		return EXIT_FAILURE;
	}
	
	printf("+ Selecione o seu provedor de serviços:\r\n\r\n");
	
	for (size_t index = 0; index < PROVIDERS_NUM; index++) {
		const struct Provider provider = PROVIDERS[index];
		printf("%zu. \r\nNome: %s\r\nURL: %s\r\n\r\n", index + 1, provider.label, provider.url);
	}
	
	int value = input_integer(1, (int) PROVIDERS_NUM);
	
	const struct Provider provider = PROVIDERS[value - 1];
	const struct ProviderMethods methods = provider.methods;
	
	char* directory = get_configuration_directory();
	
	if (directory == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Não foi possível obter um diretório de configurações válido: %s\r\n", error.message);
		return EXIT_FAILURE;
	}
	
	char configuration_directory[strlen(directory) + strlen(A) + strlen(PATH_SEPARATOR) + strlen(provider.label) + 1];
	strcpy(configuration_directory, directory);
	strcat(configuration_directory, A);
	strcat(configuration_directory, PATH_SEPARATOR);
	strcat(configuration_directory, provider.label);
	
	free(directory);
	
	if (!directory_exists(configuration_directory)) {
		fprintf(stderr, "- Diretório de configurações não encontrado, criando-o\r\n");
		
		if (!create_directory(configuration_directory)) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", configuration_directory, error.message);
			return EXIT_FAILURE;
		}
	}
	
	directory = get_temporary_directory();
	
	if (directory == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Não foi possível obter um diretório temporário válido: %s\r\n", error.message);
		return EXIT_FAILURE;
	}
	
	char temporary_directory[strlen(directory) + strlen(PATH_SEPARATOR) + strlen(A) + 1];
	strcpy(temporary_directory, directory);
	strcat(temporary_directory, PATH_SEPARATOR);
	strcat(temporary_directory, A);
	
	free(directory);
	
	if (!directory_exists(temporary_directory)) {
		fprintf(stderr, "- Diretório temporário não encontrado, criando-o\r\n");
		
		if (!create_directory(temporary_directory)) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", temporary_directory, error.message);
			return EXIT_FAILURE;
		}
	}
	
	char accounts_file[strlen(configuration_directory) + strlen(PATH_SEPARATOR) + strlen(LOCAL_ACCOUNTS_FILENAME) + 1];
	strcpy(accounts_file, configuration_directory);
	strcat(accounts_file, PATH_SEPARATOR);
	strcat(accounts_file, LOCAL_ACCOUNTS_FILENAME);
	
	CURL* curl_easy = get_global_curl_easy();
	
	if (curl_easy == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar inicializar o cliente HTTP!\r\n");
		return EXIT_FAILURE;
	}
	
	CURLM* curl_multi = get_global_curl_multi();
	
	if (curl_multi == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar inicializar o cliente HTTP!\r\n");
		return EXIT_FAILURE;
	}
	
	struct Credentials credentials = {0};
	
	if (file_exists(accounts_file)) {
		struct FStream* const stream = fstream_open(accounts_file, "r");
		
		if (stream == NULL) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar abrir o arquivo em '%s': %s\r\n", accounts_file, error.message);
			return EXIT_FAILURE;
		}
		
		json_auto_t* tree = json_load_callback(json_load_cb, (void*) stream, 0, NULL);
		
		fstream_close(stream);
		
		if (tree == NULL || !json_is_array(tree)) {
			fprintf(stderr, "- O arquivo de credenciais localizado em '%s' possui uma sintaxe inválida ou não reconhecida!\r\n", accounts_file);
			return EXIT_FAILURE;
		}
		
		const size_t total_items = json_array_size(tree);
		
		if (total_items < 1) {
			fprintf(stderr, "- O arquivo de credenciais localizado em '%s' não possui credenciais salvas!\r\n", accounts_file);
			return EXIT_FAILURE;
		}
		
		struct Credentials items[total_items];
		
		size_t index = 0;
		const json_t* item = NULL;
		
		printf("+ Como você deseja acessar este serviço?\r\n\r\n");
		printf("0.\r\nAdicionar e usar nova conta\r\n\r\n");
		
		json_array_foreach(tree, index, item) {
			json_t* subobj = json_object_get(item, "username");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- O arquivo de configurações localizado em '%s' possui um formato inválido!\r\n", accounts_file);
				return EXIT_FAILURE;
			}
			
			const char* const username = json_string_value(subobj);
			
			subobj = json_object_get(item, "access_token");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- O arquivo de configurações localizado em '%s' possui um formato inválido!\r\n", accounts_file);
				return EXIT_FAILURE;
			}
			
			const char* const access_token = json_string_value(subobj);
			
			struct Credentials credentials = {
				.access_token = malloc(strlen(access_token) + 1)
			};
			
			if (credentials.access_token == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
				return EXIT_FAILURE;
			}
			
			strcpy(credentials.access_token, access_token);
			
			items[index] = credentials;
			
			printf("%zu. \r\nAcessar usando a conta: '%s'\r\n\r\n", index + 1, username);
		}
		
		const int value = input_integer(0, (int) total_items);
		
		if (value == 0) {
			char username[MAX_INPUT_SIZE] = {'\0'};
			input("> Insira seu usuário: ", username);
			
			char password[MAX_INPUT_SIZE] = {'\0'};
			input("> Insira sua senha: ", password);
			
			const int code = (*methods.authorize)(username, password, &credentials);
			
			switch (code) {
				case UERR_SUCCESS:
					break;
				case UERR_CURL_FAILURE:
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
					return EXIT_FAILURE;
				default:
					fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
					return EXIT_FAILURE;
			}
			
			struct FStream* const stream = fstream_open(accounts_file, "wb");
			
			if (stream == NULL) {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", accounts_file, error.message);
				return EXIT_FAILURE;
			}
			
			json_auto_t* subtree = json_object();
			json_object_set_new(subtree, "username", json_string(credentials.username));
			json_object_set_new(subtree, "access_token", json_string(credentials.access_token));
			
			json_array_append(tree, subtree);
			
			const int rcode = json_dump_callback(tree, json_dump_cb, (void*) stream, JSON_COMPACT);
			
			if (rcode != 0) {
				const struct SystemError error = get_system_error();
				
				fstream_close(stream);
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar o arquivo de credenciais para '%s': %s\r\n", accounts_file, error.message);
				return EXIT_FAILURE;
			}
			
			fstream_close(stream);
		} else {
			credentials = items[value - 1];
		}
	} else {
		char username[MAX_INPUT_SIZE] = {'\0'};
		input("> Insira seu usuário: ", username);
		
		char password[MAX_INPUT_SIZE] = {'\0'};
		input("> Insira sua senha: ", password);
		
		const int code = (*methods.authorize)(username, password, &credentials);
		
		if (code != UERR_SUCCESS) {
			fprintf(stderr, "- Não foi possível realizar a autenticação: %s\r\n", strurr(code));
			return EXIT_FAILURE;
		}
		
		struct FStream* const stream = fstream_open(accounts_file, "wb");
		
		if (stream == NULL) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", accounts_file, error.message);
			return EXIT_FAILURE;
		}
		
		json_auto_t* tree = json_array();
		json_auto_t* obj = json_object();
		
		if (tree == NULL || obj == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
			return EXIT_FAILURE;
		}
		
		json_object_set_new(obj, "username", json_string(credentials.username));
		json_object_set_new(obj, "access_token", json_string(credentials.access_token));
		
		json_array_append(tree, obj);
		
		const int rcode = json_dump_callback(tree, json_dump_cb, (void*) stream, JSON_COMPACT);
		
		if (rcode != 0) {
			const struct SystemError error = get_system_error();
			
			fstream_close(stream);
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar o arquivo de credenciais para '%s': %s\r\n", accounts_file, error.message);
			return EXIT_FAILURE;
		}
		
		fstream_close(stream);
	}
	
	printf("+ Obtendo lista de conteúdos disponíveis\r\n");
	
	struct Resources resources = {0};
	
	const int code = (*methods.get_resources)(&credentials, &resources);
	
	switch (code) {
		case UERR_SUCCESS:
			break;
		case UERR_PROVIDER_SESSION_EXPIRED:
			fprintf(stderr, "- Sua sessão expirou ou foi revogada, refaça o login!\r\n");
			return EXIT_FAILURE;
		case UERR_CURL_FAILURE:
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
			return EXIT_FAILURE;
		default:
			fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
			return EXIT_FAILURE;
	}
	
	if (resources.offset < 1) {
		fprintf(stderr, "- Não foram encontrados conteúdos disponíveis para baixar!\r\n");
		return EXIT_FAILURE;
	}
	
	printf("+ Selecione o que deseja baixar:\r\n\r\n");
	
	for (size_t index = 0; index < resources.offset; index++) {
		const struct Resource* const resource = &resources.items[index];
		printf("%zu. \r\nNome: %s\r\nQualificação: %s\r\nURL: %s\r\n\r\n", index + 1, resource->name, resource->qualification.name == NULL ? "N/A" : resource->qualification.name, resource->url);
	}
	
	struct Resource download_queue[resources.offset];
	size_t queue_count = 0;
	
	while (1) {
		if (queue_count > 0) {
			queue_count = 0;
		}
		
		char query[MAX_INPUT_SIZE] = {'\0'};
		input("> Digite sua escolha: ", query);
		
		const char* start = query;
		int err = 0;
		
		for (size_t index = 0; index < strlen(query) + 1; index++) {
			const char* const ch = &query[index];
			
			if (!(*ch == *COMMA || (*ch == '\0' && index > 0))) {
				continue;
			}
			
			const size_t size = (size_t) (ch - start);
			
			if (size < 1) {
				fprintf(stderr, "- Não podem haver valores vazios dentre as escolhas!\r\n");
				err = 1;
				break;
			}
			
			char value[size + 1];
			memcpy(value, start, size);
			value[size] = '\0';
			
			const char* hyphen = strstr(value, HYPHEN);
			
			if (hyphen == NULL) {
				 if (!isnumeric(value)) {
					fprintf(stderr, "- O valor inserido é inválido ou não reconhecido!\r\n");
					err = 1;
					break;
				}
				
				const size_t position = (size_t) atoi(value);
				
				if (position < 1) {
					fprintf(stderr, "- O valor mínimo de uma escolha deve ser >=1!\r\n");
					err = 1;
					break;
				}
				
				if (position > resources.offset) {
					fprintf(stderr, "- O valor máximo de uma escolha deve ser <=%zu!\r\n", resources.offset);
					err = 1;
					break;
				}
				
				struct Resource resource = resources.items[position - 1];
				
				for (size_t index = 0; index < queue_count; index++) {
					const struct Resource subresource = download_queue[index];
					
					if (subresource.id == resource.id) {
						fprintf(stderr, "- Não podem haver conteúdos duplicados dentre as escolhas!\r\n");
						err = 1;
						break;
					}
				}
				
				if (err) {
					break;
				}
				
				download_queue[queue_count++] = resource;
			} else {
				size_t size = (size_t) (hyphen - value);
				
				if (size < 1) {
					fprintf(stderr, "- O valor mínimo é obrigatório para intervalos de seleção!\r\n");
					err = 1;
					break;
				}
				
				char mins[size + 1];
				memcpy(mins, value, size);
				mins[size] = '\0';
				
				const size_t min = (size_t) atoi(mins);
				
				if (min < 1) {
					fprintf(stderr, "- O valor mínimo para este intervalo deve ser >=1!\r\n");
					err = 1;
					break;
				}
				
				const char* const end = value + (sizeof(value) - 1);
				hyphen++;
				
				size = (size_t) (end - hyphen);
				
				if (size < 1) {
					fprintf(stderr, "- O valor máximo é obrigatório para intervalos de seleção!\r\n");
					err = 1;
					break;
				}
				
				char maxs[size + 1];
				memcpy(maxs, hyphen, size);
				maxs[size] = '\0';
				
				const size_t max = (size_t) atoi(maxs);
				
				if (max > resources.offset) {
					fprintf(stderr, "- O valor máximo para este intervalo deve ser <=%zu!\r\n", resources.offset);
					err = 1;
					break;
				}
				
				for (size_t index = min; index < (max + 1); index++) {
					struct Resource resource = resources.items[index - 1];
					
					for (size_t index = 0; index < queue_count; index++) {
						const struct Resource subresource = download_queue[index];
						
						if (subresource.id == resource.id) {
							fprintf(stderr, "- Não podem haver conteúdos duplicados dentre as escolhas!\r\n");
							err = 1;
							break;
						}
					}
					
					if (err) {
						break;
					}
					
					download_queue[queue_count++] = resource;
				}
			}
			
			start = (ch + 1);
		}
		
		if (!err) {
			break;
		}
	}
	
	if (queue_count > 1) {
		printf("- %zu conteúdos foram enfileirados para serem baixados\r\n", queue_count);
	}
	
	int kof = 0;
	
	while (1) {
		char answer[MAX_INPUT_SIZE] = {'\0'};
		input("> Manter o nome original de arquivos e diretórios? (S/n) ", answer);
		
		switch (*answer) {
			case 's':
			case 'y':
			case 'S':
			case 'Y':
				kof = 1;
				break;
			case 'n':
			case 'N':
				kof = 0;
				break;
			default:
				fprintf(stderr, "- O valor inserido é inválido ou não reconhecido!\r\n");
				continue;
		}
		
		break;
	}
	
	fclose(stdin);
	
	char* cwd = get_current_directory();
	
	if (cwd == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter o diretório de trabalho atual: %s\r\n", error.message);
		return EXIT_FAILURE;
	}
	
	const int has_trailing_sep = (strlen(cwd) > 0 && *(strchr(cwd, '\0') - 1) == *PATH_SEPARATOR);
	
	for (size_t index = 0; index < queue_count; index++) {
		struct Resource* const resource = &download_queue[index];
		
		printf("+ Obtendo lista de módulos do produto '%s'\r\n", resource->name);
		
		const int code = (*methods.get_modules)(&credentials, resource);
		
		switch (code) {
			case UERR_SUCCESS:
				break;
			case UERR_CURL_FAILURE:
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
				return EXIT_FAILURE;
			default:
				fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
				return EXIT_FAILURE;
		}
		
		resource->path = malloc(strlen(cwd) + (has_trailing_sep ? 0 : strlen(PATH_SEPARATOR)) + (resource->qualification.id == NULL ? 0 : (kof ? strlen(resource->qualification.dirname) : strlen(resource->qualification.short_dirname)) + strlen(PATH_SEPARATOR)) + (kof ? strlen(resource->dirname) : strlen(resource->short_dirname)) + 1);
		
		if (resource->path == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
			return EXIT_FAILURE;
		}
		
		strcpy(resource->path, cwd);
		
		if (!has_trailing_sep) {
			strcat(resource->path, PATH_SEPARATOR);
		}
		
		if (resource->qualification.id != NULL) {
			strcat(resource->path, kof ? resource->qualification.dirname : resource->qualification.short_dirname);
			strcat(resource->path, PATH_SEPARATOR);
		}
		
		strcat(resource->path, kof ? resource->dirname : resource->short_dirname);
		
		if (!directory_exists(resource->path)) {
			fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", resource->path);
			
			if (!create_directory(resource->path)) {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", resource->path, error.message);
				return EXIT_FAILURE;
			}
		}
		
		for (size_t index = 0; index < resource->modules.offset; index++) {
			struct Module* module = &resource->modules.items[index];
			
			printf("+ Obtendo lista de aulas do módulo '%s'\r\n", module->name);
			
			if (module->is_locked) {
				fprintf(stderr, "- Módulo inacessível, pulando para o próximo\r\n");
				continue;
			}
			
			const int code = (*methods.get_module)(&credentials, resource, module);
			
			switch (code) {
				case UERR_SUCCESS:
					break;
				case UERR_NOT_IMPLEMENTED:
					fprintf(stderr, "- As informações sobre este módulo já foram obtidas, pulando etapa\r\n");
					break;
				case UERR_CURL_FAILURE:
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
					return EXIT_FAILURE;
				default:
					fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
					return EXIT_FAILURE;
			}
			
			printf("+ Verificando estado do módulo '%s'\r\n", module->name);
			
			module->path = malloc(strlen(resource->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(module->dirname) : strlen(module->short_dirname)) + 1);
			
			if (module->path == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
				return EXIT_FAILURE;
			}
			
			strcpy(module->path, resource->path);
			strcat(module->path, PATH_SEPARATOR);
			strcat(module->path, kof ? module->dirname : module->short_dirname);
			
			if (!directory_exists(module->path)) {
				fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", module->path);
				
				if (!create_directory(module->path)) {
					const struct SystemError error = get_system_error();
					
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", module->path, error.message);
					return EXIT_FAILURE;
				}
			}
			
			curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 0L);
			curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
			curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
			
			for (size_t index = 0; index < module->attachments.offset; index++) {
				struct Attachment* const attachment = &module->attachments.items[index];
				
				attachment->path = malloc(strlen(module->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(attachment->filename) : strlen(attachment->short_filename)) + 1);
				
				if (attachment->path == NULL) {
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
					return EXIT_FAILURE;
				}
			
				strcpy(attachment->path, module->path);
				strcat(attachment->path, PATH_SEPARATOR);
				strcat(attachment->path, kof ? attachment->filename : attachment->short_filename);
				
				if (!file_exists(attachment->path)) {
					char download_location[strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(attachment->short_filename) + 1];
					strcpy(download_location, temporary_directory);
					strcat(download_location, PATH_SEPARATOR);
					strcat(download_location, attachment->short_filename);
					
					fprintf(stderr, "- O arquivo '%s' não existe, ele será baixado\r\n", attachment->path);
					printf("+ Baixando de '%s' para '%s'\r\n", attachment->url, download_location);
					
					struct FStream* const stream = fstream_open(download_location, "wb");
					
					if (stream == NULL) {
						const struct SystemError error = get_system_error();
						
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", download_location, error.message);
						return EXIT_FAILURE;
					}
					
					curl_easy_setopt(curl_easy, CURLOPT_URL, attachment->url);
					curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, (void*) stream);
					
					const CURLcode code = curl_easy_perform(curl_easy);
					
					printf("\n");
					
					fstream_close(stream);
					
					if (code != CURLE_OK) {
						remove_file(download_location);
						
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
						return EXIT_FAILURE;
					}
					
					printf("+ Movendo arquivo de '%s' para '%s'\r\n", download_location, attachment->path);
					
					if (!move_file(download_location, attachment->path)) {
						const struct SystemError error = get_system_error();
						
						remove_file(download_location);
						
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar mover o arquivo de '%s' para '%s': %s\r\n", download_location, attachment->path, error.message);
						return EXIT_FAILURE;
					}
				}
			}
			
			curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
			curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
			curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
			curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 1L);
			curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 60L);
			curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
			curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
			
			printf("+ Obtendo lista de páginas do módulo '%s'\r\n", module->name);
			
			for (size_t index = 0; index < module->pages.offset; index++) {
				struct Page* page = &module->pages.items[index];
				
				printf("+ Obtendo informações sobre a aula '%s'\r\n", page->name);
				
				if (page->is_locked) {
					fprintf(stderr, "- Aula inacessível, pulando para a próxima\r\n");
					continue;
				}
				
				const int code = (*methods.get_page)(&credentials, resource, page);
				
				switch (code) {
					case UERR_SUCCESS:
						break;
					case UERR_NOT_IMPLEMENTED:
						fprintf(stderr, "- As informações sobre esta aula já foram obtidas, pulando etapa\r\n");
						break;
					case UERR_CURL_FAILURE:
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
						return EXIT_FAILURE;
					default:
						fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
						return EXIT_FAILURE;
				}
				
				printf("+ Verificando estado da aula '%s'\r\n", page->name);
				
				page->path = malloc(strlen(module->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(page->dirname) : strlen(page->short_dirname)) + 1);
				
				if (page->path == NULL) {
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
					return EXIT_FAILURE;
				}
				
				strcpy(page->path, module->path);
				strcat(page->path, PATH_SEPARATOR);
				strcat(page->path, kof ? page->dirname : page->short_dirname);
				
				if (!directory_exists(page->path)) {
					fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", page->path);
					
					if (!create_directory(page->path)) {
						const struct SystemError error = get_system_error();
						
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", page->path, error.message);
						return EXIT_FAILURE;
					}
				}
				
				if (page->document.content != NULL) {
					page->document.path = malloc(strlen(page->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(page->document.filename) : strlen(page->document.short_filename)) + 1);
					
					if (page->document.path == NULL) {
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
						return EXIT_FAILURE;
					}
					
					strcpy(page->document.path, page->path);
					strcat(page->document.path, PATH_SEPARATOR);
					strcat(page->document.path, kof ? page->document.filename : page->document.short_filename);
					
					if (!file_exists(page->document.path)) {
						fprintf(stderr, "- O arquivo '%s' não existe, salvando-o\r\n", page->document.path);
						
						struct FStream* const stream = fstream_open(page->document.path, "wb");
						
						if (stream == NULL) {
							const struct SystemError error = get_system_error();
							
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", page->document.path, error.message);
							return EXIT_FAILURE;
						}
						
						const int status = fstream_write(stream, page->document.content, strlen(page->document.content));
						
						if (!status) {
							const struct SystemError error = get_system_error();
							
							fstream_close(stream);
							remove_file(page->document.path);
							
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar salvar o documento em '%s': %s\r\n", page->document.path, error.message);
							return EXIT_FAILURE;
						}
						
						fstream_close(stream);
					}
				}
				
				for (size_t index = 0; index < page->medias.offset; index++) {
					struct Media* media = &page->medias.items[index];
					
					char* media_filename = NULL;
					
					if (media->audio.url != NULL && media->video.url == NULL) {
						media_filename = malloc(strlen(page->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(media->audio.filename) : strlen(media->audio.short_filename)) + 1);
						
						if (media_filename == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
							return EXIT_FAILURE;
						}
						
						strcpy(media_filename, page->path);
						strcat(media_filename, PATH_SEPARATOR);
						strcat(media_filename, kof ? media->audio.filename : media->audio.short_filename);
					}
					
					if (media->video.url != NULL) {
						media_filename = malloc(strlen(page->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(media->video.filename) : strlen(media->video.short_filename)) + 1);
						
						if (media_filename == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
							return EXIT_FAILURE;
						}
						
						strcpy(media_filename, page->path);
						strcat(media_filename, PATH_SEPARATOR);
						strcat(media_filename, kof ? media->video.filename : media->video.short_filename);
					}
					
					media->path = media_filename;
					
					if (!file_exists(media_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, baixando-o\r\n", media_filename);
						
						char* audio_path __attribute__((__cleanup__(charpp_free))) = NULL;
						char* video_path __attribute__((__cleanup__(charpp_free))) = NULL;
						
						if (media->audio.url != NULL) {
							audio_path = malloc(strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(media->audio.short_filename) + 1);
							
							if (audio_path == NULL) {
								fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
								return EXIT_FAILURE;
							}
							
							strcpy(audio_path, temporary_directory);
							strcat(audio_path, PATH_SEPARATOR);
							strcat(audio_path, media->audio.short_filename);
							
							switch (media->type) {
								case MEDIA_M3U8: {
									printf("+ Baixando seguimentos de mídia de '%s' para '%s'\r\n", media->audio.url, temporary_directory);
									
									if (m3u8_download(media->audio.url, audio_path) != UERR_SUCCESS) {
										return EXIT_FAILURE;
									}
									
									break;
								}
								case MEDIA_SINGLE: {
									printf("+ Baixando arquivo de mídia de '%s' para '%s'\r\n", media->audio.url, audio_path);
									
									struct FStream* const stream = fstream_open(audio_path, "wb");
									
									if (stream == NULL) {
										const struct SystemError error = get_system_error();
										
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n",  audio_path, error.message);
										return EXIT_FAILURE;
									}
									
									curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 0L);
									curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
									curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 0L);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, (void*) stream);
									curl_easy_setopt(curl_easy, CURLOPT_URL, media->audio.url);
									curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
									
									const CURLcode code = curl_easy_perform(curl_easy);
									
									printf("\n");
									
									fstream_close(stream);
									
									curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 1L);
									curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 60L);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
									
									if (code != CURLE_OK) {
										remove_file(audio_path);
										
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
										return EXIT_FAILURE;
									}
									
									break;
								}
							}
						}
						
						if (media->video.url != NULL) {
							video_path = malloc(strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(media->video.short_filename) + 1);
							
							if (video_path == NULL) {
								fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
								return EXIT_FAILURE;
							}
							
							strcpy(video_path, temporary_directory);
							strcat(video_path, PATH_SEPARATOR);
							strcat(video_path, media->video.short_filename);
							
							switch (media->type) {
								case MEDIA_M3U8: {
									printf("+ Baixando seguimentos de mídia de '%s' para '%s'\r\n", media->video.url, temporary_directory);
									
									if (m3u8_download(media->video.url, video_path) != UERR_SUCCESS) {
										return EXIT_FAILURE;
									}
									
									break;
								}
								case MEDIA_SINGLE: {
									printf("+ Baixando arquivo de mídia de '%s' para '%s'\r\n", media->video.url, video_path);
									
									struct FStream* const stream = fstream_open(video_path, "wb");
									
									if (stream == NULL) {
										const struct SystemError error = get_system_error();
										
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n",  video_path, error.message);
										return EXIT_FAILURE;
									}
									
									curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 0L);
									curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
									curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 0L);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, (void*) stream);
									curl_easy_setopt(curl_easy, CURLOPT_URL, media->video.url);
									curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
									
									const CURLcode code = curl_easy_perform(curl_easy);
									
									printf("\n");
									
									fstream_close(stream);
									
									curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 1L);
									curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 60L);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
									curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
									
									if (code != CURLE_OK) {
										remove_file(video_path);
										
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
										return EXIT_FAILURE;
									}
									
									break;
								}
							}
						}
						
						if (audio_path != NULL && video_path != NULL) {
							const char* const file_extension = get_file_extension(video_path);
							
							char temporary_file[strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(media->video.id) + strlen(media->audio.id) + strlen(DOT) + strlen(file_extension) + 1];
							strcpy(temporary_file, temporary_directory);
							strcat(temporary_file, PATH_SEPARATOR);
							strcat(temporary_file, media->video.id);
							strcat(temporary_file, media->audio.id);
							strcat(temporary_file, DOT);
							strcat(temporary_file, file_extension);
							
							const char* const command = "ffmpeg -nostdin -nostats -loglevel fatal -i \"%s\" -i \"%s\" -c copy -movflags +faststart -map_metadata -1 -map 0:v:0 -map 1:a:0 \"%s\"";
							
							const int size = snprintf(NULL, 0, command, video_path, audio_path, temporary_file);
							char shell_command[size + 1];
							snprintf(shell_command, sizeof(shell_command), command, video_path, audio_path, temporary_file);
							
							printf("+ Copiando canais de áudio e vídeo para uma única mídia em '%s'\r\n", temporary_file);
							
							const int exit_code = execute_shell_command(shell_command);
							
							remove_file(audio_path);
							remove_file(video_path);
							
							if (exit_code != 0) {
								fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar processar a mídia!\r\n");
								return EXIT_FAILURE;
							}
							
							printf("+ Movendo arquivo de mídia de '%s' para '%s'\r\n", temporary_file, media_filename);
							
							if (!move_file(temporary_file, media_filename)) {
								const struct SystemError error = get_system_error();
								
								fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar mover o arquivo de '%s' para '%s': %s\r\n", temporary_file, media_filename, error.message);
								return EXIT_FAILURE;
							}
						} else {
							const char* const input = audio_path == NULL ? video_path : audio_path;
							
							const char* const command = "ffmpeg -nostdin -nostats -loglevel fatal -i \"%s\" -c copy -movflags +faststart -map_metadata -1 \"%s\"";
							
							const int size = snprintf(NULL, 0, command, input, media_filename);
							char shell_command[size + 1];
							snprintf(shell_command, sizeof(shell_command), command, input, media_filename);
							
							printf("+ Movendo arquivo de mídia de '%s' para '%s'\r\n", input, media_filename);
							
							const int exit_code = execute_shell_command(shell_command);
							
							remove_file(input);
							
							if (exit_code != 0) {
								fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar processar a mídia!\r\n");
								return EXIT_FAILURE;
							}
						}
					}
				}
				
				curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 0L);
				curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
				curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 0L);
				curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
				
				for (size_t index = 0; index < page->attachments.offset; index++) {
					struct Attachment* const attachment = &page->attachments.items[index];
					
					attachment->path = malloc(strlen(page->path) + strlen(PATH_SEPARATOR) + (kof ? strlen(attachment->filename) : strlen(attachment->short_filename)) + 1);
					
					if (attachment->path == NULL) {
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
						return EXIT_FAILURE;
					}
					
					strcpy(attachment->path, page->path);
					strcat(attachment->path, PATH_SEPARATOR);
					strcat(attachment->path, kof ? attachment->filename : attachment->short_filename);
					
					if (!file_exists(attachment->path)) {
						char download_location[strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(attachment->short_filename) + 1];
						strcpy(download_location, temporary_directory);
						strcat(download_location, PATH_SEPARATOR);
						strcat(download_location, attachment->short_filename);
						
						fprintf(stderr, "- O arquivo '%s' não existe, ele será baixado\r\n", attachment->path);
						printf("+ Baixando de '%s' para '%s'\r\n", attachment->url, download_location);
						
						struct FStream* const stream = fstream_open(download_location, "wb");
						
						if (stream == NULL) {
							const struct SystemError error = get_system_error();
							
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", attachment->path, error.message);
							return EXIT_FAILURE;
						}
						
						curl_easy_setopt(curl_easy, CURLOPT_URL, attachment->url);
						curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, (void*) stream);
						
						const CURLcode code = curl_easy_perform(curl_easy);
						
						printf("\n");
						
						fstream_close(stream);
						
						if (code != CURLE_OK) {
							remove_file(download_location);
							
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor HTTP: %s\r\n", get_global_curl_error());
							return EXIT_FAILURE;
						}
						
						printf("+ Movendo arquivo de '%s' para '%s'\r\n", download_location, attachment->path);
						
						if (!move_file(download_location, attachment->path)) {
							const struct SystemError error = get_system_error();
							
							remove_file(download_location);
							
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar mover o arquivo de '%s' para '%s': %s\r\n", download_location, attachment->path, error.message);
							return EXIT_FAILURE;
						}
					}
				}
				
				curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
				curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
				curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
				curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 1L);
				curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 60L);
				curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
				curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
			}
		}
	}
	
	for (size_t index = 0; index < queue_count; index++) {
		struct Resource* const resource = &download_queue[index];
		
		json_t* jresource = json_object();
		json_object_set_new(jresource, "type", json_string("Resource"));
		json_object_set_new(jresource, "id", json_string(resource->id));
		json_object_set_new(jresource, "name", json_string(resource->name));
		json_object_set_new(jresource, "dirname", json_string(resource->dirname));
		json_object_set_new(jresource, "short_dirname", json_string(resource->short_dirname));
		json_object_set_new(jresource, "url", json_string(resource->url));
		
		if (resource->qualification.id == NULL) {
			json_object_set_new(jresource, "qualification", json_null());
		} else {
			json_t* jqualification = json_object();
			
			json_object_set_new(jqualification, "id", json_string(resource->qualification.id));
			json_object_set_new(jqualification, "name", json_string(resource->qualification.name));
			json_object_set_new(jqualification, "dirname", json_string(resource->qualification.dirname));
			json_object_set_new(jqualification, "short_dirname", json_string(resource->qualification.short_dirname));
			
			json_object_set_new(jresource, "qualification", jqualification);
		}
		
		if (resource->modules.offset < 1) {
			json_object_set_new(jresource, "modules", json_null());
		} else {
			json_t* jmodules = json_object();
			json_object_set_new(jmodules, "type", json_string("Modules"));
			json_object_set_new(jmodules, "offset", json_integer(resource->modules.offset));
			json_t* jitems = json_array();
			
			for (size_t index = 0; index < resource->modules.offset; index++) {
				json_t* jmodule = json_object();
				
				struct Module* const module = &resource->modules.items[index];
				
				json_object_set_new(jmodule, "type", json_string("Module"));
				json_object_set_new(jmodule, "id", json_string(module->id));
				json_object_set_new(jmodule, "name", json_string(module->name));
				json_object_set_new(jmodule, "dirname", json_string(module->dirname));
				json_object_set_new(jmodule, "short_dirname", json_string(module->short_dirname));
				json_object_set_new(jmodule, "is_locked", module->is_locked ? json_true() : json_false());
				
				if (module->attachments.offset < 1) {
					json_object_set_new(jmodule, "attachments", json_null());
				} else {
					json_t* jattachments = json_object();
					json_object_set_new(jattachments, "type", json_string("Attachments"));
					json_object_set_new(jattachments, "offset", json_integer(module->attachments.offset));
					json_t* jitems = json_array();
					
					for (size_t index = 0; index < module->attachments.offset; index++) {
						struct Attachment* const attachment = &module->attachments.items[index];
						
						json_t* jattachment = json_object();
						
						json_object_set_new(jattachment, "id", json_string(attachment->id));
						json_object_set_new(jattachment, "filename", json_string(attachment->filename));
						json_object_set_new(jattachment, "short_filename", json_string(attachment->short_filename));
						json_object_set_new(jattachment, "path", json_string(attachment->path));
						
						json_array_append_new(jitems, jattachment);
					}
					
					json_object_set_new(jattachments, "items", jitems);
					json_object_set_new(jmodule, "attachments", jattachments);
				}
				
				if (module->pages.offset < 1) {
					json_object_set_new(jmodule, "pages", json_null());
				} else {
					json_t* jpages = json_object();
					json_object_set_new(jpages, "type", json_string("Pages"));
					json_object_set_new(jpages, "offset", json_integer(module->pages.offset));
					json_t* jitems = json_array();
					
					for (size_t index = 0; index < module->pages.offset; index++) {
						struct Page* const page = &module->pages.items[index];
						
						json_t* jpage = json_object();
						
						json_object_set_new(jpage, "type", json_string("Page"));
						json_object_set_new(jpage, "id", json_string(page->id));
						json_object_set_new(jpage, "dirname", json_string(page->dirname));
						json_object_set_new(jpage, "short_dirname", json_string(page->short_dirname));
						
						if (page->document.id == NULL) {
							json_object_set_new(jpage, "document", json_null());
						} else {
							json_t* jdocument = json_object();
							
							json_object_set_new(jdocument, "type", json_string("Document"));
							json_object_set_new(jdocument, "id", json_string(page->document.id));
							json_object_set_new(jdocument, "filename", json_string(page->document.filename));
							json_object_set_new(jdocument, "short_filename", json_string(page->document.short_filename));
							json_object_set_new(jdocument, "path", json_string(page->document.path));
							
							json_object_set_new(jpage, "document", jdocument);
						}
						
						if (page->medias.offset < 1) {
							json_object_set_new(jpage, "medias", json_null());
						} else {
							json_t* jmedias = json_object();
							json_object_set_new(jmedias, "type", json_string("Medias"));
							json_object_set_new(jmedias, "offset", json_integer(page->medias.offset));
							json_t* jitems = json_array();
							
							for (size_t index = 0; index < page->medias.offset; index++) {
								struct Media* const media = &page->medias.items[index];
								
								json_t* jmedia = json_object();
								
								const int is_audio = media->audio.url != NULL && media->video.url == NULL;
								
								json_object_set_new(jmedia, "type", json_string("Media"));
								json_object_set_new(jmedia, "id", json_string(is_audio ? media->audio.id : media->video.id));
								json_object_set_new(jmedia, "filename", json_string(is_audio ? media->audio.filename : media->video.filename));
								json_object_set_new(jmedia, "short_filename", json_string(is_audio ? media->audio.short_filename : media->video.short_filename));
								json_object_set_new(jmedia, "path", json_string(media->path));
								
								json_array_append_new(jitems, jmedia);
							}
							
							json_object_set_new(jmedias, "items", jitems);
							json_object_set_new(jpage, "medias", jmedias);
						}
								
						if (page->attachments.offset < 1) {
							json_object_set_new(jpage, "attachments", json_null());
						} else {
							json_t* jattachments = json_object();
							json_object_set_new(jattachments, "type", json_string("Attachments"));
							json_object_set_new(jattachments, "offset", json_integer(page->attachments.offset));
							json_t* jitems = json_array();
							
							for (size_t index = 0; index < page->attachments.offset; index++) {
								struct Attachment* const attachment = &page->attachments.items[index];
								
								json_t* jattachment = json_object();
								
								json_object_set_new(jattachment, "type", json_string("Attachment"));
								json_object_set_new(jattachment, "id", json_string(attachment->id));
								json_object_set_new(jattachment, "filename", json_string(attachment->filename));
								json_object_set_new(jattachment, "short_filename", json_string(attachment->short_filename));
								json_object_set_new(jattachment, "path", json_string(attachment->path));
								
								json_array_append_new(jitems, jattachment);
							}
							
							json_object_set_new(jattachments, "items", jitems);
							json_object_set_new(jpage, "attachments", jattachments);
						}
						
						json_object_set_new(jpage, "is_locked", page->is_locked ? json_true() : json_false());
						json_object_set_new(jpage, "path", json_string(page->path));
						
						json_array_append_new(jitems, jpage);
					}
					
					json_object_set_new(jpages, "items", jitems);
					json_object_set_new(jmodule, "pages", jpages);
				}
				
				json_object_set_new(jmodule, "path", json_string(module->path));
				
				json_array_append_new(jitems, jmodule);
			}
			
			json_object_set_new(jmodules, "items", jitems);
			json_object_set_new(jresource, "modules", jmodules);
		}
		
		json_object_set_new(jresource, "path", json_string(resource->path));
		
		char filename[strlen(resource->path) + strlen(DOT) + strlen(JSON_FILE_EXTENSION) + 1];
		strcpy(filename, resource->path);
		strcat(filename, DOT);
		strcat(filename, JSON_FILE_EXTENSION);
		
		printf("- Exportando árvore de objetos para '%s'\r\n", filename);
		
		struct FStream* const stream = fstream_open(filename, "wb");
		
		if (stream == NULL) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, error.message);
			return EXIT_FAILURE;
		}
		
		const int code = json_dump_callback(jresource, json_dump_cb, (void*) stream, JSON_COMPACT);
		
		if (code != 0) {
			const struct SystemError error = get_system_error();
			
			fstream_close(stream);
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar a árvore de objetos para '%s': %s\r\n", filename, error.message);
			return EXIT_FAILURE;
		}
		
		fstream_close(stream);
		
	}
	
	return EXIT_SUCCESS;
	
}
