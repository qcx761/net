char* removeStars(char* s) {
    char* res = malloc(strlen(s) + 1);
    int len = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] != '*') {
            res[len++] = s[i];
        } else {
            len--;
        }
    }
    res[len] = '\0';
    return res;
}
