#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *infp = fopen("wordle-past-words.txt", "r");
    FILE *outfp = fopen("wordle-past-words-06.02.26.txt", "w");
    char buffer[1024];
    while (fgets(buffer, 1024, infp) != NULL) {
        for (int i = 0; i < 5; i++) {
            buffer[i] += 32;
        }
        buffer[5] = '\0';
        fprintf(outfp, "%s\n", buffer);
    }
    fclose(infp);
    fclose(outfp);
}