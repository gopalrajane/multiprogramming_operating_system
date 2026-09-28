# Multiprogramming Operating System (MOS) – Phase 1

A C-based simulation of a **Multiprogramming Operating System (MOS)** implementing basic CPU, memory, instruction execution, and supervisor call operations.

## 📌 Project Overview

This project simulates the first phase of a multiprogramming operating system. It contains a simulated CPU, memory, registers, loader, and MOS services.

The system reads jobs from an input file, loads them into memory, executes the instructions, performs input/output operations, and stores the output in a file.

## ⚙️ Features

* Simulated memory of **100 words**
* Each memory word contains **4 characters**
* CPU registers:

  * **IR** – Instruction Register
  * **IC** – Instruction Counter
  * **R** – General Purpose Register
  * **C** – Comparison Toggle
* Instruction execution:

  * `LR` – Load Register
  * `SR` – Store Register
  * `CR` – Compare Register
  * `BT` – Branch on Toggle
  * `GD` – Get Data
  * `PD` – Put Data
  * `H` – Halt
* Supervisor calls:

  * `SI = 1` → READ
  * `SI = 2` → WRITE
  * `SI = 3` → TERMINATE
* Job control cards:

  * `$AMJ` – Start Job
  * `$DTA` – Start Data Section
  * `$END` – End Job

## 📂 Project Structure

```text
MOS-Phase1/
│
├── main.c
├── input_Phase1.txt
├── output.txt
├── run.bat
└── README.md
```

## 🖥️ Requirements

You need:

* Windows 10/11
* VS Code (recommended)
* GCC compiler / MinGW
* Git (optional, for GitHub)

## 🚀 How to Run

### Step 1 – Open the Project

Download or clone this repository and open the project folder in VS Code.

### Step 2 – Open Terminal

In VS Code:

```text
Terminal → New Terminal
```

### Step 3 – Compile the Program

Run:

```bash
gcc -std=c11 -Wall -Wextra -O2 main.c -o mos.exe
```

If there are no errors, `mos.exe` will be created.

### Step 4 – Run the Program

Run:

```bash
.\mos.exe input_Phase1.txt output.txt
```

The program will read the jobs from `input_Phase1.txt` and write the results to `output.txt`.

### Step 5 – View Output

To display the output in the terminal:

```bash
type output.txt
```

Or open `output.txt` directly in VS Code.

## ▶️ Quick Run Using `run.bat`

If `run.bat` is included, simply double-click it.

Or run:

```bash
.\run.bat
```

It will compile and execute the program.

## 📝 Input Format

The input file contains jobs using control cards:

```text
$AMJ...
Program Instructions
$DTA
Input Data
$END...
```

Example:

```text
$AMJ020100120003
GD20LR20GD30PD30H
$DTA
HELLO WORLD
$END0201
```

## 🔄 Program Flow

```text
Input File
     ↓
   Loader
     ↓
   Memory
     ↓
 CPU Fetch
     ↓
 Decode & Execute
     ↓
 GD / PD / H
     ↓
Supervisor Interrupt
     ↓
 MOS Services
     ↓
Output File
     ↓
 Next Job
```

## 🧠 Instruction Summary

| Instruction | Operation                           |
| ----------- | ----------------------------------- |
| `LR`        | Load memory content into register R |
| `SR`        | Store register R into memory        |
| `CR`        | Compare R with memory               |
| `BT`        | Branch if comparison is true        |
| `GD`        | Read data from input                |
| `PD`        | Write data to output                |
| `H`         | Halt/terminate the job              |

## 📚 Concepts Demonstrated

* Operating System fundamentals
* CPU simulation
* Memory management
* Instruction execution
* Registers
* Supervisor calls
* Interrupt handling
* Job loading
* Input/Output operations
* Multiprogramming OS concepts

## 🎓 Project Purpose

This project is developed for educational purposes to understand the internal working of a basic **Multiprogramming Operating System** through simulation.

It demonstrates how a CPU executes instructions and how supervisor calls allow user programs to communicate with operating system services.

## 👨‍💻 Technologies Used

* **Language:** C
* **Compiler:** GCC
* **Environment:** Windows / VS Code
* **Version Control:** Git & GitHub

## 📌 Author

**Pranil Mahale**

---

⭐ If you find this project useful, consider giving the repository a star!
