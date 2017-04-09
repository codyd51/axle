#include "plistman.h"
#include <kernel/util/yxml/yxml.h>
#include <std/kheap.h>
#include <std/memory.h>

#define BUFSIZE 1024
#define PROC_MASTER_PERMISSION 1 << 0

static char* read_file(FILE* file) {
	char* raw = kmalloc(BUFSIZE);
	memset(raw, 0, BUFSIZE);
	int i = 0;
	for (; i < BUFSIZE; i++) {
		raw[i] = fgetc(file);
		if (raw[i] == EOF) break;
	}
	raw[i] = '\0';
	return raw;
}

void plist_parse(plist_t* plist, FILE* file) {
	memset(plist, 0, sizeof(plist));
	//task_t->permissions
	uint32_t permissions = 0;

	char* doc = read_file(file);
	char* raw = doc;

	yxml_t* x = kmalloc(sizeof(yxml_t) + BUFSIZE);
	memset(x, 0, sizeof(yxml_t) + BUFSIZE);
	yxml_init(x, x+1, BUFSIZE);

	int content_len[64] = {0};

	for(; *doc; doc++) {
		yxml_ret_t r = yxml_parse(x, *doc);
		if(r < 0) {
			if (r == YXML_EREF || r == YXML_ESTACK || r == YXML_ESYN) {
				printf("Bad XML (");
				switch (r) {
					case YXML_EREF:
						printf("unexpected EOF");
						break;
					case YXML_ESTACK:
						printf("stack overflow");
						break;
					case YXML_ESYN:
						printf("syntax error");
						break;
					default:
						printf("%d", r);
						break;
				}
				printf(") %d %s\n", *doc, x->data);
			}
			else if (r == YXML_ECLOSE) {
				printf("Bad close tag\n");
			}
			break;
		}
		else if (r == YXML_OK) {
			continue;
		}
		else if (r == YXML_ATTREND) {
			strcpy(&(plist->keys[plist->key_count++]), x->attr);
		}
		else if (r == YXML_ATTRVAL) {
			//it's so beautiful ;_;
			plist->vals[plist->key_count][content_len[plist->key_count]++] = x->data[0];
		}
	}

	yxml_ret_t r = yxml_eof(x);
	if (r == YXML_EEOF) {
		printf("Malformed XML\n");
	}

	kfree(x);
	kfree(raw);
}

char* plist_val_for_key(plist_t* plist, char* key) {
	for (int i = 0; i < plist->key_count; i++) {
		if (!strcmp(plist->keys, key)) {
			return plist->vals[i];
		}
	}
	return NULL;
}

char* plist_key_for_val(plist_t* plist, char* val) {
	for (int i = 0; i < plist->key_count; i++) {
		if (!strcmp(plist->vals, val)) {
			return plist->keys[i];
		}
	}
	return NULL;
}

