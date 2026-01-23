// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>

size_t strlen(const char *str);

char *strcpy(char *destination, const char *source) {
	/* TODO: Implement strcpy(). */
	int i = 0;
	while (source[i] != '\0') {
		destination[i] = source[i];
		i++;
	}
	destination[i] = '\0';

	return destination;
}

char *strncpy(char *destination, const char *source, size_t len) {
	/* TODO: Implement strncpy(). */
	for (unsigned int i = 0; i < len; i++) {
		destination[i] = source[i];

		if (source[i] == '\0') {
			for (unsigned int j = i + 1; j < len; j++) {
				destination[j] = '\0';
			}
		}
	}

	return destination;
}

char *strcat(char *destination, const char *source) {
	/* TODO: Implement strcat(). */
	int dest_len = strlen(destination);

	int i = 0;
	while (source[i] != '\0') {
		destination[i + dest_len] = source[i];
		i++;
	}
	destination[i + dest_len] = '\0';

	return destination;
}

char *strncat(char *destination, const char *source, size_t len) {
	/* TODO: Implement strncat(). */
	int dest_len = strlen(destination);

	unsigned int i;
	for (i = 0; i < len && source[i] != '\0'; i++) {
		destination[i + dest_len] = source[i];
	}
	destination[i + dest_len] = '\0';

	return destination;
}

int strcmp(const char *str1, const char *str2) {
	/* TODO: Implement strcmp(). */

	int i = 0;
	while (str1[i] == str2[i] && str1[i] != '\0') {
		i++;
	}

	if (str1[i] < str2[i]) {
		return -1;
	} else if (str1[i] > str2[i]) {
		return 1;
	} else {
		return 0;
	}
}

int strncmp(const char *str1, const char *str2, size_t len) {
	/* TODO: Implement strncmp(). */

	unsigned int i = 0;
	while (str1[i] == str2[i] && str1[i] != '\0' && i < len) {
		i++;
	}

	if (i == len) {
		return 0;
	}

	if (str1[i] < str2[i]) {
		return -1;
	} else if (str1[i] > str2[i]) {
		return 1;
	} else {
		return 0;
	}
}

size_t strlen(const char *str) {
	size_t i = 0;

	for (; *str != '\0'; str++, i++)
		;

	return i;
}

char *strchr(const char *str, int c) {
	/* TODO: Implement strchr(). */

	for (unsigned int i = 0; i <= strlen(str); i++) {
		if (str[i] == c) {
			return (char *)(str + i);
		}
	}

	return NULL;
}

char *strrchr(const char *str, int c) {
	/* TODO: Implement strrchr(). */

	for (int i = strlen(str); i >= 0; i--) {
		if (str[i] == c) {
			return (char *)(str + i);
		}
	}

	return NULL;
}

char *strstr(const char *haystack, const char *needle) {
	/* TODO: Implement strstr(). */

	unsigned int needle_len = strlen(needle), i, j;

	for (i = 0; i + needle_len <= strlen(haystack); i++) {
		j = 0;
		while (j < needle_len && haystack[i + j] == needle[j]) {
			j++;
		}

		if (j == needle_len) {
			return (char *)(haystack + i);
		}
	}

	return NULL;
}

char *strrstr(const char *haystack, const char *needle) {
	/* TODO: Implement strrstr(). */

	unsigned int needle_len = strlen(needle), j;

	for (int i = strlen(haystack) - needle_len; i >= 0; i--) {
		j = 0;
		while (j < needle_len && haystack[i + j] == needle[j]) {
			j++;
		}

		if (j == needle_len) {
			return (char *)(haystack + i);
		}
	}

	return NULL;
}

void *memcpy(void *destination, const void *source, size_t num) {
	/* TODO: Implement memcpy(). */

	for (unsigned int i = 0; i < num; i++) {
		((char *)destination)[i] = ((char *)source)[i];
	}

	return destination;
}

void *memmove(void *destination, const void *source, size_t num) {
	/* TODO: Implement memmove(). */

	if (destination <= source) {
		for (unsigned int i = 0; i < num; i++) {
			((char *)destination)[i] = ((char *)source)[i];
		}
	} else {
		for (int i = num - 1; i >= 0; i--) {
			((char *)destination)[i] = ((char *)source)[i];
		}
	}

	return destination;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num) {
	/* TODO: Implement memcmp(). */

	unsigned int i = 0;
	while (((char *)ptr1)[i] == ((char *)ptr2)[i] && i < num) {
		i++;
	}

	if (i == num) {
		return 0;
	}

	if (((char *)ptr1)[i] < ((char *)ptr2)[i]) {
		return -1;
	} else if (((char *)ptr1)[i] > ((char *)ptr2)[i]) {
		return 1;
	} else {
		return 0;
	}
}

void *memset(void *source, int value, size_t num) {
	/* TODO: Implement memset(). */

	for (unsigned int i = 0; i < num; i++) {
		((char *)source)[i] = (unsigned char)value;
	}

	return source;
}
