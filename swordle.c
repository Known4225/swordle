/*
Created by Ryan Srichai, 31.01.26

All words source: https://gist.github.com/dracos/dd0668f281e685bad51479e5acaadb93#file-valid-wordle-words-txt
Valid answers source: https://gist.github.com/cfreshman/a03ef2cba789d8cf00c08f767e0fad7b
*/

#include "turtle.h"
#include <time.h>
#include <pthread.h>

enum {
    S_KEY_LMB = 0,
    S_KEY_RMB = 1,
    S_KEY_ENTER = 2,
    S_KEY_BACKSPACE = 3,
};

typedef enum {
    SWORDLE_COLOR_TEXT = 0,
    SWORDLE_COLOR_GREEN = 1,
    SWORDLE_COLOR_YELLOW = 2,
    SWORDLE_COLOR_GREY = 3, // also the same as border non-highlighted
    SWORDLE_COLOR_BORDER_HIGHLIGHT = 4,
    SWORDLE_COLOR_BACKGROUND = 5,
    SWORDLE_COLOR_KEYBOARD = 6,
    SWORDLE_COLOR_SIDEBAR = 7,
} swordle_color_t;

typedef struct {
    char canvas[60]; // character, colour
    double colors[90];
    double canvasX; // x position of canvas (top left corner)
    double canvasY; // y position of canvas (top left corner)
    double dropX; // canvas X coordinates per letter
    double dropY; // canvas Y coordinates per letter
    double boxPercentage; // value from 0 to 1 that determines how big the box is relative to the dropX * dropY area
    int32_t mouseIndex; // canvas hover index (halved)
    int32_t cursorIndex; // canvas cursor index (halved)
    char keys[8];
    list_t *possibleWords;
    list_t *allWords;
    list_t *canvasPossible;
    list_t *best;
    /* sidebars */
    double bestX;
    int32_t bestIndex;
    double bestOffset;
    tt_scrollbar_t *bestScrollbar;
    double possibleX;
    int32_t possibleIndex;
    double possibleOffset;
    tt_scrollbar_t *possibleScrollbar;
    /* keyboard */
    double keyX[3]; // top, middle, bottom
    double keyY; // top left
    double keyDropX; // key width
    double keyDropSpecialX; // key width of ENTER and BACKSPACE keys
    double keyDropY; // key height
    double keyPercentage; // value from 0 to 1 that determines how big the key is relative to the keyDropX * keyDropY area
    /* solver thread */
    volatile int8_t solving;
    volatile int8_t solverThreadExists;

    int32_t tick;
} swordle_t;

swordle_t self;

void swordle_setColor(swordle_color_t color) {
    turtlePenColor(self.colors[color * 3], self.colors[color * 3 + 1], self.colors[color * 3 + 2]);
}

void printCanvas(char *canvas) {
    for (int32_t j = 0; j < 6; j++) {
        if (canvas[j * 10] == 0) {
            continue;
        }
        for (int32_t i = 0; i < 5; i++) {
            printf("%c %d ", canvas[j * 10 + i * 2], canvas[j * 10 + i * 2 + 1]);
        }
        printf("\n");
    }
};

void swordleUnicodeCallback(uint32_t codepoint) {
    if (codepoint >= 97 && codepoint <= 122) {
        codepoint -= 32;
    }
    if (codepoint >= 65 && codepoint <= 90 && self.cursorIndex < 30) {
        self.canvas[self.cursorIndex * 2] = codepoint;
        if (self.canvas[self.cursorIndex * 2 + 1] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
            self.canvas[self.cursorIndex * 2 + 1] = SWORDLE_COLOR_GREY;
        }
        self.cursorIndex++;
    }
}

