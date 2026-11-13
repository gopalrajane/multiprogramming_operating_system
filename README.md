# Multiprogramming Operating System

A C-based simulation of operating system concepts, developed in two phases. The project demonstrates instruction execution, memory management, job processing, interrupts, and input/output services.

## Phases

### Phase 1 — Basic MOS

Implements a simple simulated machine with:

- 100-word memory and 4-character words
- Instruction register, instruction counter, general-purpose register, and condition flag
- Loader and instruction execution
- `LR`, `SR`, `CR`, `BT`, `GD`, `PD`, and `H` instructions
- Supervisor services for reading, writing, and job termination

### Phase 2 — Paging and Job Control

Extends the simulation with:

- 300-word real memory divided into frames
- Virtual memory and page-table address translation
- Dynamic frame allocation and page-fault handling
- Process Control Block (PCB) for job information
- Supervisor, timer, and program interrupts
- Time and line limits, instruction validation, and job reports

## Project Structure

```text
multiprogramming_operating_system/
├── phase1/
│   ├── main.c
│   ├── input.txt
│   └── output.txt
├── phase2/
│   ├── main.c
│   ├── input_Phase2.txt
│   └── output_Phase2.txt
└── README.md
```

## Requirements

- C compiler (GCC recommended)
- Windows, Linux, or macOS terminal

## Build and Run

Run each phase from its own directory.

**Phase 1**

```bash
cd phase1
gcc -std=c11 -Wall -Wextra main.c -o mos
./mos
```

The program reads `input.txt` and writes results to `output.txt`.

**Phase 2**

```bash
cd phase2
gcc -std=c11 -Wall -Wextra main.c -o mos
./mos input_Phase2.txt output_Phase2.txt
```

Optional verbose page-table output:

```bash
./mos input_Phase2.txt output_Phase2.txt -v
```

On Windows, run the generated executable as `mos.exe`.

## Concepts

Operating system simulation, CPU instruction cycle, memory management, paging, address translation, process control, interrupts, and file-based input/output.

## Author

[Gopal Rajane](https://github.com/gopalrajane) · [LinkedIn](https://www.linkedin.com/in/gopal-rajane/)
