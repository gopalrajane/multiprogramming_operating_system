/*
 * MOS Phase 2 (Stage II) simulation
 *
 * What is new compared to Stage I:
 *   - Paging: every job gets a page table, addresses in the program are virtual
 *     and are translated to real addresses. Frames are allocated randomly.
 *   - Process data structure: a PCB holds job id, time limit, line limit and
 *     the two counters (time and lines).
 *   - Interrupts: SI (supervisor call), TI (timer) and PI (program error) are
 *     set while the program runs and serviced by MOS().
 *   - Error handling: seven error messages and a short report for every job.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MEMORY_SIZE  300    /* real memory: 300 words of 4 characters */
#define WORD_SIZE    4
#define FRAME_WORDS  10     /* one frame (and one page) is 10 words = one card */
#define FRAME_COUNT  30     /* 300 / 10 */
#define PAGE_COUNT   10     /* virtual memory of one job: 10 pages = 100 words */
#define CARD_LENGTH  40
#define NO_LIMIT     -1
#define RANDOM_SEED  2026   /* fixed, so a run can be repeated exactly */

/* error codes (EM) */
enum {
    E_NONE, E_OUT_OF_DATA, E_LINE_LIMIT, E_TIME_LIMIT,
    E_OPCODE, E_OPERAND, E_PAGE_FAULT, E_MEMORY
};

const char *error_text[] = {
    "NO ERROR",
    "OUT OF DATA",
    "LINE LIMIT EXCEEDED",
    "TIME LIMIT EXCEEDED",
    "OPCODE ERROR",
    "OPERAND ERROR",
    "INVALID PAGE FAULT",
    "MEMORY EXCEEDED"
};

enum { OP_BAD, OP_LR, OP_SR, OP_CR, OP_BT, OP_GD, OP_PD, OP_H };

/* process control block */
typedef struct {
    char job_id[5];
    int ttl;    /* time limit, NO_LIMIT if the card does not have one */
    int tll;    /* line limit */
    int ttc;    /* total time counter */
    int llc;    /* line limit counter (lines written so far) */
} PCB;

char M[MEMORY_SIZE][WORD_SIZE];
char IR[WORD_SIZE];
char R[WORD_SIZE];
int IC, C;
int PTR;                    /* real address (word index) of the page table */
int SI, TI, PI;             /* interrupt flags */
int operand;                /* address part of the current instruction, -1 if bad */
int fault_va;               /* virtual address that caused the last page fault */
int fault_is_valid;         /* 1 when the faulting instruction is GD or SR */
int frame_used[FRAME_COUNT];
int terminated;
int wrote_line;             /* has this job written any output line yet */
int verbose;
PCB pcb;

FILE *inputFile;
FILE *outputFile;

void MOS(void);
void READ(void);
void WRITE(void);
void TERMINATE(int em1, int em2);

/* ---------- small helpers ---------- */

