#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define A "SparkleC"

#ifdef WIN32
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
#include "cacert.h"
#include "curl.h"
#include "hotmart.h"
#include "fstream.h"

#if defined(WIN32) && defined(UNICODE)
	#include "wio.h"
#endif

static const char LOCAL_ACCOUNTS_FILENAME[] = "accounts.json";

#define MAX_INPUT_SIZE 1024

static int ask_user_credentials(struct Credentials* const obj) {
	
	char username[MAX_INPUT_SIZE + 1] = {'\0'};
	char password[MAX_INPUT_SIZE + 1] = {'\0'};
	
	while (1) {
		printf("> Insira seu usuário: ");
		
		if (fgets(username, sizeof(username), stdin) != NULL && *username != '\n') {
			break;
		}
		
		fprintf(stderr, "- Usuário inválido ou não reconhecido!\r\n");
	}
	
	*strchr(username, '\n') = '\0';
	
	while (1) {
		printf("> Insira sua senha: ");
		
		if (fgets(password, sizeof(password), stdin) != NULL && *password != '\n') {
			break;
		}
		
		fprintf(stderr, "- Senha inválida ou não reconhecida!\r\n");
	}
	
	*strchr(password, '\n') = '\0';
	
	const int code = authorize(username, password, obj);
	
	if (code != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível realizar a autenticação: %s\r\n", strurr(code));
		return 0;
	}
	
	obj->username = malloc(strlen(username) + 1);
	
	if (obj->username == NULL) {
		
	}
	
	strcpy(obj->username, username);
	
	printf("+ Usuário autenticado com sucesso!\r\n");
	
	return 1;
	
}

