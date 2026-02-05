#include "util.h"

void strcat(char *destination, char *source) {
}

int strlen(char *str) {
	int i = 0;
	while(*str != '\0' && *str) {
		i++;
		str++;
	}
	return i;
}

char *strpbrk(char *str, char *delim) {
	const char *string = str;
	while(*string != '\0' && *string) {
		const char *delimeter = delim;
		while(*delimeter != '\0' && *delimeter) {
			if(*string == *delimeter) {
				return (char *)string;
			}
			delimeter++;
		}
		string++;
	}
	return NULL;
}

char *strtok(char *str, const char *delim) {
	static char *last_pos = NULL;

	if(str != NULL) {
		last_pos = str;
	}

	if(last_pos == NULL || *last_pos == '\0') {
		return NULL;
	}
	
	while (*last_pos != '\0') {
        	int is_delim = 0;
        	for (int i = 0; delim[i] != '\0'; i++) {
			if (*last_pos == delim[i]) {
               			 is_delim = 1;
               			 break;
	    		}
        	}
        	if (!is_delim) break; // Ketemu karakter yang bukan pembatas
		last_pos++;
    	}

	if(*last_pos == '\0') return NULL;

	char *token_start = last_pos;

	last_pos = strpbrk(token_start, delim);

	if(last_pos != NULL) {
		*last_pos = '\0';

		last_pos++;
	}
	return token_start;
}