void initMemory(void) {
    memset(M, ' ', sizeof(M));
    memset(IR, ' ', WORD_SIZE);
    memset(R, ' ', WORD_SIZE);
    memset(frame_used, 0, sizeof(frame_used));
    IC = 0;
    C = 0;
    PTR = 0;
    SI = 0;
    TI = 0;
    PI = 0;
    operand = -1;
    fault_va = 0;
    fault_is_valid = 0;
    terminated = 0;
    wrote_line = 0;
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

/* Reads a 4-digit number from the control card at the given offset.
   Returns -1 if the card is too short or the field is not all digits. */
int read_limit(const char *card, int offset) {
    if ((int)strlen(card) < offset + 4) return -1;
    int value = 0;
    for (int i = 0; i < 4; i++) {
        if (!isdigit((unsigned char)card[offset + i])) return -1;
        value = value * 10 + (card[offset + i] - '0');
    }
    return value;
}

/* Fills the PCB from the $AMJ card: $AMJ + job id + time limit + line limit */
void init_pcb(const char *card) {
    memset(&pcb, 0, sizeof(pcb));
    if ((int)strlen(card) >= 8) {
        memcpy(pcb.job_id, card + 4, 4);
        pcb.job_id[4] = '\0';
    } else {
        strcpy(pcb.job_id, "----");
    }
    pcb.ttl = read_limit(card, 8);
    pcb.tll = read_limit(card, 12);
}

/* ---------- paging ---------- */

/* Frame number of a page, or -1 if the page is not in memory.
   A page table entry is "****" when empty, otherwise the frame number as 4 digits. */
int frame_of_page(int page) {
    const char *w = M[PTR + page];
    if (w[0] == '*') return -1;
    return (w[0] - '0') * 1000 + (w[1] - '0') * 100 + (w[2] - '0') * 10 + (w[3] - '0');
}

void set_page_entry(int page, int frame) {
    char buf[8];
    sprintf(buf, "%04d", frame);
    memcpy(M[PTR + page], buf, WORD_SIZE);
}

/* Picks a random free frame and clears it. */
int ALLOCATE(void) {
    int frame;
    do {
        frame = rand() % FRAME_COUNT;
    } while (frame_used[frame]);
    frame_used[frame] = 1;
    memset(M[frame * FRAME_WORDS], ' ', FRAME_WORDS * WORD_SIZE);
    return frame;
}

/* Virtual to real address. Returns -1 and sets PI = 3 if the page is not in memory. */
int ADDRESSMAP(int va) {
    if (va < 0 || va >= PAGE_COUNT * FRAME_WORDS) {
        fault_va = va;
        PI = 3;
        return -1;
    }
    int frame = frame_of_page(va / FRAME_WORDS);
    if (frame < 0) {
        fault_va = va;
        PI = 3;
        return -1;
    }
    return frame * FRAME_WORDS + va % FRAME_WORDS;
}

/* Valid page fault: give the missing page a frame and carry on. */
void service_page_fault(void) {
    int page = fault_va / FRAME_WORDS;
    set_page_entry(page, ALLOCATE());
    PI = 0;
    fault_is_valid = 0;
}

/* Address map for an instruction that writes to memory (SR, GD).
   A missing page is a valid page fault here, so MOS gets it allocated. */
int map_for_store(int va) {
    int ra = ADDRESSMAP(va);
    if (PI == 3) {
        fault_is_valid = 1;
        MOS();
        ra = ADDRESSMAP(va);
    }
    return ra;
}

/* Makes sure every page touched by a 10 word block starting at va exists. */
void map_block_for_store(int va) {
    for (int p = va / FRAME_WORDS; p <= (va + FRAME_WORDS - 1) / FRAME_WORDS; p++)
        map_for_store(p * FRAME_WORDS);
}

/* ---------- instruction decoding ---------- */

int get_opcode(void) {
    if (IR[0] == 'H' && IR[1] == ' ') return OP_H;
    if (strncmp(IR, "LR", 2) == 0) return OP_LR;
    if (strncmp(IR, "SR", 2) == 0) return OP_SR;
    if (strncmp(IR, "CR", 2) == 0) return OP_CR;
    if (strncmp(IR, "BT", 2) == 0) return OP_BT;
    if (strncmp(IR, "GD", 2) == 0) return OP_GD;
    if (strncmp(IR, "PD", 2) == 0) return OP_PD;
    return OP_BAD;
}

/* Address part of IR, or -1 if the two characters are not digits. */
int get_operand(void) {
    if (!isdigit((unsigned char)IR[2]) || !isdigit((unsigned char)IR[3]))
        return -1;
    return (IR[2] - '0') * 10 + (IR[3] - '0');
}

/* GD and SR take 2 time units, everything else takes 1. */
int time_cost(int opcode) {
    return (opcode == OP_GD || opcode == OP_SR) ? 2 : 1;
}

/* ---------- supervisor services ---------- */

void READ(void) {
    char data[256];

    if (!fgets(data, sizeof(data), inputFile)) {
        TERMINATE(E_OUT_OF_DATA, E_NONE);
        return;
    }
    trim_newline(data);
    if (is_control(data, "$END")) {
        TERMINATE(E_OUT_OF_DATA, E_NONE);
        return;
    }

    int len = (int)strlen(data);
    if (len > CARD_LENGTH) len = CARD_LENGTH;

    for (int word = 0; word < FRAME_WORDS; word++) {
        int ra = ADDRESSMAP(operand + word);
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = word * WORD_SIZE + j;
            M[ra][j] = (pos < len) ? data[pos] : ' ';
        }
    }
}

