#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#ifdef _WIN32
	#include <windows.h>
	
	#ifdef _UNICODE
		#include <fcntl.h>
		#include <io.h>
	#endif
#else
	#include <sys/stat.h>
#endif

#include <bearssl.h>
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>

#ifdef _WIN32
	#include "wregistry.h"

	#ifdef _UNICODE
		#include "wio.h"
	#endif
#endif

#include "filesystem.h"
#include "stringu.h"
#include "symbols.h"
#include "errors.h"
#include "os.h"
#include "fstream.h"
#include "cleanup.h"

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#ifndef _WIN32
	struct Shell {
		char* name;
		char* rc;
		char* pathset;
	};
	
	static const struct Shell SHELLS[] = {
		{
			.name = "zsh",
			.rc = ".zshrc",
			.pathset = "path+="
		},
		{
			.name = "bash",
			.rc = ".bashrc",
			.pathset = "PATH+="
		},
		{
			.name = "fish",
			.rc = ".config" PATH_SEPARATOR "fish" PATH_SEPARATOR "config.fish",
			.pathset = "fish_add_path --universal --append "
		}
	};
#endif

static const char* const INSTALL_FILES[] = {
	EXECUTABLE_DIRECTORY PATH_SEPARATOR "sparklec" EXECUTABLE_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libjansson" SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libbearssl" SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libcurl" SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libtidy" SHARED_LIBRARY_EXTENSION,
#if defined(__APPLE__)
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavformat" SOVERSION_SEPARATOR STRINGIFY(LIBAVFORMAT_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavcodec" SOVERSION_SEPARATOR STRINGIFY(LIBAVCODEC_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavutil" SOVERSION_SEPARATOR STRINGIFY(LIBAVUTIL_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
#elif defined(__ANDROID__)
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavformat" SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavcodec" SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavutil" SHARED_LIBRARY_EXTENSION,
#elif defined(_WIN32)
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "avformat" SOVERSION_SEPARATOR STRINGIFY(LIBAVFORMAT_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "avcodec" SOVERSION_SEPARATOR STRINGIFY(LIBAVCODEC_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "avutil" SOVERSION_SEPARATOR STRINGIFY(LIBAVUTIL_VERSION_MAJOR) SHARED_LIBRARY_EXTENSION,
#else
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavformat" SHARED_LIBRARY_EXTENSION SOVERSION_SEPARATOR STRINGIFY(LIBAVFORMAT_VERSION_MAJOR),
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavcodec" SHARED_LIBRARY_EXTENSION SOVERSION_SEPARATOR STRINGIFY(LIBAVCODEC_VERSION_MAJOR),
	SHARED_LIBRARY_DIRECTORY PATH_SEPARATOR "libavutil" SHARED_LIBRARY_EXTENSION SOVERSION_SEPARATOR STRINGIFY(LIBAVUTIL_VERSION_MAJOR),
#endif
	CONFIGURATION_DIRECTORY PATH_SEPARATOR "tls" PATH_SEPARATOR "cert.pem"
};

int sha256_digest(const char* const filename, char* const output) {
	
	struct FStream* const stream = fstream_open(filename, FSTREAM_READ);
	
	if (stream == NULL) {
		return -1;
	}
	
	br_sha256_context context = {0};
	br_sha256_init(&context);
	
	char chunk[8192] = {'\0'};
	
	while (1) {
		const ssize_t size = fstream_read(stream, chunk, sizeof(chunk));
		
		switch (size) {
			case -1:
				fstream_close(stream);
				return -1;
			case 0:
				if (fstream_close(stream) == -1) {
					return -1;
				}
				break;
		}
		
		if (size == 0) {
			break;
		}
		
		br_sha256_update(&context, chunk, size);
	}
	
	br_sha256_out(&context, output);
	
	return 0;
	
}

#if defined(_WIN32) && defined(_UNICODE)
	#define main wmain
	int wmain(void);
#endif

int main(void) {
	
	#if defined(_WIN32) && defined(_UNICODE)
		_setmode(_fileno(stdout), _O_WTEXT);
		_setmode(_fileno(stderr), _O_WTEXT);
		_setmode(_fileno(stdin), _O_WTEXT);
	#endif
	
	char app_filename[PATH_MAX] = {'\0'};
	get_app_filename(app_filename);
	
	char app_root_directory[PATH_MAX] = {'\0'};
	get_parent_directory(app_filename, app_root_directory, 2);
	
	char* home __free__ = get_home_directory();
	
	if (home == NULL) {
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter o diretório de usuário!\r\n");
		return EXIT_FAILURE;
	}
	
	char install_directory[strlen(home) + strlen(PATH_SEPARATOR) + 9 + 1];
	strcpy(install_directory, home);
	strcat(install_directory, PATH_SEPARATOR);
	strcat(install_directory, ".sparklec");
	
	for (size_t index = 0; index < sizeof(INSTALL_FILES) / sizeof(*INSTALL_FILES); index++) {
		const char* const filename = INSTALL_FILES[index];
		
		char parent_directory[strlen(filename) + 1];
		*parent_directory = '\0';
		
		get_parent_directory(filename, parent_directory, 1);
		
		char destination_directory[strlen(install_directory) + strlen(PATH_SEPARATOR) + strlen(parent_directory) + 1];
		strcpy(destination_directory, install_directory);
		strcat(destination_directory, PATH_SEPARATOR);
		strcat(destination_directory, parent_directory);
		
		switch (directory_exists(destination_directory)) {
			case 0: {
				fprintf(stderr, "- O diretório '%s' não existe, criando-o\r\n", destination_directory);
				
				if (create_directory(destination_directory) == -1) {
					const struct SystemError error = get_system_error();
					
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar criar o diretório em '%s': %s\r\n", destination_directory, error.message);
					return EXIT_FAILURE;
				}
				
				break;
			}
			case -1: {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter informações sobre o diretório em '%s': %s\r\n", destination_directory, error.message);
				return EXIT_FAILURE;
			}
		}
		
		char source_file[strlen(app_root_directory) + strlen(PATH_SEPARATOR) + strlen(filename) + 1];
		strcpy(source_file, app_root_directory);
		strcat(source_file, PATH_SEPARATOR);
		strcat(source_file, filename);
		
		switch (file_exists(source_file)) {
			case 0: {
				fprintf(stderr, "- O arquivo '%s' não existe, abortando instalação\r\n", source_file);
				return EXIT_FAILURE;
			}
			case -1: {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter informações sobre o arquivo em '%s': %s\r\n", source_file, error.message);
				return EXIT_FAILURE;
			}
		}
		
		char destination_file[strlen(install_directory) + strlen(PATH_SEPARATOR) + strlen(filename) + 1];
		strcpy(destination_file, install_directory);
		strcat(destination_file, PATH_SEPARATOR);
		strcat(destination_file, filename);
		
		int should_continue = 0;
		
		switch (file_exists(destination_file)) {
			case 1: {
				char isha256[br_sha256_SIZE] = {'\0'};
				char osha256[br_sha256_SIZE] = {'\0'};
				
				if (sha256_digest(source_file, isha256) == -1) {
					const struct SystemError error = get_system_error();
					
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar calcular o SHA256 do arquivo em '%s': %s\r\n", source_file, error.message);
					return EXIT_FAILURE;
				};
				
				if (sha256_digest(destination_file, osha256) == -1) {
					const struct SystemError error = get_system_error();
					
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar calcular o SHA256 do arquivo em '%s': %s\r\n", destination_file, error.message);
					return EXIT_FAILURE;
				};
				
				if (memcmp(isha256, osha256, sizeof(isha256)) == 0) {
					printf("- O arquivo '%s' não sofreu alterações desde a última instalação, ignorando-o\r\n", source_file);
					should_continue = 1;
				}
				
				break;
			}
			case -1: {
				const struct SystemError error = get_system_error();
				
				fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter informações sobre o arquivo em '%s': %s\r\n", destination_file, error.message);
				return EXIT_FAILURE;
			}
		}
		
		if (should_continue) {
			continue;
		}
		
		printf("+ Copiando arquivo de '%s' para '%s'\r\n", source_file, destination_file);
		
		if (copy_file(source_file, destination_file) == -1) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar copiar o arquivo de '%s' para '%s': %s\r\n", source_file, destination_file, error.message);
			return EXIT_FAILURE;
		}
		
		#ifndef _WIN32
			const char* const name = extract_filename(destination_file);
			
			if (strcmp(name, "sparklec") == 0) {
				printf("+ Aplicando permissões de executável para o arquivo em '%s'\r\n", destination_file);
				
				if (chmod(destination_file, S_IRWXU) == -1) {
					const struct SystemError error = get_system_error();
					
					fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar aplicar as permissões ao arquivo '%s': %s\r\n", destination_file, error.message);
					return EXIT_FAILURE;
				}
			}
		#endif
	}
	
	char* const executable = find_exe("sparklec");
	
	free(executable);
	
	if (executable != NULL) {
		return EXIT_SUCCESS;
	}
	
	printf("+ Verificando a possibilidade de adicionar o diretório de executáveis ao PATH do sistema\r\n");
	
	char executable_directory[strlen(install_directory) + strlen(PATH_SEPARATOR) + strlen(EXECUTABLE_DIRECTORY) + 1];
	strcpy(executable_directory, install_directory);
	strcat(executable_directory, PATH_SEPARATOR);
	strcat(executable_directory, EXECUTABLE_DIRECTORY);
	
	#ifdef _WIN32
		char* const current_path = wregistry_get(HKEY_CURRENT_USER, "Environment", "Path");
		
		const struct SystemError error = get_system_error();
		
		if (current_path == NULL && error.code != ERROR_FILE_NOT_FOUND) {
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar consultar as chaves de registro do Windows: %s\r\n", error.message);	
			return EXIT_FAILURE;
		}
		
		char new_path[(current_path == NULL ? 0 : strlen(current_path) + strlen(SEMICOLON)) + strlen(executable_directory) + 1];
		*new_path = '\0';
		
		if (current_path != NULL) {
			strcat(new_path, current_path);
			strcat(new_path, SEMICOLON);
		}
		
		strcat(new_path, executable_directory);
		
		free(current_path);
		
		printf("+ Modificando o valor da chave de registro %%PATH%% para '%s'\r\n", new_path);
		
		if (wregistry_put(HKEY_CURRENT_USER, "Environment", "Path", new_path) == -1) {
			const struct SystemError error = get_system_error();
		
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar modificar as chaves de registro do Windows: %s\r\n", error.message);
			return EXIT_FAILURE;
		};
	#else
		const char* const shell_executable = getenv("SHELL");
		
		if (shell_executable == NULL) {
			fprintf(stderr, "- Não foi possível determinar qual implementação de shell está sendo usada!\r\n");
			return EXIT_FAILURE;
		}
		
		const char* const name = extract_filename(shell_executable);
		
		const struct Shell* shell = NULL;
		
		for (size_t index = 0; index < sizeof(SHELLS) / sizeof(*SHELLS); index++) {
			const struct Shell* const sh = &SHELLS[index];
			
			if (strcmp(sh->name, name) != 0) {
				continue;
			}
			
			shell = sh;
			
			break;
		}
		
		if (shell == NULL) {
			fprintf(stderr, "- A implementação de shell atualmente sendo usada ('%s') não é suportada!\r\n", name);
			return EXIT_FAILURE;
		}
		
		char shellrc_filename[strlen(home) + strlen(PATH_SEPARATOR) + strlen(shell->rc) + 1];
		strcpy(shellrc_filename, home);
		strcat(shellrc_filename, PATH_SEPARATOR);
		strcat(shellrc_filename, shell->rc);
		
		char pathset[strlen(shell->pathset) + strlen(APOSTROPHE) * 2 + strlen(executable_directory) + 1];
		strcpy(pathset, shell->pathset);
		strcat(pathset, APOSTROPHE);
		strcat(pathset, executable_directory);
		strcat(pathset, APOSTROPHE);
		
		printf("+ Modificando o arquivo de configurações em '%s'\r\n", shellrc_filename);
		
		struct FStream* const stream = fstream_open(shellrc_filename, FSTREAM_APPEND);
		
		if (stream == NULL) {
			const struct SystemError error = get_system_error();
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar abrir o arquivo em '%s': %s\r\n", shellrc_filename, error.message);
			return EXIT_FAILURE;
		}
		
		if (fstream_write(stream, pathset, strlen(pathset)) == -1) {
			const struct SystemError error = get_system_error();
			
			fstream_close(stream);
			
			fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar salvar o documento em '%s': %s\r\n", shellrc_filename, error.message);
			return EXIT_FAILURE;
		}
		
		fstream_close(stream);
	#endif
	
	return EXIT_SUCCESS;
	
}