/* returns number of possible words given a canvas */
int32_t simulate(char *canvas, list_t *possibleWords) {
    /* create word whitelist and global count */
    int8_t count[26] = {0}; // need to have at least count[letter] of a particular letter, if count[letter] is negative then you need to have exactly -count[letter] in a word
    uint32_t lookup[26];
    for (int32_t i = 0; i < 26; i++) {
        lookup[i] = pow(2, i);
    }
    uint32_t whitelist[5] = {0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF}; // so i just learned today that you can only do one of these when it is 0
    for (int32_t j = 0; j < 6; j++) {
        int8_t currentCount[26] = {0};
        for (int32_t i = 0; i < 5; i++) {
            if (canvas[j * 10 + i * 2] == 0) {
                continue;
            }
            switch (canvas[j * 10 + i * 2 + 1]) {
                case SWORDLE_COLOR_GREEN:
                    if (currentCount[canvas[j * 10 + i * 2] - 65] < 0) {
                        currentCount[canvas[j * 10 + i * 2] - 65]--;
                    } else {
                        currentCount[canvas[j * 10 + i * 2] - 65]++;
                    }
                    whitelist[i] = lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                break;
                case SWORDLE_COLOR_YELLOW:
                    for (int32_t k = 0; k < i; k++) {
                        if (canvas[j * 10 + k * 2] == canvas[j * 10 + i * 2] && canvas[j * 10 + k * 2 + 1] == SWORDLE_COLOR_GREY) {
                            return 0;
                        }
                    }
                    currentCount[canvas[j * 10 + i * 2] - 65]++;
                    whitelist[i] &= ~lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                break;
                case SWORDLE_COLOR_GREY:
                    uint32_t blacklist = ~lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                    int32_t startingIndex = i;
                    if (currentCount[canvas[j * 10 + i * 2] - 65] == 0) {
                        for (int32_t k = 0; k < 5; k++) {
                            whitelist[k] &= blacklist;
                        }
                    } else {
                        currentCount[canvas[j * 10 + i * 2] - 65] *= -1;
                        whitelist[i] &= ~lookup[canvas[j * 10 + i * 2] - 65];
                    }
                break;
                default:
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            if (count[i] >= 0 && abs(currentCount[i]) > count[i]) {
                count[i] = currentCount[i];
            }
        }
    }
    /* gather all possible words */
    int32_t canvasPossible = 0;
    for (int32_t i = 0; i < possibleWords -> length; i++) {
        char *word = possibleWords -> data[i].s;
        char good = 1;
        int8_t currentCount[26] = {0};
        /* check whitelist */
        for (int32_t j = 0; j < 5; j++) {
            currentCount[word[j] - 65]++;
            if ((whitelist[j] & lookup[word[j] - 65]) == 0) { // wordlists use capital letters
                good = 0;
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            /* check if minimum global count is met */
            if (abs(count[i]) > currentCount[i]) {
                good = 0;
                break;
            }
            /* check if exact global count is met (if information is available) */
            if (count[i] < 0 && currentCount[i] != -count[i]) {
                good = 0;
                break;
            }
        }
        if (good) {
            canvasPossible++;
        }
    }
    return canvasPossible;
}

/* Algorithm: minimise number of possible words, returns a list of all possible words (given current canvas) */
list_t *bestWord(char *returnWord, list_t *bestWords, char *canvas, list_t *possibleWords, list_t *allWords) {
    list_t *canvasPossible = list_init();
    /* create word whitelist and global count */
    int8_t count[26] = {0}; // need to have at least count[letter] of a particular letter, if count[letter] is negative then you need to have exactly -count[letter] in a word
    uint32_t lookup[26];
    for (int32_t i = 0; i < 26; i++) {
        lookup[i] = pow(2, i);
    }
    uint32_t whitelist[5] = {0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF}; // so i just learned today that you can only do one of these when it is 0
    for (int32_t j = 0; j < 6; j++) {
        int8_t currentCount[26] = {0};
        for (int32_t i = 0; i < 5; i++) {
            if (canvas[j * 10 + i * 2] == 0) {
                continue;
            }
            switch (canvas[j * 10 + i * 2 + 1]) {
                case SWORDLE_COLOR_GREEN:
                    if (currentCount[canvas[j * 10 + i * 2] - 65] < 0) {
                        currentCount[canvas[j * 10 + i * 2] - 65]--;
                    } else {
                        currentCount[canvas[j * 10 + i * 2] - 65]++;
                    }
                    whitelist[i] = lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                break;
                case SWORDLE_COLOR_YELLOW:
                    for (int32_t k = 0; k < i; k++) {
                        if (canvas[j * 10 + k * 2] == canvas[j * 10 + i * 2] && canvas[j * 10 + k * 2 + 1] == SWORDLE_COLOR_GREY) {
                            printf("bestWord: Invalid canvas configuration\n");
                            return canvasPossible;
                        }
                    }
                    currentCount[canvas[j * 10 + i * 2] - 65]++;
                    whitelist[i] &= ~lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                break;
                case SWORDLE_COLOR_GREY:
                    uint32_t blacklist = ~lookup[canvas[j * 10 + i * 2] - 65]; // canvas uses capital letters
                    int32_t startingIndex = i;
                    if (currentCount[canvas[j * 10 + i * 2] - 65] == 0) {
                        for (int32_t k = 0; k < 5; k++) {
                            whitelist[k] &= blacklist;
                        }
                    } else {
                        currentCount[canvas[j * 10 + i * 2] - 65] *= -1;
                        whitelist[i] &= ~lookup[canvas[j * 10 + i * 2] - 65];
                    }
                break;
                default:
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            if (count[i] >= 0 && abs(currentCount[i]) > count[i]) {
                count[i] = currentCount[i];
            }
        }
    }
    // for (int32_t i = 0; i < 26; i++) {
    //     printf("%d ", count[i]);
    // }
    // printf("\n");
    // printf("%X %X %X %X %X\n", whitelist[0], whitelist[1], whitelist[2], whitelist[3], whitelist[4]);
    /* gather all possible words */
    for (int32_t i = 0; i < possibleWords -> length; i++) {
        char *word = possibleWords -> data[i].s;
        char good = 1;
        int8_t currentCount[26] = {0};
        /* check whitelist */
        for (int32_t j = 0; j < 5; j++) {
            currentCount[word[j] - 65]++;
            if ((whitelist[j] & lookup[word[j] - 65]) == 0) { // wordlists use capital letters
                good = 0;
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            /* check if minimum global count is met */
            if (abs(count[i]) > currentCount[i]) {
                good = 0;
                break;
            }
            /* check if exact global count is met (if information is available) */
            if (count[i] < 0 && currentCount[i] != -count[i]) {
                good = 0;
                break;
            }
        }
        if (good) {
            list_append(canvasPossible, possibleWords -> data[i], 's');
        }
    }
    if (canvasPossible -> length == 0) {
        printf("bestWord: Failed due to no possible words\n");
        return canvasPossible;
    }
    /* simulate every possible next word, with every permutation */
    list_t *variance = list_init();
    char proposedCanvas[60];
    int32_t cursorIndex = 0;
    while (canvas[cursorIndex * 2] != 0 && cursorIndex < 30) {
        cursorIndex++;
    }
    if (cursorIndex >= 30 || cursorIndex % 5 != 0) {
        printf("bestWord: Failed due to improper cursorIndex\n");
        return canvasPossible;
    }
    for (int32_t i = 0; i < allWords -> length; i++) {
        memcpy(proposedCanvas, canvas, 60);
        for (int32_t j = 0; j < 5; j++) {
            proposedCanvas[cursorIndex * 2 + j * 2] = allWords -> data[i].s[j];
            proposedCanvas[cursorIndex * 2 + j * 2 + 1] = SWORDLE_COLOR_GREEN;
        }
        double mean = 0;
        int32_t values[243];
        for (int32_t j = 0; j < 243; j++) {
            values[j] = simulate(proposedCanvas, canvasPossible);
            mean += values[j];
            proposedCanvas[cursorIndex * 2 + 1]++;
            if (proposedCanvas[cursorIndex * 2 + 1] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
                proposedCanvas[cursorIndex * 2 + 1] = SWORDLE_COLOR_GREEN;
                proposedCanvas[cursorIndex * 2 + 3]++;
                if (proposedCanvas[cursorIndex * 2 + 3] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
                    proposedCanvas[cursorIndex * 2 + 3] = SWORDLE_COLOR_GREEN;
                    proposedCanvas[cursorIndex * 2 + 5]++;
                    if (proposedCanvas[cursorIndex * 2 + 5] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
                        proposedCanvas[cursorIndex * 2 + 5] = SWORDLE_COLOR_GREEN;
                        proposedCanvas[cursorIndex * 2 + 7]++;
                        if (proposedCanvas[cursorIndex * 2 + 7] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
                            proposedCanvas[cursorIndex * 2 + 7] = SWORDLE_COLOR_GREEN;
                            proposedCanvas[cursorIndex * 2 + 9]++;
                            if (proposedCanvas[cursorIndex * 2 + 9] == SWORDLE_COLOR_BORDER_HIGHLIGHT) {
                                proposedCanvas[cursorIndex * 2 + 9] = SWORDLE_COLOR_GREEN;
                            }
                        }
                    }
                }
            }
        }
        // printf("%s ", allWords -> data[i].s);
        // printf("total: %lf ", mean);
        mean /= 243.0;
        /* take variance */
        double v = 0;
        for (int32_t j = 0; j < 243; j++) {
            v += (values[j] - mean) * (values[j] - mean);
        }
        v /= 243.0;
        // printf("variance: %lf\n", v);
        list_append(variance, (unitype) v, 'd');
    }
    for (int32_t i = 0; i < allWords -> length; i++) {
        double epsilon = 0.001; // arbitrary advantage towards words that could actually be the word - meant only to break ties between these words and other words that separate all remaining possible words into distinct cases (i would prefer one of those cases to be all green if possible)
        if (list_find(canvasPossible, allWords -> data[i], 's') != -1) {
            variance -> data[i].d -= epsilon;
        }
    }
    list_t *order = list_sort_index_double(variance);
    int32_t gatherTop = 25;
    if (order -> length < gatherTop) {
        gatherTop = order -> length;
    }
    for (int32_t i = 0; i < gatherTop; i++) {
        list_append(bestWords, allWords -> data[order -> data[order -> length - i - 1].i], 's');
        list_append(bestWords, variance -> data[order -> data[order -> length - i - 1].i], 'd');
    }
    // printf("%s variance: %lf\n", allWords -> data[order -> data[order -> length - 1].i].s, variance -> data[order -> data[order -> length - 1].i].d);
    memcpy(returnWord, allWords -> data[order -> data[order -> length - 1].i].s, 6);
    list_free(order);
    return canvasPossible;
}

void *solverThread(void *arg) {
    while (self.solverThreadExists) {
        if (self.solving == 1) {
            self.solving = 2;
            char word[6] = {0};
            list_t *best = list_init();
            list_t *canvasPossible = bestWord(word, best, self.canvas, self.possibleWords, self.allWords);
            list_copy(self.best, best);
            list_copy(self.canvasPossible, canvasPossible);
            list_free(best);
            list_free(canvasPossible);
            self.solving = 0;
        }
    }
}

BOOL WINAPI swordleSignal(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        self.solverThreadExists = 0;
    }
    return 0;
}

void init() {
    /* initialise canvas */
    for (int32_t i = 0; i < 60; i += 2) {
        self.canvas[i] = 0;
        self.canvas[i + 1] = SWORDLE_COLOR_BORDER_HIGHLIGHT;
    }
    self.canvasX = -73.5;
    self.canvasY = 150;
    self.dropX = 30;
    self.dropY = -30;
    self.boxPercentage = 0.9;
    self.mouseIndex = -1;
    /* colors */
    double copyColors[] = {
        248, 248, 248,
        83, 141, 78,
        181, 159, 59,
        58, 58, 60,
        86, 87, 88,
        18, 18, 19,
        129, 131, 132,
        9, 9, 10,
    };
    memcpy(self.colors, copyColors, sizeof(copyColors));
    /* keyboard */
    turtle.unicodeCallback = swordleUnicodeCallback;
    self.keyX[0] = -115;
    self.keyX[1] = -103;
    self.keyX[2] = -115;
    self.keyY = -50;
    self.keyDropX = 23.23;
    self.keyDropY = -32;
    self.keyDropSpecialX = 35.23;
    self.keyPercentage = 0.87;
    /* load words */
    self.possibleWords = list_init();
    FILE *possiblefp = fopen("wordle-answers-alphabetical.txt", "r");
    if (possiblefp != NULL) {
        char word[10];
        while (fgets(word, 10, possiblefp) != NULL) {
            for (int32_t i = 0; i < 5; i++) {
                if (word[i] >= 97 && word[i] <= 122) {
                    word[i] -= 32;
                }
            }
            word[5] = 0;
            list_append(self.possibleWords, (unitype) word, 's');
        }
        fclose(possiblefp);
    }
    self.allWords = list_init();
    FILE *allfp = fopen("valid-wordle-words.txt", "r");
    if (allfp != NULL) {
        char word[10];
        while (fgets(word, 10, allfp) != NULL) {
            for (int32_t i = 0; i < 5; i++) {
                if (word[i] >= 97 && word[i] <= 122) {
                    word[i] -= 32;
                }
            }
            word[5] = 0;
            list_append(self.allWords, (unitype) word, 's');
        }
        fclose(allfp);
    }
    self.canvasPossible = list_init();
    self.best = list_init();
    /* sidebars */
    self.bestX = -250;
    self.bestIndex = 0;
    self.bestOffset = 0;
    self.bestScrollbar = scrollbarInit(NULL, TT_SCROLLBAR_VERTICAL, self.bestX - 62, -8, 6, 330, 50);
    self.possibleX = 250;
    self.possibleIndex = 0;
    self.possibleOffset = 0;
    self.possibleScrollbar = scrollbarInit(NULL, TT_SCROLLBAR_VERTICAL, self.possibleX + 62, -8, 6, 330, 50);

    /* solver thread */
    self.solverThreadExists = 1;
    self.solving = 0;
    pthread_t solverThreadVar;
    pthread_create(&solverThreadVar, NULL, solverThread, NULL);
    /* signal handler */
    SetConsoleCtrlHandler(swordleSignal, TRUE);
}

void turtleRoundedRectangle(double x1, double y1, double x2, double y2, double radius) {
    if (x1 > x2) {
        double temp = x1;
        x1 = x2;
        x2 = temp;
    }
    if (y1 > y2) {
        double temp = y1;
        y1 = y2;
        y2 = temp;
    }
    turtlePenSize(radius * 2);
    turtleGoto(x1 + radius, y1 + radius);
    turtlePenDown();
    turtleGoto(x1 + radius, y2 - radius);
    turtleGoto(x2 - radius, y2 - radius);
    turtleGoto(x2 - radius, y1 + radius);
    turtleGoto(x1 + radius, y1 + radius);
    turtlePenUp();
    turtleRectangle(x1 + radius, y1 + radius, x2 - radius, y2 - radius);
}

void renderCanvas() {
    /* render mouse position */
    tt_setColor(TT_COLOR_TEXT);
    turtleTextWriteStringf(-310, -170, 5, 0, "%.2lf, %.2lf", turtle.mouseX, turtle.mouseY);
    self.mouseIndex = -1;
    /* render canvas */
    double ypos = self.canvasY;
    for (int32_t j = 0; j < 6; j++) {
        double xpos = self.canvasX;
        for (int32_t i = 0; i < 5; i++) {
            switch (self.canvas[j * 10 + i * 2 + 1]) {
                case SWORDLE_COLOR_GREEN:
                case SWORDLE_COLOR_YELLOW:
                case SWORDLE_COLOR_GREY:
                    swordle_setColor(self.canvas[j * 10 + i * 2 + 1]);
                    turtleRectangle(xpos, ypos, xpos + self.dropX * self.boxPercentage, ypos + self.dropY * self.boxPercentage);
                break;
                default:
                    swordle_setColor(SWORDLE_COLOR_BORDER_HIGHLIGHT);
                    turtlePenSize(0.6);
                    turtleGoto(xpos, ypos);
                    turtlePenDown();
                    turtleGoto(xpos + self.dropX * self.boxPercentage, ypos);
                    turtleGoto(xpos + self.dropX * self.boxPercentage, ypos + self.dropY * self.boxPercentage);
                    turtleGoto(xpos, ypos + self.dropY * self.boxPercentage);
                    turtleGoto(xpos, ypos);
                    turtlePenUp();
                break;
            }
            if (turtle.mouseX >= xpos && turtle.mouseX <= xpos + self.dropX * self.boxPercentage && turtle.mouseY >= ypos + self.dropY * self.boxPercentage && turtle.mouseY <= ypos) {
                self.mouseIndex = j * 5 + i;
            }
            if (self.canvas[j * 10 + i * 2] != 0) {
                swordle_setColor(SWORDLE_COLOR_TEXT);
                char string[2] = {0, 0};
                string[0] = self.canvas[j * 10 + i * 2];
                turtleTextWriteString(string, (xpos * 2 + self.dropX * self.boxPercentage) / 2, (ypos * 2 + self.dropY * self.boxPercentage) / 2, fabs(self.dropY * 0.5), 50);
            }
            xpos += self.dropX;
        }
        ypos += self.dropY;
    }
    /* render keyboard */
    char keys[] = {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '1', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '2'};
    double xpos = self.keyX[0];
    ypos = self.keyY;
    for (int32_t i = 0; i < 10; i++) {
        swordle_setColor(SWORDLE_COLOR_KEYBOARD);
        turtleRoundedRectangle(xpos, ypos, xpos + self.keyDropX * self.keyPercentage, ypos + self.keyDropY * self.keyPercentage, self.keyDropX / 10);
        char string[2] = {0, 0};
        string[0] = keys[i];
        swordle_setColor(SWORDLE_COLOR_TEXT);
        turtleTextWriteString(string, (xpos * 2 + self.keyDropX * self.keyPercentage) / 2, (ypos * 2 + self.keyDropY * self.keyPercentage) / 2, fabs(self.keyDropX * 0.5), 50);
        xpos += self.keyDropX;
    }
    ypos += self.keyDropY;
    xpos = self.keyX[1];
    for (int32_t i = 0; i < 9; i++) {
        swordle_setColor(SWORDLE_COLOR_KEYBOARD);
        turtleRoundedRectangle(xpos, ypos, xpos + self.keyDropX * self.keyPercentage, ypos + self.keyDropY * self.keyPercentage, self.keyDropX / 10);
        char string[2] = {0, 0};
        string[0] = keys[i + 10];
        swordle_setColor(SWORDLE_COLOR_TEXT);
        turtleTextWriteString(string, (xpos * 2 + self.keyDropX * self.keyPercentage) / 2, (ypos * 2 + self.keyDropY * self.keyPercentage) / 2, fabs(self.keyDropX * 0.5), 50);
        xpos += self.keyDropX;
    }
    ypos += self.keyDropY;
    xpos = self.keyX[2];
    for (int32_t i = 0; i < 9; i++) {
        if (keys[i + 19] == '1') {
            /* enter key */
            swordle_setColor(SWORDLE_COLOR_KEYBOARD);
            turtleRoundedRectangle(xpos, ypos, xpos + self.keyDropSpecialX - self.keyDropX * (1 - self.keyPercentage), ypos + self.keyDropY * self.keyPercentage, self.keyDropX / 10);
            swordle_setColor(SWORDLE_COLOR_TEXT);
            turtleTextWriteString("ENTER", (xpos * 2 + self.keyDropSpecialX - self.keyDropX * (1 - self.keyPercentage)) / 2, (ypos * 2 + self.keyDropY * self.keyPercentage) / 2, fabs(self.keyDropX * 0.5) / 2, 50);
            xpos += self.keyDropSpecialX;
        } else if (keys[i + 19] == '2') {
            /* backspace key */
            swordle_setColor(SWORDLE_COLOR_KEYBOARD);
            turtleRoundedRectangle(xpos, ypos, xpos + self.keyDropSpecialX - self.keyDropX * (1 - self.keyPercentage), ypos + self.keyDropY * self.keyPercentage, self.keyDropX / 10);
            swordle_setColor(SWORDLE_COLOR_TEXT);
            turtlePenSize(1);
            double symbolSize = 4;
            double centerX = (xpos * 2 + self.keyDropSpecialX - self.keyDropX * (1 - self.keyPercentage)) / 2 - symbolSize * 0.75;
            double centerY = (ypos * 2 + self.keyDropY * self.keyPercentage) / 2;
            turtleGoto(centerX - symbolSize * 0.75, centerY);
            turtlePenDown();
            turtleGoto(centerX, centerY - symbolSize);
            turtleGoto(centerX + symbolSize * 2, centerY - symbolSize);
            turtleGoto(centerX + symbolSize * 2, centerY + symbolSize);
            turtleGoto(centerX, centerY + symbolSize);
            turtleGoto(centerX - symbolSize * 0.75, centerY);
            turtlePenUp();
            centerX += symbolSize * 0.9;
            turtleGoto(centerX + symbolSize / 2, centerY + symbolSize / 2);
            turtlePenDown();
            turtleGoto(centerX - symbolSize / 2, centerY - symbolSize / 2);
            turtlePenUp();
            turtleGoto(centerX + symbolSize / 2, centerY - symbolSize / 2);
            turtlePenDown();
            turtleGoto(centerX - symbolSize / 2, centerY + symbolSize / 2);
            turtlePenUp();
        } else {
            swordle_setColor(SWORDLE_COLOR_KEYBOARD);
            turtleRoundedRectangle(xpos, ypos, xpos + self.keyDropX * self.keyPercentage, ypos + self.keyDropY * self.keyPercentage, self.keyDropX / 10);
            char string[2] = {0, 0};
            string[0] = keys[i + 19];
            swordle_setColor(SWORDLE_COLOR_TEXT);
            turtleTextWriteString(string, (xpos * 2 + self.keyDropX * self.keyPercentage) / 2, (ypos * 2 + self.keyDropY * self.keyPercentage) / 2, fabs(self.keyDropX * 0.5), 50);
            xpos += self.keyDropX;
        }
    }
}

void renderResults() {
    double xpos = self.bestX;
    double ypos = 150;
    swordle_setColor(SWORDLE_COLOR_SIDEBAR);
    turtleRectangle(-320, -180, xpos + 70, 180);
    if (self.solving) {
        tt_setColor(TT_COLOR_YELLOW);
        int32_t splitter = self.tick % 400;
        if (splitter < 100) {
            turtleTextWriteString("Searching", xpos, 0, 10, 50);
        } else if (splitter < 200) {
            turtleTextWriteString("Searching.", xpos, 0, 10, 50);
        } else if (splitter < 300) {
            turtleTextWriteString("Searching..", xpos, 0, 10, 50);
        } else {
            turtleTextWriteString("Searching...", xpos, 0, 10, 50);
        }
    } else {
        if (self.best -> length == 0) {
            swordle_setColor(SWORDLE_COLOR_TEXT);
            turtleTextWriteString("No Best Words", xpos, 0, 10, 50);
        } else {
            swordle_setColor(SWORDLE_COLOR_TEXT);
            turtleTextWriteString("Best", xpos - 30, ypos, 10, 50);
            turtleTextWriteString("Variance", xpos + 30, ypos, 10, 50);
            ypos -= 20;
            for (int32_t i = 0; i < self.best -> length; i += 2) {
                turtleTextWriteString(self.best -> data[i].s, xpos - 30, ypos, 6, 50);
                turtleTextWriteStringf(xpos + 30, ypos, 6, 50, "%.3lf", self.best -> data[i + 1].d);
                ypos -= 12;
            }
        }
    }
    xpos = self.possibleX;
    ypos = 150;
    swordle_setColor(SWORDLE_COLOR_SIDEBAR);
    turtleRectangle(320, -180, xpos - 70, 180);
    if (self.solving) {
        tt_setColor(TT_COLOR_YELLOW);
        int32_t splitter = self.tick % 400;
        if (splitter < 100) {
            turtleTextWriteString("Searching", xpos, 0, 10, 50);
        } else if (splitter < 200) {
            turtleTextWriteString("Searching.", xpos, 0, 10, 50);
        } else if (splitter < 300) {
            turtleTextWriteString("Searching..", xpos, 0, 10, 50);
        } else {
            turtleTextWriteString("Searching...", xpos, 0, 10, 50);
        }
    } else {
        list_t *selectList;
        if (self.canvasPossible -> length == 0) {
            // swordle_setColor(SWORDLE_COLOR_TEXT);
            // turtleTextWriteString("No Possible Words", xpos, 0, 10, 50);
            selectList = self.possibleWords;
        } else {
            selectList = self.canvasPossible;
        }
        swordle_setColor(SWORDLE_COLOR_TEXT);
        ypos -= 20;
        self.possibleIndex = 0;
        self.possibleOffset = 0;
        if (selectList -> length > 26) {
            self.possibleScrollbar -> enabled = TT_ELEMENT_ENABLED;
            self.possibleScrollbar -> barPercentage = 100 / (selectList -> length / 26);
            double divisor = selectList -> length * 100.0 / (selectList -> length - 26);
            self.possibleOffset = self.possibleScrollbar -> value * selectList -> length / divisor * 12;
            while (self.possibleOffset > 12) {
                self.possibleOffset -= 12;
                self.possibleIndex++;
            }
        } else {
            self.possibleScrollbar -> enabled = TT_ELEMENT_HIDE;
        }
        ypos += self.possibleOffset;
        int32_t possibleEnding = self.possibleIndex + 30;
        if (possibleEnding > selectList -> length) {
            possibleEnding = selectList -> length;
        }
        for (int32_t i = self.possibleIndex; i < possibleEnding; i++) {
            turtleTextWriteString(selectList -> data[i].s, xpos, ypos, 6, 50);
            ypos -= 12;
        }
        swordle_setColor(SWORDLE_COLOR_SIDEBAR);
        turtleRectangle(320, 150 - 16, xpos - 70, 180);
        swordle_setColor(SWORDLE_COLOR_TEXT);
        turtleTextWriteString("Possible", xpos, 150, 10, 50);
    }
}

void mouseTick() {
    if (turtleMouseDown()) {
        if (self.keys[S_KEY_LMB] == 0) {
            self.keys[S_KEY_LMB] = 1;
            if (self.mouseIndex != -1 && self.canvas[self.mouseIndex * 2] != 0) {
                self.canvas[self.mouseIndex * 2 + 1]--;
                if (self.canvas[self.mouseIndex * 2 + 1] < SWORDLE_COLOR_GREEN) {
                    self.canvas[self.mouseIndex * 2 + 1] = SWORDLE_COLOR_GREY;
                }
            }
        }
    } else {
        self.keys[S_KEY_LMB] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_ENTER)) {
        if (self.keys[S_KEY_ENTER] == 0) {
            self.keys[S_KEY_ENTER] = 1;
            if (self.cursorIndex % 5 == 0 && self.cursorIndex < 30) {
                self.possibleScrollbar -> enabled = TT_ELEMENT_HIDE;
                self.possibleScrollbar -> value = 0;
                self.bestScrollbar -> enabled = TT_ELEMENT_HIDE;
                self.bestScrollbar -> value = 0;
                self.solving = 1;
            }
        }
    } else {
        self.keys[S_KEY_ENTER] = 0;
    }
    if (turtleKeyPressed(GLFW_KEY_BACKSPACE)) {
        if (self.keys[S_KEY_BACKSPACE] < 2) {
            if (self.keys[S_KEY_BACKSPACE] == 0) {
                self.keys[S_KEY_BACKSPACE] = 10;
            } else {
                self.keys[S_KEY_BACKSPACE] = 2;
            }
            if (self.cursorIndex > 0) {
                self.cursorIndex--;
                self.canvas[self.cursorIndex * 2] = 0;
                self.canvas[self.cursorIndex * 2 + 1] = SWORDLE_COLOR_BORDER_HIGHLIGHT;
            }
        } else {
            self.keys[S_KEY_BACKSPACE]++;
            if (self.keys[S_KEY_BACKSPACE] == 5 || self.keys[S_KEY_BACKSPACE] > 60) {
                self.keys[S_KEY_BACKSPACE] = 1;
            }
        }
    } else {
        self.keys[S_KEY_BACKSPACE] = 0;
    }
    double scroll = turtleMouseWheel();
    if (scroll > 0) {
        // if () {
        //     self.
        // }
    } else if (scroll < 0) {
        
    }
}

void parseRibbonOutput() {
    if (tt_ribbon.output[0] == 0) {
        return;
    }
    tt_ribbon.output[0] = 0;
    if (tt_ribbon.output[1] == 0) { // File
        if (tt_ribbon.output[2] == 1) { // New
            list_clear(osToolsFileDialog.selectedFilenames);
            /* initialise canvas */
            for (int32_t i = 0; i < 60; i += 2) {
                self.canvas[i] = 0;
                self.canvas[i + 1] = SWORDLE_COLOR_BORDER_HIGHLIGHT;
            }
            self.cursorIndex = 0;
            list_clear(self.best);
            list_clear(self.canvasPossible);
        }
        if (tt_ribbon.output[2] == 2) { // Save
            if (osToolsFileDialog.selectedFilenames -> length == 0) {
                if (osToolsFileDialogSave(OSTOOLS_FILE_DIALOG_FILE, "Save.txt", NULL) != -1) {
                    printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
                }
            } else {
                printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
            }
        }
        if (tt_ribbon.output[2] == 3) { // Save As...
            list_clear(osToolsFileDialog.selectedFilenames);
            if (osToolsFileDialogSave(OSTOOLS_FILE_DIALOG_FILE, "Save.txt", NULL) != -1) {
                printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
            }
        }
        if (tt_ribbon.output[2] == 4) { // Open
            list_clear(osToolsFileDialog.selectedFilenames);
            if (osToolsFileDialogOpen(OSTOOLS_FILE_DIALOG_MULTIPLE_SELECT, OSTOOLS_FILE_DIALOG_FILE, "", NULL) != -1) {
                printf("Loaded data from: ");
                list_print(osToolsFileDialog.selectedFilenames);
            }
        }
    }
    if (tt_ribbon.output[1] == 1) { // Edit
        if (tt_ribbon.output[2] == 1) { // Undo
            printf("Undo\n");
        }
        if (tt_ribbon.output[2] == 2) { // Redo
            printf("Redo\n");
        }
        if (tt_ribbon.output[2] == 3) { // Cut
            osToolsClipboardSetText("test123");
            printf("Cut \"test123\" to clipboard!\n");
        }
        if (tt_ribbon.output[2] == 4) { // Copy
            osToolsClipboardSetText("test345");
            printf("Copied \"test345\" to clipboard!\n");
        }
        if (tt_ribbon.output[2] == 5) { // Paste
            osToolsClipboardGetText();
            printf("Pasted \"%s\" from clipboard!\n", osToolsClipboard.text);
        }
    }
    if (tt_ribbon.output[1] == 2) { // View
        if (tt_ribbon.output[2] == 1) { // Change theme
            printf("Change theme\n");
            if (tt_theme == TT_THEME_DARK) {
                turtleBgColor(36, 30, 32);
                turtleToolsSetTheme(TT_THEME_COLT);
            } else if (tt_theme == TT_THEME_COLT) {
                turtleBgColor(212, 201, 190);
                turtleToolsSetTheme(TT_THEME_NAVY);
            } else if (tt_theme == TT_THEME_NAVY) {
                turtleBgColor(255, 255, 255);
                turtleToolsSetTheme(TT_THEME_LIGHT);
            } else if (tt_theme == TT_THEME_LIGHT) {
                turtleBgColor(30, 30, 30);
                turtleToolsSetTheme(TT_THEME_DARK);
            }
        } 
        if (tt_ribbon.output[2] == 2) { // GLFW
            printf("GLFW settings\n");
        } 
    }
}

int main(int argc, char *argv[]) {
    /* Initialise glfw */
    if (!glfwInit()) {
        return -1;
    }
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA (Anti-Aliasing) with 4 samples (must be done before window is created (?))

    /* Create a windowed mode window and its OpenGL context */
    const GLFWvidmode *monitorSize = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int32_t windowHeight = monitorSize -> height;
    double optimizedScalingFactor = 0.8; // Set this number to 1 on windows and 0.8 on Ubuntu for maximum compatibility (fixes issue with incorrect stretching)
    #ifdef OS_WINDOWS
    optimizedScalingFactor = 1;
    #endif
    #ifdef OS_LINUX
    optimizedScalingFactor = 0.8;
    #endif
    GLFWwindow *window = glfwCreateWindow(windowHeight * 16 / 9 * optimizedScalingFactor, windowHeight * optimizedScalingFactor, "swordle", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetWindowSizeLimits(window, windowHeight * 16 / 9 * 0.4, windowHeight * 0.4, windowHeight * 16 / 9 * optimizedScalingFactor, windowHeight * optimizedScalingFactor);
    /* initialise logo */
    GLFWimage icon;
    int32_t iconChannels;
    char constructedFilepath[5120];
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "images/thumbnail.jpg");
    // uint8_t *iconPixels = stbi_load(constructedFilepath, &icon.width, &icon.height, &iconChannels, 4); // 4 color channels for RGBA
    // if (iconPixels != NULL) {
    //     icon.pixels = iconPixels;
    //     glfwSetWindowIcon(window, 1, &icon);
    //     glfwPollEvents(); // update taskbar icon correctly on windows - https://github.com/glfw/glfw/issues/2753
    //     free(iconPixels);
    // } else {
    //     printf("Could not load thumbnail %s\n", constructedFilepath);
    // }

    /* initialise turtle */
    turtleInit(window, -320, -180, 320, 180);
    #ifdef OS_LINUX
    glfwSetWindowPos(window, 0, 36);
    #endif
    if (optimizedScalingFactor > 0.85) {
        glfwSetWindowSize(window, windowHeight * 16 / 9 * 0.865, windowHeight * 0.85); // doing it this way ensures the window spawns in the top left of the monitor and fixes resizing limits
    } else {
        glfwSetWindowSize(window, windowHeight * 16 / 9 * optimizedScalingFactor, windowHeight * optimizedScalingFactor);
    }
    /* initialise osTools */
    osToolsInit(argv[0], window); // must include argv[0] to get executableFilepath, must include GLFW window for copy paste and cursor functionality
    osToolsFileDialogAddGlobalExtension("txt"); // add txt to extension restrictions
    osToolsFileDialogAddGlobalExtension("csv"); // add csv to extension restrictions
    /* initialise turtleText */
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "config/roberto.tgl");
    turtleTextInit(constructedFilepath);
    /* initialise turtleTools ribbon */
    turtleToolsSetTheme(TT_THEME_DARK); // dark theme preset
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "config/ribbonConfig.txt");
    ribbonInit(constructedFilepath);
    // list_t *ribbonConfig = list_init();
    // list_append(ribbonConfig, (unitype) "File, New, Save, Save As..., Open", 's');
    // list_append(ribbonConfig, (unitype) "Edit, Undo, Redo, Cut, Copy, Paste", 's');
    // list_append(ribbonConfig, (unitype) "View, Change Theme, GLFW", 's');
    // ribbonInitList(ribbonConfig);

    init();

    uint32_t tps = 120; // ticks per second (locked to fps in this case)
    clock_t start, end;

    while (turtle.close == 0) {
        start = clock();
        turtleGetMouseCoords();
        turtleClear();
        renderCanvas();
        renderResults();
        mouseTick();
        turtleToolsUpdate(); // update turtleTools
        parseRibbonOutput(); // user defined function to use ribbon
        turtleUpdate(); // update the screen
        end = clock();
        while ((double) (end - start) / CLOCKS_PER_SEC < (1.0 / tps)) {
            end = clock();
        }
        self.tick++;
    }
    self.solverThreadExists = 0;
    turtleFree();
    glfwTerminate();
    return 0;
}
