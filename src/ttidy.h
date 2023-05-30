#include <tidy.h>
#include <tidybuffio.h>

typedef struct _TidyNode tidy_node_t;
typedef struct _TidyBuffer tidy_buffer_t;
typedef struct _TidyDoc tidy_doc_t;
typedef struct _TidyAttr tidy_attr_t;

#define tidy_buffer_init tidyBufInit
#define tidy_buffer_append tidyBufAppend
#define tidy_buffer_free tidyBufFree
#define tidy_create tidyCreate
#define tidy_parse_buffer tidyParseBuffer
#define tidy_get_root tidyGetRoot
#define tidy_get_next tidyGetNext
#define tidy_get_child tidyGetChild
#define tidy_node_get_text tidyNodeGetText
#define tidy_node_get_name tidyNodeGetName
#define tidy_attr_first tidyAttrFirst
#define tidy_attr_name tidyAttrName
#define tidy_attr_value tidyAttrValue
#define tidy_attr_next tidyAttrNext
#define tidy_release tidyRelease
#define tidy_opt_set_bool tidyOptSetBool

void __tidy_release(const tidy_doc_t* const* ptr);

#define __tidy_release__ __attribute__((__cleanup__(__tidy_release)))

#pragma once
