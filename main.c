#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MEMORY_SIZE 100
#define WORD_SIZE 4
#define CARD_WORDS 10

char M[MEMORY_SIZE][WORD_SIZE];
char IR[WORD_SIZE];
char R[WORD_SIZE];
int IC;
int C;
int SI;
int terminated;
int aborted;

FILE *inputFile;
FILE *outputFile;

void initMemory(void) {
    for (int i = 0; i < MEMORY_SIZE; i++)
        for (int j = 0; j < WORD_SIZE; j++)
            M[i][j] = ' ';

    for (int i = 0; i < WORD_SIZE; i++) {
        IR[i] = ' ';
        R[i] = ' ';
    }
    IC = 0;
    C = 0;
    SI = 0;
    terminated = 0;
    aborted = 0;
}

void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

void trim_leading_spaces(char *s) {
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

int is_control(const char *s, const char *control) {
    return strncmp(s, control, strlen(control)) == 0;
}

void store_card(const char *card, int *m) {
    int len = (int)strlen(card);
    if (len > 40) len = 40;

    if (*m + CARD_WORDS > MEMORY_SIZE) {
        aborted = 1;
        fprintf(outputFile, "MEMORY EXCEEDED\n\n");
        return;
    }

    for (int word = 0; word < CARD_WORDS; word++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = word * WORD_SIZE + j;
            M[*m + word][j] = (pos < len) ? card[pos] : ' ';
        }
    }
    *m += CARD_WORDS;
}

int get_address(void) {
    if (!isdigit((unsigned char)IR[2]) || !isdigit((unsigned char)IR[3]))
        return -1;
    return (IR[2] - '0') * 10 + (IR[3] - '0');
}

void READ(void) {
    char data[256];
    if (!fgets(data, sizeof(data), inputFile)) {
        fprintf(outputFile, "OUT OF DATA\n\n");
        aborted = 1;
        terminated = 1;
        return;
    }
    trim_newline(data);

    if (is_control(data, "$END")) {
        fprintf(outputFile, "OUT OF DATA\n\n");
        aborted = 1;
        terminated = 1;
        return;
    }

    int address = get_address();
    if (address < 0 || address + 9 >= MEMORY_SIZE) {
        fprintf(outputFile, "INVALID READ ADDRESS\n\n");
        aborted = 1;
        terminated = 1;
        return;
    }

    int len = (int)strlen(data);
    if (len > 40) len = 40;
    for (int word = 0; word < CARD_WORDS; word++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = word * WORD_SIZE + j;
            M[address + word][j] = (pos < len) ? data[pos] : ' ';
        }
    }
}

void WRITE(void) {
    int address = get_address();
    if (address < 0 || address + 9 >= MEMORY_SIZE) {
        fprintf(outputFile, "INVALID WRITE ADDRESS\n\n");
        aborted = 1;
        terminated = 1;
        return;
    }

    for (int word = 0; word < CARD_WORDS; word++) {
        for (int j = 0; j < WORD_SIZE; j++)
            fputc(M[address + word][j], outputFile);
    }
    fputc('\n', outputFile);
}

void TERMINATE(void) {
    fprintf(outputFile, "\n\n");
    terminated = 1;
}

void MOS(void) {
    switch (SI) {
        case 1:
            READ();
            break;
        case 2:
            WRITE();
            break;
        case 3:
            TERMINATE();
            break;
        default:
            fprintf(outputFile, "INVALID SUPERVISOR CALL\n\n");
            aborted = 1;
            terminated = 1;
    }
    SI = 0;
}

void EXECUTEUSERPROGRAM(void) {
    IC = 0;
    terminated = 0;
    aborted = 0;

    while (!terminated) {
        if (IC < 0 || IC >= MEMORY_SIZE) {
            fprintf(outputFile, "INVALID INSTRUCTION ADDRESS\n\n");
            aborted = 1;
            terminated = 1;
            break;
        }

        for (int i = 0; i < WORD_SIZE; i++)
            IR[i] = M[IC][i];
        IC++;

        char opcode[3] = { IR[0], IR[1], '\0' };
        int address = get_address();

        if (strcmp(opcode, "LR") == 0) {
            if (address < 0 || address >= MEMORY_SIZE) {
                fprintf(outputFile, "INVALID LR ADDRESS\n\n");
                aborted = 1;
                terminated = 1;
                continue;
            }
            memcpy(R, M[address], WORD_SIZE);
        }
        else if (strcmp(opcode, "SR") == 0) {
            if (address < 0 || address >= MEMORY_SIZE) {
                fprintf(outputFile, "INVALID SR ADDRESS\n\n");
                aborted = 1;
                terminated = 1;
                continue;
            }
            memcpy(M[address], R, WORD_SIZE);
        }
        else if (strcmp(opcode, "CR") == 0) {
            if (address < 0 || address >= MEMORY_SIZE) {
                fprintf(outputFile, "INVALID CR ADDRESS\n\n");
                aborted = 1;
                terminated = 1;
                continue;
            }
            C = (memcmp(R, M[address], WORD_SIZE) == 0);
        }
        else if (strcmp(opcode, "BT") == 0) {
            if (address < 0 || address >= MEMORY_SIZE) {
                fprintf(outputFile, "INVALID BT ADDRESS\n\n");
                aborted = 1;
                terminated = 1;
                continue;
            }
            if (C) IC = address;
        }
        else if (strcmp(opcode, "GD") == 0) {
            SI = 1;
            MOS();
        }
        else if (strcmp(opcode, "PD") == 0) {
            SI = 2;
            MOS();
        }
        else if (strcmp(opcode, "H ") == 0 || opcode[0] == 'H') {
            SI = 3;
            MOS();
        }
        else {
            fprintf(outputFile, "INVALID OPERATION: %c%c\n\n", IR[0], IR[1]);
            aborted = 1;
            terminated = 1;
        }
    }
}

void STARTEXECUTION(void) {
    EXECUTEUSERPROGRAM();
}

void LOAD(void) {
    char buffer[256];
    int m = 0;

    initMemory();

    while (fgets(buffer, sizeof(buffer), inputFile)) {
        trim_newline(buffer);
        trim_leading_spaces(buffer);

        if (buffer[0] == '\0') continue;

        if (is_control(buffer, "$AMJ")) {
            // New job. Phase I uses one job at a time.
            initMemory();
            m = 0;

            // Read program cards until $DTA.
            while (fgets(buffer, sizeof(buffer), inputFile)) {
                trim_newline(buffer);
                trim_leading_spaces(buffer);

                if (is_control(buffer, "$DTA")) {
                    STARTEXECUTION();
                    break;
                }
                if (is_control(buffer, "$END")) {
                    break;
                }

                store_card(buffer, &m);
                if (aborted) break;
            }

            // The execution normally stops on H, after which the outer loop
            // continues reading until it encounters the job's $END.
            // If LOAD's inner loop stopped at $END, the next iteration starts
            // with the following job.
            continue;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *inputName = (argc > 1) ? argv[1] : "input.txt";
    const char *outputName = (argc > 2) ? argv[2] : "output.txt";

    inputFile = fopen(inputName, "r");
    if (!inputFile) {
        perror("Cannot open input file");
        return 1;
    }

    outputFile = fopen(outputName, "w");
    if (!outputFile) {
        perror("Cannot open output file");
        fclose(inputFile);
        return 1;
    }

    LOAD();

    fclose(inputFile);
    fclose(outputFile);

    printf("Phase 1 MOS execution completed.\n");
    printf("Output written to %s\n", outputName);
    return 0;
}
