#include "character.h"

bool isalpha(char ch) {
	char* az = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (int i = 0; i < strlen(az); i++) {
		if (ch == az[i]) return true;
	}
	return false;
}

bool isalnum(char ch) {
	char* nums = "0123456789";
	for (int i = 0; i < strlen(nums); i++) {
		if (ch == nums[i]) return true;
	}
	return isalpha(ch);
}

bool isupper(char ch) {
	char* up = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (int i = 0; i < strlen(up); i++) {
		if (ch == up[i]) return true;
	}
	return false;
}
char toupper(char ch) {
	//if already uppercase or not an alphabetic, just return the character
	if (isupper(ch) || !isalpha(ch)) return ch;

	return ch - 32;
}

char tolower(char ch) {
	//if already lowercase or not an alphabetic, just return the character
	if (!isupper(ch) || !isalpha(ch)) return ch;

	return ch + 32;
}