void WRITE(void) {
    if (pcb.tll != NO_LIMIT && pcb.llc >= pcb.tll) {
        TERMINATE(E_LINE_LIMIT, E_NONE);
        return;
    }

    for (int word = 0; word < FRAME_WORDS; word++) {
        int ra = ADDRESSMAP(operand + word);
        for (int j = 0; j < WORD_SIZE; j++)
            fputc(M[ra][j], outputFile);
    }
    fputc('\n', outputFile);
    pcb.llc++;
    wrote_line = 1;
}

void dump_page_table(void) {
    printf("  job %s: page table in frame %d\n", pcb.job_id, PTR / FRAME_WORDS);
    for (int p = 0; p < PAGE_COUNT; p++) {
        int frame = frame_of_page(p);
        if (frame >= 0) printf("    page %d -> frame %d\n", p, frame);
    }
}

/* Ends the job and writes the report. em2 is only used when a time limit
   error comes together with a program error. */
void TERMINATE(int em1, int em2) {
    char ir[WORD_SIZE + 1];
    memcpy(ir, IR, WORD_SIZE);
    ir[WORD_SIZE] = '\0';
    for (int i = WORD_SIZE - 1; i >= 0 && ir[i] == ' '; i--) ir[i] = '\0';

    if (wrote_line) fputc('\n', outputFile);
    fprintf(outputFile, "JOB ID : %s\n", pcb.job_id);
    fprintf(outputFile, "STATUS : %s\n", error_text[em1]);
    if (em2 != E_NONE)
        fprintf(outputFile, "         %s\n", error_text[em2]);
    fprintf(outputFile, "IC     : %02d\n", IC);
    fprintf(outputFile, "IR     : %s\n", ir);
    fprintf(outputFile, "TTC    : %02d\n", pcb.ttc);
    fprintf(outputFile, "LLC    : %02d\n", pcb.llc);
    fputc('\n', outputFile);

    if (verbose) dump_page_table();
    terminated = 1;
}

/* Interrupt servicing. Called when SI, TI or PI is set. */
void MOS(void) {
    if (TI == 0) {
        if (PI == 1) {
            TERMINATE(E_OPCODE, E_NONE);
        } else if (PI == 2) {
            TERMINATE(E_OPERAND, E_NONE);
        } else if (PI == 3) {
            if (fault_is_valid) service_page_fault();
            else TERMINATE(E_PAGE_FAULT, E_NONE);
        } else if (SI == 1) {
            READ();
        } else if (SI == 2) {
            WRITE();
        } else if (SI == 3) {
            TERMINATE(E_NONE, E_NONE);
        }
    } else {    /* TI == 2: the time limit is over */
        if (PI == 1) {
            TERMINATE(E_TIME_LIMIT, E_OPCODE);
        } else if (PI == 2) {
            TERMINATE(E_TIME_LIMIT, E_OPERAND);
        } else if (PI == 3) {
            TERMINATE(E_TIME_LIMIT, E_NONE);
        } else if (SI == 2) {
            WRITE();                       /* the line is still written */
            if (!terminated) TERMINATE(E_TIME_LIMIT, E_NONE);
        } else if (SI == 3) {
            TERMINATE(E_NONE, E_NONE);     /* job was ending anyway */
        } else {
            TERMINATE(E_TIME_LIMIT, E_NONE);
        }
    }
}

/* ---------- running a job ---------- */

