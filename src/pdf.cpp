#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _UNICODE
	#include <fcntl.h>
	#include <io.h>
#endif

#define POINTERHOLDER_TRANSITION 0

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>

#if defined(_WIN32) && defined(_UNICODE)
	#include "wio.h"
#endif

#include "input.h"
#include "errors.h"
#include "symbols.h"
#include "fstream.h"
#include "filesystem.h"
#include "walkdir.h"
#include "os.h"
#include "stringu.h"

static const char BOM_MAGIC_NUMBERS[] = {
	(char) 0xef,
	(char) 0xbb,
	(char) 0xbf
};

static const char PDF_MAGIC_NUMBERS[] = {
	(char) 0x25,
	(char) 0x50,
	(char) 0x44,
	(char) 0x46
};

static char* temporary_directory = NULL;

static int magic_matches(const char* const filename) {
	
	struct FStream* const stream = fstream_open(filename, FSTREAM_READ);
	
	if (stream == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar abrir o arquivo em '%s': %s\r\n", filename, error.message);
		return -1;
	}
	
	char chunk[sizeof(BOM_MAGIC_NUMBERS) + sizeof(PDF_MAGIC_NUMBERS)] = {'\0'};
	
	const ssize_t size = fstream_read(stream, chunk, sizeof(chunk));
	
	if (size == -1) {
		const struct SystemError error = get_system_error();
		fstream_close(stream);
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar ler o arquivo em '%s': %s\r\n", filename, error.message);
		return -1;
	}
	
	fstream_close(stream);
	
	const char* start = chunk;
	
	const int bom = memcmp(start, BOM_MAGIC_NUMBERS, sizeof(BOM_MAGIC_NUMBERS)) == 0;
	
	if (bom) {
		start += sizeof(BOM_MAGIC_NUMBERS);
	}
	
	const int matches = memcmp(chunk, PDF_MAGIC_NUMBERS, sizeof(PDF_MAGIC_NUMBERS)) == 0;
	
	return matches;
	
}

static int walk_directory(const char* const directory, const char* const pattern) {
	
	struct WalkDir walkdir = {};
	
	if (walkdir_init(&walkdir, directory) == -1) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar listar os arquivos do diretório em '%s': %s\r\n", directory, error.message);
		return -1;
	}
	
	while (1) {
		const struct WalkDirItem* const item = walkdir_next(&walkdir);
		
		if (item == NULL) {
			break;
		}
		
		if (strcmp(item->name, ".") == 0 || strcmp(item->name, "..") == 0) {
			continue;
		}
		
		char path[strlen(directory) + strlen(PATH_SEPARATOR) + strlen(item->name) + 1];
		strcpy(path, directory);
		strcat(path, PATH_SEPARATOR);
		strcat(path, item->name);
		
		switch (item->type) {
			case WALKDIR_ITEM_DIRECTORY: {
				printf("+ Adentrando no diretório localizado em '%s'\r\n", path);
				
				const int code = walk_directory(path, pattern);
				
				if (code == -1) {
					walkdir_free(&walkdir);
					return -1;
				}
				
				break;
			}
			case WALKDIR_ITEM_FILE: {
				const int matches = magic_matches(path);
				
				if (!matches) {
					break;
				}
				
				printf("+ Abrindo o documento PDF localizado em '%s'\r\n", path);
				
				size_t total_pages = 0;
				
				QPDF pdf;
				pdf.processFile(path, nullptr);
				
				QPDFObjectHandle null = QPDFObjectHandle::newNull();
				
				const std::vector<QPDFObjectHandle> pages = pdf.getAllPages();
				
				for (QPDFObjectHandle page : pages) {
					QPDFObjectHandle contents = page.getKey("/Contents");
					
					std::shared_ptr<Buffer> stream = contents.getStreamData();
					char* const buffer = (char*) stream->getBuffer();
					
					char* match = strstr(buffer, pattern);
					
					if (match != NULL) {
						char* start = match;
						
						while (!(start == match || *start == '\n' || *start == '\r')) {
							start--;
						}
						
						*start = '\0';
						contents.replaceStreamData(buffer, null, null);
						
						total_pages += 1;
					}
				}
				
				if (total_pages > 0) {
					const char* const name = extract_filename(path);
					
					char temporary_file[strlen(temporary_directory) + strlen(PATH_SEPARATOR) + strlen(name) + 1];
					strcpy(temporary_file, temporary_directory);
					strcat(temporary_file, PATH_SEPARATOR);
					strcat(temporary_file, name);
					
					printf("+ %zu páginas foram modificadas; exportando o documento PDF modificado para '%s'\r\n", total_pages, temporary_file);
					
					QPDFWriter writer = QPDFWriter(pdf, temporary_file);
					writer.setObjectStreamMode(qpdf_o_generate);
					writer.write();
					
					printf("+ Movendo arquivo de '%s' para '%s'\r\n", temporary_file, path);
					
					if (move_file(temporary_file, path) == -1) {
						const struct SystemError error = get_system_error();
						
						remove_file(temporary_file);
						
						fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar mover o arquivo de '%s' para '%s': %s\r\n", temporary_file, path, error.message);
						return EXIT_FAILURE;
					}
					
					remove_file(temporary_file);
				} else {
					fprintf(stderr, "- Nenhuma correspondência foi encontrada no documento PDF localizado em '%s'; o arquivo não sofrerá alterações\r\n", path);
				}
				
				break;
			}
			default:
				break;
		}
	}
	
	return 0;
	
}

#if defined(_WIN32) && defined(_UNICODE)
	#define main wmain
	int wmain(void);
#endif

int main() {
	
	#if defined(_WIN32) && defined(_UNICODE)
		_setmode(_fileno(stdout), _O_WTEXT);
		_setmode(_fileno(stderr), _O_WTEXT);
		_setmode(_fileno(stdin), _O_WTEXT);
	#endif
	
	char* cwd = get_current_directory();
	
	if (cwd == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter o diretório de trabalho atual: %s\r\n", error.message);
		return EXIT_FAILURE;
	}
	
	char* tmp = get_temporary_directory();
	
	if (tmp == NULL) {
		const struct SystemError error = get_system_error();
		
		fprintf(stderr, "- Ocorreu uma falha inesperada ao tentar obter o diretório temporário: %s\r\n", error.message);
		return EXIT_FAILURE;
	}
	
	temporary_directory = tmp;
	
	char pattern[MAX_INPUT_SIZE] = {'\0'};
	input("> Insira o texto: ", pattern);
	
	if (walk_directory(cwd, pattern) == -1) {
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
	
}