#include <stdio.h>
#include <string.h>

#define MEMORY_SIZE 100
#define WORD_SIZE 4

char M[MEMORY_SIZE][WORD_SIZE];
char IR[WORD_SIZE];
char R[WORD_SIZE];

int IC = 0;          // Instruction Counter
int C = 0;           // Condition flag
int SI = 0;          // Supervisor Interrupt

FILE *inputFile;
FILE *outputFile;



void init() {
    int i, j;

    for (i = 0; i < MEMORY_SIZE; i++)
        for (j = 0; j < WORD_SIZE; j++)
            M[i][j] = ' ';

    for (i = 0; i < WORD_SIZE; i++) {
        IR[i] = ' ';
        R[i] = ' ';
    }

    IC = 0;
    C = 0;
    SI = 0;
}

\
int getAddress() {
    return (IR[2] - '0') * 10 + (IR[3] - '0');
}



void READ() {
    char data[100];
    int address;
    int i;

    fgets(data, sizeof(data), inputFile);

    data[strcspn(data, "\n")] = '\0';

    address = getAddress();

    for (i = 0; i < 40; i++) {
        if (data[i] == '\0')
            break;

        M[address + i / 4][i % 4] = data[i];
    }
}


void WRITE() {
    int address;
    int i, j;

    address = getAddress();

    for (i = 0; i < 10; i++) {
        for (j = 0; j < 4; j++)
            fputc(M[address + i][j], outputFile);
    }

    fputc('\n', outputFile);
}


void TERMINATE() {
    printf("Job terminated.\n");
}



void MOS() {

    if (SI == 1)
        READ();

    else if (SI == 2)
        WRITE();

    else if (SI == 3)
        TERMINATE();

    SI = 0;
}



void EXECUTE() {

    int running = 1;

    IC = 0;

    while (running) {


        for (int i = 0; i < 4; i++)
            IR[i] = M[IC][i];

        IC++;


        if (IR[0] == 'G' && IR[1] == 'D') {

            SI = 1;
            MOS();
        }

        else if (IR[0] == 'P' && IR[1] == 'D') {

            SI = 2;
            MOS();
        }

        else if (IR[0] == 'L' && IR[1] == 'R') {

            int address = getAddress();

            for (int i = 0; i < 4; i++)
                R[i] = M[address][i];
        }

        else if (IR[0] == 'S' && IR[1] == 'R') {

            int address = getAddress();

            for (int i = 0; i < 4; i++)
                M[address][i] = R[i];
        }

        else if (IR[0] == 'C' && IR[1] == 'R') {

            int address = getAddress();

            C = 1;

            for (int i = 0; i < 4; i++) {
                if (R[i] != M[address][i])
                    C = 0;
            }
        }

        else if (IR[0] == 'B' && IR[1] == 'T') {

            int address = getAddress();

            if (C == 1)
                IC = address;
        }

        else if (IR[0] == 'H') {

            SI = 3;
            MOS();
            running = 0;
        }
    }
}



void LOAD() {

    char line[100];
    int memory = 0;

    while (fgets(line, sizeof(line), inputFile)) {

        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "$AMJ", 4) == 0) {

            init();
            memory = 0;

            while (fgets(line, sizeof(line), inputFile)) {

                line[strcspn(line, "\n")] = '\0';


                if (strncmp(line, "$DTA", 4) == 0) {

                    EXECUTE();
                    break;
                }

                if (strncmp(line, "$END", 4) == 0)
                    break;

                for (int i = 0; i < strlen(line); i++) {

                    M[memory][i % 4] = line[i];

                    if (i % 4 == 3)
                        memory++;
                }
            }
        }
    }
}

int main() {

    inputFile = fopen("input.txt", "r");
    outputFile = fopen("output.txt", "w");

    if (inputFile == NULL || outputFile == NULL) {
        printf("File opening error!\n");
        return 1;
    }

    LOAD();

    fclose(inputFile);
    fclose(outputFile);

    printf("MOS execution completed.\n");

    return 0;
}