static void curl_poll(struct Download* dqueue, const size_t dcount, size_t* total_done) {
	
	CURLM* curl_multi = curl_multi_global();
	
	int still_running = 1;
	
	curl_progress_cb(NULL, dcount, *total_done, 0, 0);
	
	while (still_running) {
		CURLMcode mc = curl_multi_perform(curl_multi, &still_running);
		
		if (still_running) {
			mc = curl_multi_poll(curl_multi, NULL, 0, 1000, NULL);
		}
			
		CURLMsg* msg = NULL;
		int msgs_left = 0;
		
		while ((msg = curl_multi_info_read(curl_multi, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE) {
				struct Download* download = NULL;
				
				for (size_t index = 0; index < dcount; index++) {
					struct Download* subdownload = &dqueue[index];
					
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
					curl_progress_cb(NULL, dcount, *total_done, 0, 0);
				} else {
					fstream_seek(download->stream, 0, FSTREAM_SEEK_BEGIN);
					curl_multi_add_handle(curl_multi, msg->easy_handle);
				}
			}
		}
		
		if (mc) {
			break;
		}
	}
	
}

static int m3u8_download(const char* const url, const char* const output) {
	
	CURLM* curl_multi = curl_multi_global();
	CURL* curl_easy = curl_easy_global();
	
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
		struct Tag* tag = &tags.items[index];
		
		if (tag->type == EXT_X_KEY) {
			struct Attribute* attribute = attributes_get(&tag->attributes, "URI");
			
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
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, strerror(errno));
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
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, strerror(errno));
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
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", filename, strerror(errno));
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
	
	printf("\r\n");
	
	printf("+ Exportando lista de reprodução para '%s'\r\n", playlist_filename);
	 
	struct FStream* stream = fstream_open(playlist_filename, "wb");
	
	if (stream == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", playlist_filename, strerror(errno));
		return UERR_FAILURE;
	}
			
	const int ok = tags_dumpf(&tags, stream);
	
	fstream_close(stream);
	m3u8_free(&tags);
	
	if (!ok) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar a lista de reprodução!\r\n");
		return UERR_FAILURE;
	}
	
	printf("+ Copiando arquivos de mídia para '%s'\r\n", output);
	
	const char* const command = "ffmpeg -loglevel error -allowed_extensions ALL -i \"%s\" -c copy \"%s\"";
	
	const int size = snprintf(NULL, 0, command, playlist_filename, output);
	char shell_command[size + 1];
	snprintf(shell_command, sizeof(shell_command), command, playlist_filename, output);
	
	const int exit_code = execute_shell_command(shell_command);
	
	for (size_t index = 0; index < dl_total; index++) {
		struct Download* download = &dl_queue[index];
		
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

#if defined(WIN32) && defined(UNICODE)
	#define main wmain
#endif

int main(void) {
	
	#ifdef WIN32
		_setmaxstdio(2048);
		
		#ifdef UNICODE
			_setmode(_fileno(stdout), _O_WTEXT);
			_setmode(_fileno(stderr), _O_WTEXT);
			
			setlocale(LC_ALL, ".UTF8");
		#endif
	#else
		struct rlimit rlim = {0};
		getrlimit(RLIMIT_NOFILE, &rlim);
		
		rlim.rlim_cur = rlim.rlim_max;
		setrlimit(RLIMIT_NOFILE, &rlim);
	#endif
	/*
	if (is_administrator()) {
		fprintf(stderr, "- Você não precisa e nem deve executar este programa com privilégios elevados!\r\n");
		return EXIT_FAILURE;
	}
	*/
	char* const directory = get_configuration_directory();
	
	if (directory == NULL) {
		fprintf(stderr, "- Não foi possível obter um diretório de configurações válido!\r\n");
		return EXIT_FAILURE;
	}
	
	char configuration_directory[strlen(directory) + strlen(A) + 1];
	strcpy(configuration_directory, directory);
	strcat(configuration_directory, A);
	
	free(directory);
	
	if (!directory_exists(configuration_directory)) {
		fprintf(stderr, "- Diretório de configurações não encontrado, criando-o\r\n");
		
		if (!create_directory(configuration_directory)) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s'\r\n", configuration_directory);
			return EXIT_FAILURE;
		}
	}
	
	char accounts_file[strlen(configuration_directory) + strlen(PATH_SEPARATOR) + strlen(LOCAL_ACCOUNTS_FILENAME) + 1];
	strcpy(accounts_file, configuration_directory);
	strcat(accounts_file, PATH_SEPARATOR);
	strcat(accounts_file, LOCAL_ACCOUNTS_FILENAME);
	
	CURL* curl_easy = curl_easy_global();
	
	if (curl_easy == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar inicializar o cliente HTTP!\r\n");
		return EXIT_FAILURE;
	}
	
	CURLM* curl_multi = curl_multi_global();
	
	if (curl_multi == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar inicializar o cliente HTTP!\r\n");
		return EXIT_FAILURE;
	}
	
	struct Credentials credentials = {0};
	
	if (file_exists(accounts_file)) {
		json_auto_t* tree = json_load_file(accounts_file, 0, NULL);
		
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
		json_t *item = NULL;
		
		printf("+ Selecione qual das suas contas você deseja usar: \r\n\r\n");
		printf("0.\r\nAcessar uma outra conta\r\n\r\n");
		
		json_array_foreach(tree, index, item) {
			json_t* subobj = json_object_get(item, "username");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Uma ou mais credenciais localizadas no arquivo em '%s' possui um formato inválido ou não reconhecido!\r\n", accounts_file);
				return EXIT_FAILURE;
			}
			
			const char* const username = json_string_value(subobj);
			
			subobj = json_object_get(item, "access_token");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Uma ou mais credenciais localizadas no arquivo em '%s' possui um formato inválido ou não reconhecido!\r\n", accounts_file);
				return EXIT_FAILURE;
			}
			
			const char* const access_token = json_string_value(subobj);
			
			subobj = json_object_get(item, "refresh_token");
			
			if (subobj == NULL || !json_is_string(subobj)) {
				fprintf(stderr, "- Uma ou mais credenciais localizadas no arquivo em '%s' possui um formato inválido ou não reconhecido!\r\n", accounts_file);
				return EXIT_FAILURE;
			}
			
			const char* const refresh_token = json_string_value(subobj);
			
			struct Credentials credentials = {
				.access_token = malloc(strlen(access_token) + 1),
				.refresh_token = malloc(strlen(refresh_token) + 1)
			};
			
			if (credentials.access_token == NULL || credentials.refresh_token == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
				return EXIT_FAILURE;
			}
			
			strcpy(credentials.access_token, access_token);
			strcpy(credentials.refresh_token, refresh_token);
			
			items[index] = credentials;
			
			printf("%zu. \r\nAcessar usando a conta: '%s'\r\n\r\n", index + 1, username);
		}
		
		char answer[4 + 1];
		int value = 0;
		
		while (1) {
			printf("> Digite sua escolha: ");
			
			if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
				*strchr(answer, '\n') = '\0';
				
				if (isnumeric(answer)) {
					value = atoi(answer);
					
					if (value >= 0 && (size_t) value <= total_items) {
						break;
					}
				}
			}
			
			fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
		}
		
		if (value == 0) {
			if (!ask_user_credentials(&credentials)) {
				return EXIT_FAILURE;
			}
			
			FILE* const file = fopen(accounts_file, "wb");
			
			if (file == NULL) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", accounts_file, strerror(errno));
				return EXIT_FAILURE;
			}
			
			json_auto_t* subtree = json_object();
			json_object_set_new(subtree, "username", json_string(credentials.username));
			json_object_set_new(subtree, "access_token", json_string(credentials.access_token));
			json_object_set_new(subtree, "refresh_token", json_string(credentials.refresh_token));
			
			json_array_append(tree, subtree);
			
			char* const buffer = json_dumps(tree, JSON_COMPACT);
			
			if (buffer == NULL) {
				fprintf(stderr, "- Ocorreu uma falha ao tentar exportar o arquivo de credenciais!\r\n");
				return EXIT_FAILURE;
			}
			
			const size_t buffer_size = strlen(buffer);
			const size_t wsize = fwrite(buffer, sizeof(*buffer), buffer_size, file);
			
			const int cerrno = errno;
			
			free(buffer);
			fclose(file);
			
			if (wsize != buffer_size) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar o arquivo de credenciais: %s\r\n", strerror(cerrno));
				return EXIT_FAILURE;
			}
		} else {
			credentials = items[value - 1];
		}
	} else {
		if (!ask_user_credentials(&credentials)) {
			return EXIT_FAILURE;
		}
		
		FILE* const stream = fopen(accounts_file, "wb");
		
		if (stream == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", accounts_file, strerror(errno));
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
		json_object_set_new(obj, "refresh_token", json_string(credentials.refresh_token));
		
		json_array_append(tree, obj);
		
		char* const buffer = json_dumps(tree, JSON_COMPACT);
		
		if (buffer == NULL) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar o arquivo de credenciais!\r\n");
			return EXIT_FAILURE;
		}
		
		const size_t buffer_size = strlen(buffer);
		const size_t wsize = fwrite(buffer, sizeof(*buffer), buffer_size, stream);
		
		const int cerrno = errno;
		
		free(buffer);
		fclose(stream);
		
		if (wsize != buffer_size) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar exportar o arquivo de credenciais: %s\r\n", strerror(cerrno));
			return EXIT_FAILURE;
		}
	}
	
	printf("+ Obtendo lista de produtos\r\n");
	
	struct Resources resources = {0};
	
	const int code = get_resources(&credentials, &resources);
	
	if (code == UERR_HOTMART_SESSION_EXPIRED) {
		fprintf(stderr, "- Sua sessão expirou ou foi revogada, refaça o login!\r\n");
		return EXIT_FAILURE;
	}
	
	if (code != UERR_SUCCESS) {
		fprintf(stderr, "- Não foi possível obter a lista de produtos!\r\n");
		return EXIT_FAILURE;
	}
	
	printf("+ Selecione o que deseja baixar:\r\n\r\n");
	
	printf("0.\r\nTodos os produtos disponíveis\r\n\r\n");
	
	for (size_t index = 0; index < resources.offset; index++) {
		const struct Resource* resource = &resources.items[index];
		printf("%zu. \r\nNome: %s\r\nhttps://%s%s\r\n\r\n", index + 1, resource->name, resource->subdomain, HOTMART_CLUB_SUFFIX);
	}
	
	char answer[4 + 1];
	int value = 0;
	
	while (1) {
		printf("> Digite sua escolha: ");
		
		if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
			*strchr(answer, '\n') = '\0';
			
			if (isnumeric(answer)) {
				value = atoi(answer);
				
				if (value >= 0 && (size_t) value <= resources.offset) {
					break;
				}
			}
		}
		
		fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
	}
	
	int kof = 0;
	
	while (1) {
		printf("> Manter o nome original de arquivos e diretórios? (S/n) ");
		
		if (fgets(answer, sizeof(answer), stdin) != NULL && *answer != '\n') {
			*strchr(answer, '\n') = '\0';
			
			switch (*answer) {
				case 's':
				case 'y':
					kof = 1;
					break;
				case 'n':
					kof = 0;
					break;
				default:
					fprintf(stderr, "- Opção inválida ou não reconhecida!\r\n");
					continue;
			}
			
			break;
		}
	}
	
	fclose(stdin);
	
	struct Resource download_queue[resources.offset];
	
	size_t queue_count = 0;
	
	if (value == 0) {
		for (size_t index = 0; index < sizeof(download_queue) / sizeof(*download_queue); index++) {
			struct Resource resource = resources.items[index];
			download_queue[index] = resource;
			
			queue_count++;
		}
	} else {
		struct Resource resource = resources.items[value - 1];
		*download_queue = resource;
		
		queue_count++;
	}
	
	char* cwd = get_current_directory();
	
	if (cwd == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada!\r\n");
		return EXIT_FAILURE;
	}
	
	for (size_t index = 0; index < queue_count; index++) {
		struct Resource* resource = &download_queue[index];
		
		printf("+ Obtendo lista de módulos do produto '%s'\r\n", resource->name);
		
		const int code = get_modules(&credentials, resource);
		
		if (code != UERR_SUCCESS) {
			fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
			return EXIT_FAILURE;
		}
		
		char directory[(kof ? strlen(resource->name) : strlen(resource->subdomain)) + 1];
		strcpy(directory, (kof ? resource->name : resource->subdomain));
		normalize_filename(directory);
		
		char resource_directory[strlen(cwd) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
		strcpy(resource_directory, cwd);
		strcat(resource_directory, PATH_SEPARATOR);
		strcat(resource_directory, directory);
		
		if (!directory_exists(resource_directory)) {
			fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", resource_directory);
			
			if (!create_directory(resource_directory)) {
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", resource_directory, strerror(errno));
				return EXIT_FAILURE;
			}
		}
		
		for (size_t index = 0; index < resource->modules.offset; index++) {
			struct Module* module = &resource->modules.items[index];
			
			printf("+ Verificando estado do módulo '%s'\r\n", module->name);
			
			if (module->is_locked) {
				fprintf(stderr, "- Módulo inacessível, pulando para o próximo\r\n");
				continue;
			}
			
			char directory[(kof ? strlen(module->name) : strlen(module->id)) + 1];
			strcpy(directory, (kof ? module->name : module->id));
			normalize_filename(directory);
			
			char module_directory[strlen(resource_directory) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
			strcpy(module_directory, resource_directory);
			strcat(module_directory, PATH_SEPARATOR);
			strcat(module_directory, directory);
			
			if (!directory_exists(module_directory)) {
				fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", module_directory);
				
				if (!create_directory(module_directory)) {
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", module_directory, strerror(errno));
					return EXIT_FAILURE;
				}
			}
			
			printf("+ Obtendo lista de páginas do módulo '%s'\r\n", module->name);
			
			for (size_t index = 0; index < module->pages.offset; index++) {
				struct Page* page = &module->pages.items[index];
				
				printf("+ Obtendo informações sobre a página '%s'\r\n", page->name);
				
				if (page->is_locked) {
					fprintf(stderr, "- Página inacessível, pulando para a próxima\r\n");
					continue;
				}
				
				const int code = get_page(&credentials, resource, page);
				
				if (code != UERR_SUCCESS) {
					fprintf(stderr, "- Ocorreu uma falha inesperada: %s\r\n", strurr(code));
					return EXIT_FAILURE;
				}
				
				printf("+ Verificando estado da página '%s'\r\n", page->name);
				
				char directory[(kof ? strlen(page->name) : strlen(page->id)) + 1];
				strcpy(directory, (kof ? page->name : page->id));
				normalize_filename(directory);
				
				char page_directory[strlen(module_directory) + strlen(PATH_SEPARATOR) + strlen(directory) + 1];
				strcpy(page_directory, module_directory);
				strcat(page_directory, PATH_SEPARATOR);
				strcat(page_directory, directory);
				
				if (!directory_exists(page_directory)) {
					fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", page_directory);
					
					if (!create_directory(page_directory)) {
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", page_directory, strerror(errno));
						return EXIT_FAILURE;
					}
				}
				
				int suffix = 0;
				
				if (page->document.content != NULL) {
					const char* const extension = get_file_extension(page->document.filename);
					suffix++;
					
					char document_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + (kof ? strlen(page->document.filename) : strlen(page->id) + intlen(suffix) + strlen(DOT) + strlen(extension)) + 1];
					strcpy(document_filename, page_directory);
					strcat(document_filename, PATH_SEPARATOR);
					
					if (kof) {
						strcat(document_filename, page->document.filename);
					} else {
						strcat(document_filename, page->id);
						
						char value[intlen(suffix) + 1];
						snprintf(value, sizeof(value), "%i", suffix);
						
						strcat(document_filename, value);
						strcat(document_filename, DOT);
						strcat(document_filename, extension);
						
						normalize_filename(basename(document_filename));
					}
					
					if (!file_exists(document_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, salvando-o\r\n", document_filename);
						
						struct FStream* stream = fstream_open(document_filename, "wb");
						
						if (stream == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", document_filename, strerror(errno));
							return EXIT_FAILURE;
						}
						
						const int status = fstream_write(stream, page->document.content, strlen(page->document.content));
						const int cerrno = errno;
						
						fstream_close(stream);
						
						if (!status) {
							remove_file(document_filename);
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar salvar o documento: %s\r\n", strerror(cerrno));
							return EXIT_FAILURE;
						}
					}
				}
				
				for (size_t index = 0; index < page->medias.offset; index++) {
					struct Media* media = &page->medias.items[index];
					
					const char* const extension = get_file_extension(media->video.filename);
					suffix++;
					
					char media_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + (kof ? strlen(media->video.filename) : strlen(page->id) + intlen(suffix) + strlen(DOT) + strlen(extension)) + 1];
					strcpy(media_filename, page_directory);
					strcat(media_filename, PATH_SEPARATOR);
					
					if (kof) {
						strcat(media_filename, media->video.filename);
					} else {
						strcat(media_filename, page->id);
						
						char value[intlen(suffix) + 1];
						snprintf(value, sizeof(value), "%i", suffix);
						
						strcat(media_filename, value);
						strcat(media_filename, DOT);
						strcat(media_filename, extension);
						
						normalize_filename(basename(media_filename));
					}
					
					if (!file_exists(media_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, baixando-o\r\n", media_filename);
						
						switch (media->type) {
							case MEDIA_M3U8: {
								char* audio_filename = NULL;
								const char* video_filename = NULL;
								
								if (media->audio.url != NULL) {
									const char* const extension = get_file_extension(media->audio.filename);
									suffix++;
									
									audio_filename = malloc(strlen(page_directory) + strlen(PATH_SEPARATOR) + (kof ? strlen(media->audio.filename) : strlen(page->id) + intlen(suffix) + strlen(DOT) + strlen(extension)) + 1);
									
									if (audio_filename == NULL) {
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar alocar memória do sistema!\r\n");
										return EXIT_FAILURE;
									}
									
									strcpy(audio_filename, page_directory);
									strcat(audio_filename, PATH_SEPARATOR);
									
									if (kof) {
										strcat(audio_filename, media->audio.filename);
									} else {
										strcat(audio_filename, page->id);
										
										char value[intlen(suffix) + 1];
										snprintf(value, sizeof(value), "%i", suffix);
										
										strcat(audio_filename, value);
										strcat(audio_filename, DOT);
										strcat(audio_filename, extension);
										
										normalize_filename(basename(audio_filename));
									}
					
									printf("+ Baixando de '%s' para '%s'\r\n", media->audio.url, audio_filename);
									
									if (m3u8_download(media->audio.url, audio_filename) != UERR_SUCCESS) {
										return EXIT_FAILURE;
									}
								}
								
								if (media->video.url != NULL) {
									printf("+ Baixando de '%s' para '%s'\r\n", media->video.url, media_filename);
									
									if (m3u8_download(media->video.url, media_filename) != UERR_SUCCESS) {
										return EXIT_FAILURE;
									}
									
									video_filename = media_filename;
								}
								
								if (audio_filename != NULL && video_filename != NULL) {
									const char* const extension = get_file_extension(video_filename);
									
									char temporary_file[strlen(video_filename) + strlen(DOT) + strlen(extension) + 1];
									strcpy(temporary_file, video_filename);
									strcat(temporary_file, DOT);
									strcat(temporary_file, extension);
									
									const char* const command = "ffmpeg -loglevel error -i \"%s\" -i \"%s\" -c copy -map 0:v:0 -map 1:a:0 \"%s\"";
									
									const int size = snprintf(NULL, 0, command, video_filename, audio_filename, temporary_file);
									char shell_command[size + 1];
									snprintf(shell_command, sizeof(shell_command), command, video_filename, audio_filename, temporary_file);
									
									printf("+ Copiando canais de áudio e vídeo para uma única mídia em '%s'\r\n", temporary_file);
									
									const int exit_code = execute_shell_command(shell_command);
									
									remove_file(audio_filename);
									remove_file(video_filename);
									
									free(audio_filename);
									audio_filename = NULL;
									
									if (exit_code != 0) {
										fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar processar a mídia!\r\n");
										return EXIT_FAILURE;
									}
									
									printf("+ Movendo arquivo de mídia de '%s' para '%s'\r\n", temporary_file, video_filename);
									
									move_file(temporary_file, video_filename);
								}
							}
							
							curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
							curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
							curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
							
							break;
							
							case MEDIA_SINGLE: {
								printf("+ Baixando de '%s' para '%s'\r\n", media->video.url, media_filename);
								
								struct FStream* stream = fstream_open(media_filename, "wb");
								
								if (stream == NULL) {
									fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", media_filename, strerror(errno));
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
								
								fstream_close(stream);
								
								curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
								curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, NULL);
								curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 1L);
								curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 60L);
								curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, NULL);
								curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, NULL);
								curl_easy_setopt(curl_easy, CURLOPT_URL, NULL);
								curl_easy_setopt(curl_easy, CURLOPT_FOLLOWLOCATION, 0L);
								
								printf("\r\n");
								
								if (code != CURLE_OK) {
									remove_file(media_filename);
									fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor em '%s': %s\r\n", media->video.url, curl_easy_strerror(code));
									return EXIT_FAILURE;
								}
							}
							
							break;
						}
					}
				}
				
				curl_easy_setopt(curl_easy, CURLOPT_TIMEOUT, 0L);
				curl_easy_setopt(curl_easy, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
				
				for (size_t index = 0; index < page->attachments.offset; index++) {
					struct Attachment* attachment = &page->attachments.items[index];
					
					const char* const extension = get_file_extension(attachment->filename);
					suffix++;
					
					char attachment_filename[strlen(page_directory) + strlen(PATH_SEPARATOR) + (kof ? strlen(attachment->filename) : strlen(page->id) + intlen(suffix) + strlen(DOT) + strlen(extension)) + 1];
					strcpy(attachment_filename, page_directory);
					strcat(attachment_filename, PATH_SEPARATOR);
					
					if (kof) {
						strcat(attachment_filename, attachment->filename);
					} else {
						strcat(attachment_filename, page->id);
						
						char value[intlen(suffix) + 1];
						snprintf(value, sizeof(value), "%i", suffix);
						
						strcat(attachment_filename, value);
						strcat(attachment_filename, DOT);
						strcat(attachment_filename, extension);
						
						normalize_filename(basename(attachment_filename));
					}
					
					if (!file_exists(attachment_filename)) {
						fprintf(stderr, "- O arquivo '%s' não existe, ele será baixado\r\n", attachment_filename);
						printf("+ Baixando de '%s' para '%s'\r\n", attachment->url, attachment_filename);
						
						struct FStream* stream = fstream_open(attachment_filename, "wb");
						
						if (stream == NULL) {
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o arquivo em '%s': %s\r\n", attachment_filename, strerror(errno));
							return EXIT_FAILURE;
						}
						
						curl_easy_setopt(curl_easy, CURLOPT_NOPROGRESS, 0L);
						curl_easy_setopt(curl_easy, CURLOPT_URL, attachment->url);
						curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION, curl_write_file_cb);
						curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, (void*) stream);
						
						const CURLcode code = curl_easy_perform(curl_easy);
						
						fstream_close(stream);
						
						printf("\r\n");
						
						if (code != CURLE_OK) {
							remove_file(attachment_filename);
							fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar conectar com o servidor em '%s': %s\r\n", attachment->url, curl_easy_strerror(code));
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
			}
		}
	}
	
	return EXIT_SUCCESS;
	
}
