#include "util.h"

void strcat(char *destination, char *source) {
    while(*destination) destination++;
    while((*destination++ = *source++));
}

void strcpy(char *destination, char *source) {
    while((*destination++ = *source++));
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

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    do {
        if (*s == (char)c)
            last = s;
    } while (*s++);
    return (char *)last;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) {
            return NULL;
        }
    }
    return (char *)s;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.


void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}