void EXECUTEUSERPROGRAM(void) {
    IC = 0;
    pcb.ttc = 0;
    pcb.llc = 0;

    while (!terminated) {
        SI = 0;
        TI = 0;
        PI = 0;
        fault_is_valid = 0;
        memset(IR, ' ', WORD_SIZE);

        /* fetch: the instruction address is virtual as well */
        int ra = ADDRESSMAP(IC);
        if (ra < 0) {
            MOS();      /* fetch can never be a valid page fault */
            continue;
        }
        memcpy(IR, M[ra], WORD_SIZE);
        IC++;

        int opcode = get_opcode();
        operand = -1;
        if (opcode != OP_BAD && opcode != OP_H) operand = get_operand();

        if (opcode == OP_BAD) {
            PI = 1;
        } else if (opcode != OP_H && operand < 0) {
            PI = 2;
        } else {
            switch (opcode) {
            case OP_LR:
                ra = ADDRESSMAP(operand);
                if (PI == 0) memcpy(R, M[ra], WORD_SIZE);
                break;
            case OP_CR:
                ra = ADDRESSMAP(operand);
                if (PI == 0) C = (memcmp(R, M[ra], WORD_SIZE) == 0);
                break;
            case OP_SR:
                ra = map_for_store(operand);
                if (PI == 0) memcpy(M[ra], R, WORD_SIZE);
                break;
            case OP_BT:
                if (C) IC = operand;
                break;
            case OP_GD:
                if (operand + FRAME_WORDS > PAGE_COUNT * FRAME_WORDS) {
                    PI = 2;
                } else {
                    SI = 1;
                    map_block_for_store(operand);
                }
                break;
            case OP_PD:
                if (operand + FRAME_WORDS > PAGE_COUNT * FRAME_WORDS) {
                    PI = 2;
                } else {
                    SI = 2;
                    for (int p = operand / FRAME_WORDS; p <= (operand + FRAME_WORDS - 1) / FRAME_WORDS; p++) {
                        ADDRESSMAP(p * FRAME_WORDS);
                        if (PI == 3) break;     /* invalid page fault, MOS will end the job */
                    }
                }
                break;
            case OP_H:
                SI = 3;
                break;
            }
        }

        pcb.ttc += time_cost(opcode);
        if (pcb.ttl != NO_LIMIT && pcb.ttc > pcb.ttl) TI = 2;

        if (SI || TI || PI) MOS();
    }
}

void STARTEXECUTION(void) {
    EXECUTEUSERPROGRAM();
}

/* Puts one program card into a newly allocated frame and records it in the page table. */
void store_card(const char *card, int page) {
    int frame = ALLOCATE();
    set_page_entry(page, frame);

    int len = (int)strlen(card);
    if (len > CARD_LENGTH) len = CARD_LENGTH;

    for (int word = 0; word < FRAME_WORDS; word++) {
        for (int j = 0; j < WORD_SIZE; j++) {
            int pos = word * WORD_SIZE + j;
            M[frame * FRAME_WORDS + word][j] = (pos < len) ? card[pos] : ' ';
        }
    }
}

void LOAD(void) {
    char buffer[256];

    while (fgets(buffer, sizeof(buffer), inputFile)) {
        trim_newline(buffer);
        trim_leading_spaces(buffer);

        if (!is_control(buffer, "$AMJ")) continue;   /* stray lines between jobs */

        initMemory();
        init_pcb(buffer);

        /* the page table lives in a frame of its own */
        PTR = ALLOCATE() * FRAME_WORDS;
        for (int p = 0; p < PAGE_COUNT; p++)
            memcpy(M[PTR + p], "****", WORD_SIZE);

        int pages = 0;
        int too_big = 0;

        while (fgets(buffer, sizeof(buffer), inputFile)) {
            trim_newline(buffer);
            trim_leading_spaces(buffer);

            if (buffer[0] == '\0') continue;
            if (is_control(buffer, "$DTA")) {
                if (too_big) TERMINATE(E_MEMORY, E_NONE);
                else STARTEXECUTION();
                break;
            }
            if (is_control(buffer, "$END")) break;

            if (pages >= PAGE_COUNT) {
                too_big = 1;        /* keep reading until $DTA so the next job starts cleanly */
                continue;
            }
            store_card(buffer, pages++);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *in_name = "input.txt";
    const char *out_name = "output.txt";
    int names = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) verbose = 1;
        else if (names == 0) { in_name = argv[i]; names++; }
        else if (names == 1) { out_name = argv[i]; names++; }
    }

    inputFile = fopen(in_name, "r");
    if (!inputFile) {
        fprintf(stderr, "Error: cannot open input file '%s'\n", in_name);
        return 1;
    }
    outputFile = fopen(out_name, "w");
    if (!outputFile) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", out_name);
        fclose(inputFile);
        return 1;
    }

    srand(RANDOM_SEED);
    LOAD();

    fclose(inputFile);
    fclose(outputFile);
    printf("Simulation complete. Output written to %s\n", out_name);
    return 0;
}
