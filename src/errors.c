#include "errors.h"

const char* strurr(const int code) {
	
	switch (code) {
		case UERR_MEMORY_ALLOCATE_FAILURE:
			return "Não foi possível alocar memória do sistema";
		case UERR_CURL_FAILURE:
			return "A requisição para um servidor HTTP não pôde ser completada com êxito";
		case UERR_STRSTR_FAILURE:
			return "Não foi possível encontrar uma correspondência para a sequência de caracteres buscada";
		case UERR_M3U8_UNTERMINATED_STRING_LITERAL:
			return "Não foi possível processar a lista de reprodução M3U8";
		case UERR_JSON_CANNOT_PARSE:
			return "Não foi possível usar o JSON para processar as informações recebidas";
		case UERR_JSON_MISSING_REQUIRED_KEY:
			return "A chave JSON obrigatória não foi encontrada";
		case UERR_JSON_NON_MATCHING_TYPE:
			return "A chave JSON possui um valor inesperado ou não reconhecido";
		case UERR_ATTACHMENT_DRM_FAILURE:
			return "Não foi possível decodificar o anexo protegido por DRM";
		case UERR_NO_STREAMS_AVAILABLE:
			return "Não foi encontrado um canal de reprodução para a mídia";
		case UERR_TIDY_FAILURE:
			return "Não foi possível processar o conteúdo HTML";
		default:
			return "Causa desconhecida ou não especificada";
	}
	
}