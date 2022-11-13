#include <tidy.h>

#include "errors.h"
#include "types.h"
#include "html.h"

static const char URL_PREFIX[] = "https:";
	
int attribute_find_all(string_array_t* const attributes, const tidy_node_t* const root, const char* const tag, const char* const attribute) {
	
	const char* const name = tidy_node_get_name(root);
	
	if (name != NULL && strcmp(name, tag) == 0) {
		for (const tidy_attr_t* attr = tidy_attr_first(root); attr != NULL; attr = tidy_attr_next(attr)) {
			const char* const name = tidy_attr_name(attr);
			const char* const value = tidy_attr_value(attr);
			
			if (strcmp(name, attribute) == 0) {
				const int is_relative = memcmp(value, "//", 2) == 0;
				
				char* src = malloc((is_relative ? strlen(URL_PREFIX) : 0) + strlen(value) + 1);
				
				if (src == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				*src = '\0';
				
				if (is_relative) {
					strcat(src, URL_PREFIX);
				}
				
				strcat(src, value);
				
				const size_t size = attributes->size + sizeof(char*) * 1;
				char** items = (char**) realloc(attributes->items, size);
				
				if (items == NULL) {
					return UERR_MEMORY_ALLOCATE_FAILURE;
				}
				
				attributes->size = size;
				attributes->items = items;
				
				attributes->items[attributes->offset++] = src;
			}
		}
		
	}
	
	const tidy_node_t* const next = tidy_get_next(root);
	
	if (next != NULL) {
		const int code = attribute_find_all(attributes, next, tag, attribute);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	const tidy_node_t* const children = tidy_get_child(root);
	
	if (children != NULL) {
		const int code = attribute_find_all(attributes, children, tag, attribute);
		
		if (code != UERR_SUCCESS) {
			return code;
		}
	}
	
	return UERR_SUCCESS;
	
}
