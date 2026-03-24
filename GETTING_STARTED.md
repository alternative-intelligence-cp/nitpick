# Getting Started with Aria — Tester's Guide

Welcome! This guide will walk you through everything you need to set up the Aria compiler on your Linux Mint machine, write and compile Aria programs, and report any bugs you find. Every step is spelled out — just follow along in order.

---

## Table of Contents

1. [What is Aria?](#1-what-is-aria)
2. [Install Prerequisites](#2-install-prerequisites)
3. [Clone the Aria Repository](#3-clone-the-aria-repository)
4. [Build the Compiler](#4-build-the-compiler)
5. [Your First Aria Program](#5-your-first-aria-program)
6. [Install VS Code Syntax Highlighting](#6-install-vs-code-syntax-highlighting)
7. [Key Reference Files](#7-key-reference-files)
8. [Compiling Programs — Quick Reference](#8-compiling-programs--quick-reference)
9. [Exploring the Examples](#9-exploring-the-examples)
10. [Aria Language Crash Course](#10-aria-language-crash-course)
11. [Prompts for Your AI Assistant](#11-prompts-for-your-ai-assistant)
12. [How to Report Bugs](#12-how-to-report-bugs)
13. [Troubleshooting](#13-troubleshooting)

---

## 1. What is Aria?

Aria is a compiled systems programming language. Think of it like C or Rust, but with its own unique syntax. You write `.aria` files, compile them with the `ariac` compiler, and get a native executable you can run directly. It compiles down to machine code via LLVM — so the programs are fast.

Aria is brand new (version 0.2.2), so **you will probably find bugs**. That's exactly what we need your help with! When something doesn't work the way you'd expect, that's valuable information.

---

## 2. Install Prerequisites

Open a terminal (Ctrl+Alt+T) and run these commands **one at a time**. Enter your password when prompted.

### Step 1: Update your package lists

```bash
sudo apt update
```

### Step 2: Install build tools

```bash
sudo apt install -y build-essential cmake git
```

This installs the C++ compiler (`g++`), the build system (`cmake`), and `git` for downloading the code.

### Step 3: Install LLVM 20

Aria requires LLVM version 20. On Linux Mint 22.3 (based on Ubuntu 24.04), this is available in the default repositories:

```bash
sudo apt install -y llvm-20-dev
```

**If that doesn't work** (e.g., you get "unable to locate package"), you may need to enable the `universe` repository first:

```bash
sudo add-apt-repository universe
sudo apt update
sudo apt install -y llvm-20-dev
```

### Step 4: Install liburing

```bash
sudo apt install -y liburing-dev
```

### Step 5: Verify everything installed

```bash
llvm-config-20 --version
cmake --version
g++ --version
```

You should see:
- LLVM version **20.x.x** (e.g., 20.1.2)
- CMake version **3.28.x** or higher
- g++ version **13.x.x** or similar

If any of those commands fail or show a version that's too old, stop here and let us know.

---

## 3. Clone the Aria Repository

### Option A: HTTPS (recommended if you haven't set up SSH keys)

```bash
cd ~
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria
```

### Option B: SSH (if you have GitHub SSH keys set up)

```bash
cd ~
git clone git@github.com:alternative-intelligence-cp/aria.git
cd aria
```

After this, you should be inside the `~/aria` directory. You can verify:

```bash
ls
```

You should see files like `CMakeLists.txt`, `scripts/`, `examples/`, `tests/`, etc.

---

## 4. Build the Compiler

### Quick Method (recommended)

The install script handles everything — prerequisite checking, building, and optional system-wide installation:

```bash
./install.sh --build-only
```

This will check prerequisites, then build `ariac`, `aria-ls`, `aria-pkg`, `aria-doc`, and `aria-safety`. To also install them to `/usr/local/bin`:

```bash
sudo ./install.sh
```

### Alternative: Build script

```bash
./scripts/build.sh
```

This will:
1. Create a `build/` directory
2. Run CMake to configure the build
3. Compile everything using all your CPU cores

**This may take several minutes** depending on your machine. You'll see a lot of output scrolling by — that's normal.

When it finishes, verify the compiler was built:

```bash
./build/ariac --help
```

You should see the compiler's usage message listing all the flags and options. If you see that, **congratulations — you have a working Aria compiler!**

### If the build fails

Common issues:
- **"LLVM not found"**: Make sure `llvm-20-dev` is installed (`sudo apt install llvm-20-dev`)
- **"uring.h not found"**: Make sure `liburing-dev` is installed (`sudo apt install liburing-dev`)
- **Other errors**: Copy the full error output and send it to us (see [How to Report Bugs](#12-how-to-report-bugs))

---

## 5. Your First Aria Program

### Create a file

Open VS Code, create a new file called `hello.aria`, and paste this:

```aria
func:main = int32(){
    print("Hello, World!");
    pass(0);
};
func:failsafe = NIL(int32:err_code) {};
```

Save it somewhere you can find it (your home directory is fine).

### Compile it

In the terminal, navigate to where you saved the file and run:

```bash
~/aria/build/ariac hello.aria -o hello
```

This compiles `hello.aria` into an executable called `hello`.

### Run it

```bash
./hello
```

You should see:

```
Hello, World!
```

### What just happened?

- `func:main = int32()` — declares the main function (entry point), returns an int32
- `print("Hello, World!")` — prints text to the screen
- `pass(0)` — returns 0 (success) from the function
- The `};` at the end closes the function (yes, functions end with `};` in Aria)

---

## 6. Install VS Code Syntax Highlighting

The Aria repository includes a VS Code extension for syntax highlighting.

### Step 1: Copy the extension

```bash
cp -r ~/aria/vscode-aria ~/.vscode/extensions/aria-language-0.0.1
```

### Step 2: Restart VS Code

Close VS Code completely and reopen it. (Or press Ctrl+Shift+P, type "Reload Window", and hit Enter.)

### Step 3: Verify

Open any `.aria` file. You should see syntax highlighting — keywords in one color, strings in another, comments greyed out, etc.

If it doesn't work, you can also try:
```bash
cd ~/aria/vscode-aria
code --install-extension .
```

---

## 7. Key Reference Files

These are the important files to know about inside the `~/aria/` directory:

| File/Directory | What it is |
|---|---|
| `examples/basic/` | **Start here!** 5 beginner examples (hello world, variables, functions, control flow, memory) |
| `examples/` | Larger example programs (games, parsers, HTTP server, crypto, etc.) |
| `aria_ecosystem/programming_guide/` | Comprehensive language documentation — 350+ topic files |
| `.internal/aria_specs.txt` | The complete language specification (7,000+ lines — reference, not tutorial) |
| `KNOWN_ISSUES.md` | Known bugs and limitations — check here before reporting |
| `ARCHITECTURE.md` | How the compiler works internally |
| `SAFETY.md` | Aria's three-layer safety system |
| `CHANGELOG.md` | Version history and changes |
| `tests/` | 400+ test files — great for seeing how features are used |
| `stdlib/` | Standard library source code |

### Programming Guide Structure

The programming guide at `aria_ecosystem/programming_guide/` is organized by topic:

| Directory | Topics |
|---|---|
| `types/` | All Aria types (int8-int64, uint8-uint64, flt32/flt64, bool, string, int128-int4096, and more) |
| `functions/` | Function syntax, generics, closures, async |
| `control_flow/` | if/else, while, till, pick (switch), loops |
| `operators/` | All operators (+, -, *, /, comparisons, bitwise, etc.) |
| `memory_model/` | Borrow system, wild blocks (unsafe), memory management |
| `modules/` | Module system, imports, namespaces |
| `io_system/` | File I/O, network I/O |
| `debugging/` | Debug tools and techniques |
| `standard_library/` | Standard library documentation |
| `advanced_features/` | Advanced topics |

---

## 8. Compiling Programs — Quick Reference

```bash
# Basic compile and run:
~/aria/build/ariac myfile.aria -o myprogram
./myprogram

# Compile with optimizations:
~/aria/build/ariac myfile.aria -o myprogram -O2

# See the generated LLVM IR (useful for bug reports):
~/aria/build/ariac myfile.aria --emit-llvm

# See the generated assembly:
~/aria/build/ariac myfile.aria --emit-asm

# Compile with verbose output (shows what the compiler is doing):
~/aria/build/ariac myfile.aria -o myprogram -v

# Compile a multi-file project (modules):
~/aria/build/ariac main.aria utils.aria -o myprogram

# Link against a C library (e.g., math):
~/aria/build/ariac myfile.aria -o myprogram -lm
```

**Tip**: You can create an alias so you don't have to type the full path every time. Add this to the end of your `~/.bashrc` file:

```bash
alias ariac="$HOME/aria/build/ariac"
```

Then open a new terminal (or run `source ~/.bashrc`), and you can just type:

```bash
ariac myfile.aria -o myprogram
```

---

## 9. Exploring the Examples

The `examples/basic/` directory has 5 beginner-friendly examples. Try compiling and running each one:

```bash
cd ~/aria/examples/basic

# Hello World
~/aria/build/ariac 01_hello_world.aria -o hello && ./hello

# Variables and Types
~/aria/build/ariac 02_variables.aria -o variables && ./variables

# Functions
~/aria/build/ariac 03_functions.aria -o functions && ./functions

# Control Flow (if/else, loops)
~/aria/build/ariac 04_control_flow.aria -o control_flow && ./control_flow

# Memory
~/aria/build/ariac 05_memory.aria -o memory && ./memory
```

After those, check out the more advanced examples in `examples/`:

```bash
ls ~/aria/examples/
```

Look for things like `rpn_calc.aria` (calculator), `tictactoe.aria` (game), `number_demo.aria`, etc.

---

## 10. Aria Language Crash Course

Here's a quick overview of how Aria differs from languages you might have seen:

### Variables — type comes first

```aria
int32:x = 42;
flt64:pi = 3.14159;
bool:done = false;
string:name = "Aria";
```

### Functions — `func:name = returnType(params){ body };`

```aria
func:add = int32(int32:a, int32:b){
    pass(a + b);
};
```

- `pass(value)` is how you return a value (like `return` in other languages)
- Functions end with `};` (semicolon after the closing brace)

### If/Else

```aria
if(x > 10){
    print("big");
}else{
    print("small");
}
```

### Loops

```aria
// While loop
while(x < 100){
    x = x + 1;
}

// Till loop — counts from 0 to limit-1, step 1
// $ is the loop variable
till(10, 1){
    print(`&{$}`);    // prints 0 through 9
}
```

### String interpolation — backticks with `&{}`

```aria
int32:age = 30;
print(`My age is &{age}`);
```

### Pick (switch)

```aria
pick(value){
    case(1){
        print("one");
    }
    case(2){
        print("two");
    }
    default{
        print("other");
    }
}
```

### Result type — Aria's error handling

Functions return `result` types with `.val` (the value) and `.err` (error code):

```aria
func:divide = flt64(flt64:a, flt64:b){
    if(b == 0.0){
        fail(1);   // return an error
    }
    pass(a / b);   // return success
};

func:main = int32(){
    result:r = divide(10.0, 3.0);
    if(r.err != 0){
        print("Error!");
    }else{
        // use r.val
    }
    pass(0);
};
```

---

## 11. Prompts for Your AI Assistant

Here are some prompts you can give Copilot (or any AI assistant) to help you write Aria code. **Copy the relevant prompt and paste it into your AI chat.**

### Starter prompt — teach your AI about Aria

> I'm writing code in a new programming language called Aria. Here are the key syntax rules:
>
> - Variables: `type:name = value;` (e.g., `int32:x = 42;`)
> - Functions: `func:name = returnType(type:param){ body };` — note the semicolon after `}`
> - Return values: use `pass(value)` instead of `return`
> - Return errors: use `fail(code)` instead of throwing
> - Print: `print("text")` or `print(\`&{variable}\`)`
> - String interpolation: backtick strings with `&{expr}`
> - Entry point: `func:main = int32(){ ... };`
> - Comments: `//` for line, `/* */` for block
> - If/else: `if(cond){ } else { }`
> - While: `while(cond){ }`
> - Till loop: `till(limit, step){ }` — `$` is the loop variable
> - Pick (switch): `pick(val){ case(1){ } default{ } }`
> - Types: int8, int16, int32, int64, uint8-uint64, flt32, flt64, bool, string
> - Large integers: int128, int256, int512, int1024, int2048, int4096
> - Result type: `result:r = func(); r.val` for value, `r.err` for error code
> - The failsafe function is required as an error handler: `func:failsafe = NIL(int32:err_code) {};`
>
> I have the language spec at `.internal/aria_specs.txt` and examples in `examples/basic/` in my project. Help me write Aria programs following these rules.

### When porting code to Aria

> I have this [Python/JavaScript/etc.] code that I want to rewrite in Aria. Remember Aria uses `type:name` for variables (like `int32:x`), `pass()` instead of `return`, functions end with `};`, and string interpolation uses backticks with `&{}`. Please convert this code to Aria syntax:
>
> [paste your code here]

### When something doesn't compile

> I'm getting a compiler error in Aria. The compiler is called `ariac`. Here's my code and the error message. Can you help me figure out what's wrong? Remember Aria syntax requires `type:name` for variables, `pass()` for return values, and functions are declared as `func:name = returnType(params){ body };`
>
> **Code:**
> [paste code]
>
> **Error:**
> [paste error]

### When exploring crypto/data analysis features

> I want to write a data analysis tool in Aria. Aria has large integer types (int128 up to int4096) which could be useful for crypto calculations. It also has flt64 for floating-point math. Help me write an Aria program that [describe what you want]. Remember the Aria syntax rules: `type:name` for variables, `pass()` to return, `func:name = returnType(params){ body };` for functions.

---

## 12. How to Report Bugs

When you find something that doesn't work, crashes, or produces unexpected output, we need a bug report. Here's the template — **copy this, fill it in, and send it to us**:

---

### Bug Report Template

```
=== ARIA BUG REPORT ===

Date: [today's date]
Reporter: [your name]

--- SUMMARY ---
[One sentence describing what went wrong]

--- WHAT I EXPECTED ---
[What you thought should happen]

--- WHAT ACTUALLY HAPPENED ---
[What actually happened — error message, wrong output, crash, etc.]

--- ARIA SOURCE CODE ---
[Paste the COMPLETE .aria file that triggers the bug. If it's multiple files,
include all of them.]

--- COMPILER COMMAND ---
[The exact command you ran, e.g.:]
~/aria/build/ariac myfile.aria -o myprogram

--- COMPILER OUTPUT ---
[Paste the COMPLETE output from the compiler. Include any error messages,
warnings, or crash output. If the program compiled but crashed when running,
include that output too.]

--- LLVM IR (if possible) ---
[Run: ~/aria/build/ariac myfile.aria --emit-llvm 2>&1 ]
[Paste the output here — this helps us debug. Skip if the compiler crashes
before producing IR.]

--- SYSTEM INFO ---
OS: Linux Mint 22.3
LLVM: [run: llvm-config-20 --version]
Compiler built: [run: git -C ~/aria log --oneline -1]

--- STEPS TO REPRODUCE ---
1. [Step-by-step what you did]
2. [So we can reproduce the exact problem]
3. [Include any setup needed]

--- SEVERITY ---
[ ] Crash — the compiler crashes or segfaults
[ ] Wrong output — program compiles but produces incorrect results
[ ] Compile error — valid code is rejected by the compiler
[ ] Unexpected behavior — something is weird but not necessarily wrong
[ ] Performance — program runs but is unexpectedly slow

--- ADDITIONAL NOTES ---
[Anything else that might be relevant — what you were trying to do,
workarounds you found, similar code that DOES work, etc.]

=== END REPORT ===
```

---

### Tips for good bug reports

1. **Make it minimal** — try to cut your code down to the smallest program that still triggers the bug. Remove anything unrelated.
2. **Include everything** — the full source code, the exact command, and the complete output. Don't paraphrase error messages.
3. **Check KNOWN_ISSUES.md first** — run `cat ~/aria/KNOWN_ISSUES.md` to see if the bug is already documented.
4. **Test with the latest code** — before reporting, pull the latest changes:
   ```bash
   cd ~/aria
   git pull
   ./scripts/build.sh
   ```
   Then try again. The bug might already be fixed.
5. **One bug per report** — if you found three different bugs, send three separate reports.

### Quick bug report shortcut

If you find a bug and want to capture everything quickly, run this in the terminal:

```bash
echo "=== ARIA BUG REPORT ===" > bugreport.txt
echo "Date: $(date)" >> bugreport.txt
echo "Git: $(git -C ~/aria log --oneline -1)" >> bugreport.txt
echo "LLVM: $(llvm-config-20 --version)" >> bugreport.txt
echo "OS: $(lsb_release -d -s)" >> bugreport.txt
echo "" >> bugreport.txt
echo "--- SOURCE CODE ---" >> bugreport.txt
cat YOUR_FILE.aria >> bugreport.txt
echo "" >> bugreport.txt
echo "--- COMPILER OUTPUT ---" >> bugreport.txt
~/aria/build/ariac YOUR_FILE.aria -o testprog 2>&1 >> bugreport.txt
echo "" >> bugreport.txt
echo "--- PROGRAM OUTPUT ---" >> bugreport.txt
./testprog 2>&1 >> bugreport.txt
echo "=== END REPORT ===" >> bugreport.txt
```

(Replace `YOUR_FILE.aria` with your actual filename.)

Then just email us the `bugreport.txt` file.

---

## 13. Troubleshooting

### "command not found: ariac"

You need to use the full path: `~/aria/build/ariac`. Or set up the alias described in [section 8](#8-compiling-programs--quick-reference).

### "llvm-config-20: command not found"

LLVM 20 isn't installed. Run:
```bash
sudo apt install -y llvm-20-dev
```

### Build fails with "cannot find -luring"

Install liburing:
```bash
sudo apt install -y liburing-dev
```

### "Permission denied" when running build.sh

Make it executable:
```bash
chmod +x ~/aria/scripts/build.sh
```

### VS Code doesn't show syntax highlighting

1. Make sure you copied the extension correctly:
   ```bash
   ls ~/.vscode/extensions/aria-language-0.0.1/package.json
   ```
   If that file doesn't exist, redo step 6.
2. Restart VS Code completely (close all windows, reopen).
3. Make sure your file has the `.aria` extension.

### Program compiled but nothing happened

Make sure your main function prints something or returns a non-zero exit code you can check:
```bash
./myprogram
echo $?    # shows the exit code (the value you passed to pass())
```

### "Segmentation fault" when running a compiled program

This is likely a compiler bug! Please report it using the [bug report template](#bug-report-template). Include the `--emit-llvm` output if possible.

### How do I update to the latest version?

```bash
cd ~/aria
git pull
./scripts/build.sh
```

---

## 14. Database Client Libraries

Aria includes client libraries for four popular databases. Each uses a C shim to bridge Aria's FFI to the database's native C library.

### Prerequisites

Install the development libraries for the databases you want to use:

```bash
# SQLite (embedded, no server needed)
sudo apt install libsqlite3-dev

# PostgreSQL
sudo apt install libpq-dev

# MySQL
sudo apt install libmysqlclient-dev

# Redis
sudo apt install libhiredis-dev
```

### Building a database package

Each database package lives in `aria-packages/packages/aria-<db>/`. To build:

```bash
cd aria-packages/packages/aria-sqlite

# 1. Build the C shim
cd shim
cc -O2 -shared -fPIC -Wall -o libaria_sqlite_shim.so aria_sqlite_shim.c -lsqlite3
cd ..

# 2. Compile your program that uses the library
ariac my_program.aria -L shim -laria_sqlite_shim -lsqlite3 -o my_program

# 3. Run (shim must be in library path)
LD_LIBRARY_PATH=shim ./my_program
```

### Quick example — SQLite

```
func:failsafe = NIL(int32:code) { pass(NIL); };

extern "aria_sqlite_shim" {
    func:aria_sqlite_open  = int32(string:path);
    func:aria_sqlite_last_db = int32();
    func:aria_sqlite_exec  = int32(int32:db, string:sql);
    func:aria_sqlite_disconnect = int32(int32:db);
}

func:main = int32() {
    int32:db = aria_sqlite_open(":memory:");
    int32:d = aria_sqlite_last_db();
    aria_sqlite_exec(d, "CREATE TABLE test(id INTEGER PRIMARY KEY, name TEXT)");
    int32:d2 = aria_sqlite_last_db();
    aria_sqlite_exec(d2, "INSERT INTO test VALUES(1, 'hello')");
    int32:d3 = aria_sqlite_last_db();
    aria_sqlite_disconnect(d3);
    pass(0i32);
};
```

### Available packages

| Package | API Prefix | System Library | Server Required |
|---------|-----------|----------------|-----------------|
| aria-sqlite | `sqlite_*` | libsqlite3 | No (embedded) |
| aria-postgres | `pg_*` | libpq | Yes |
| aria-mysql | `mysql_db_*` | libmysqlclient | Yes |
| aria-redis | `redis_*` | libhiredis | Yes |

All SQL database packages support parameterized queries to prevent SQL injection.

## Quick Start Checklist

- [ ] Installed build-essential, cmake, git
- [ ] Installed llvm-20-dev
- [ ] Installed liburing-dev
- [ ] Cloned the repository
- [ ] Built the compiler with `./scripts/build.sh`
- [ ] Compiled and ran `hello.aria`
- [ ] Installed VS Code syntax highlighting
- [ ] Read through `examples/basic/`
- [ ] Set up the `ariac` alias (optional but recommended)
- [ ] Gave your AI assistant the Aria syntax primer prompt

---

*Questions? Problems? Send them our way along with as much detail as you can. Happy testing!*
