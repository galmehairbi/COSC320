

void stmt() {
   int *a, *b; // Temporary pointers for handling control flow
   // Handle 'if' statements
   if (tk == If) {
       next(); // Move to the next token after "if"
       if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); } // Expect '(' after 'if'
       expr(Assign); // Parse the condition expression inside the parentheses
       if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); } // Expect ')' after the condition
       *++e = BZ; b = ++e; // Emit a Branch if Zero instruction where 'b' points to the address to be filled later
       stmt();  // Parse and execute the 'if' statement
       if (tk == Else) { // Check for optional 'else' clause
           *b = (int)(e + 3); *++e = JMP; b = ++e; // Jump to end of 'else' block
           next(); // Move to the next token after 'else'
           stmt(); // Parse the statement block for the 'else' clause
       }
       *b = (int)(e + 1); // Fill the jump address for the 'if' or 'else' block
   }
   // Handle 'while' statements
   else if (tk == While) {
       next(); // Move to the next token
       a = e + 1;  // Save the starting address for the loop
       if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); } // Expect '(' after 'while'
       expr(Assign); // Parse the condition expression inside the parentheses
       if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); } // Expect ')' after the condition
       *++e = BZ; b = ++e;  // Emit a Branch if Zero instruction
       stmt();  // Parse the statement block for the 'while' loop
       *++e = JMP; *++e = (int)a; // Emit a Jump instruction to loop back to the start
       *b = (int)(e + 1);  // Fill the jump address for exiting the loop
   }
   // Handle 'return' statements
   else if (tk == Return) {
       next(); // Move to the next token after return
       if (tk != ';') expr(Assign); // Parse the return value if present
       *++e = LEV; // Emit a Leave instruction to return from the function
       if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); } // Expect ';' after 'return'
   }
   // Handle block statements (enclosed in '{' and '}')
   else if (tk == '{') {
       next();  // Move to the next token after '{'
       while (tk != '}') stmt(); // Parse all statements inside the block until '}' is encountered
       next(); // Move to the next token after '}'
   }
   // Handle empty statements (just a semicolon)
   else if (tk == ';') {
       next();  // Move to the next token after ';'
   }
   // Handle expression statements (e.g., assignments, function calls)
   else {
       expr(Assign); // Parse the expression
       if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }  // Expect ';' after the expression
   }
}


int main(int argc, char **argv) {
   int fd, bt, ty, poolsz, *idmain; // File descriptor, base type, type, memory pool size,
   // and pointer to main function
   int *pc, *sp, *bp, a, cycle; // vm registers such as program counter, stack pointer, base pointer, accumulator, and cycle counter
   int i, *t; // temporary variables for loops and pointers
   // Parse command-line arguments
   --argc; ++argv; // Skip the program name
   if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; } // Enable source printing if -s flag is present
   if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; } // Enable debug mode if -d flag is present
   if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; } // Print usage if no input file is provided
   // Open the input file
   if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; } // Open the file or exit on failure
   // Allocate memory pools for the compiler
   poolsz = 256*1024; // arbitrary size for memory pools
   if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; } // Allocate memory for the symbol table
   if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; } // Allocate memory for the code/text area
   if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }  // Allocate memory for the data area
   if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; } // Allocate memory for the stack
   // Initialize memory pools to zero
   memset(sym,  0, poolsz); // Clear the symbol table
   memset(e,    0, poolsz); // Clear the code/text area
   memset(data, 0, poolsz);  // Clear the data area
   // Add keywords and library functions to the symbol table
   p = "char else enum if int return sizeof while "
       "open read close printf malloc free memset memcmp exit void main"; // Keywords and library functions
   i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
   i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
   next(); id[Tk] = Char; // handle void type
   next(); idmain = id; // keep track of main function
   // Allocate memory for the source code and read the input file
   if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }  // Allocate memory for the source code
   if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }  // Read source code from the file into allocated memory.. handle read failure
   p[i] = 0; // Null-terminate the source code
   close(fd); // Close the file
   // Parse declarations in the source code
   line = 1; // Initialize line counter
   next();  // Start parsing tokens
   while (tk) {  // Continue parsing until no more tokens are left
       bt = INT; //  Default base type is int
       if (tk == Int) next(); // Handle 'int' type
       else if (tk == Char) { next(); bt = CHAR; } // Handle 'char' type
       else if (tk == Enum) { // Handle 'enum' type
           next();
           if (tk != '{') next();  // Skip if no opening brace
           if (tk == '{') {        // Handle enum block
               next();  // Move to the next token
               i = 0;            // Initialize enum value counter
               while (tk != '}') { // Parse enum members until closing brace
                   if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; } // Ensure valid identifier
                   next(); // Move to the next token
                   if (tk == Assign) { // Handle explicit enum value assignment
                       next();// Move to the next token
                       if (tk != Num) { printf("%d: bad enum initializer\n", line); return -1; } // Ensure valid number
                       i = ival;  // Set enum value
                       next();// Move to the next token
                   }
                   id[Class] = Num; id[Type] = INT; id[Val] = i++; // Add enum member to symbol table
                   if (tk == ',') next(); // Handle comma separator
               }
               next(); // Move past the closing brace
           }
       }
       // Loop to parse global declarations or function definitions
       while (tk != ';' && tk != '}') {
           ty = bt; // Set the current type to the base type (INT or CHAR)
           // Handle pointer types (e.g., int*, char**)
           while (tk == Mul) { next(); ty = ty + PTR; }
           // Ensure the current token is an identifier (variable or function name)
           if (tk != Id) {
               printf("%d: bad global declaration\n", line); // Print error message
               return -1; // Exit with error
           }
           // Check if the identifier is already defined (duplicate declaration)
           if (id[Class]) {
               printf("%d: duplicate global definition\n", line); // Print error message
               return -1; // Exit with error
           }
           next(); // Move to the next token after the identifier
           id[Type] = ty; // Set the type of the identifier in the symbol table
           // Handle function definitions
           if (tk == '(') { // function
               id[Class] = Fun; // Mark the identifier as a function in the symbol table
               id[Val] = (int)(e + 1); // Set the function's value to the current code position (e + 1)
               next();  // Move to the next token
               i = 0;// Initialize parameter counter
               // Parse function parameters
               while (tk != ')') {
                   ty = INT; // Default parameter type is INT
                   if (tk == Int) next(); // Handle 'int' type
                   else if (tk == Char) { next(); ty = CHAR; } // Handle 'char' type
                   // Handle pointer types for parameters (e.g., int*, char**)
                   while (tk == Mul) { next(); ty = ty + PTR; }
                   // Ensure the parameter is a valid identifier
                   if (tk != Id) {
                       printf("%d: bad parameter declaration\n", line); // Print error message
                       return -1; // Exit with error
                       if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }
                   }
                   // Save the current class, type, and value of the identifier and update the identifier as a local variable
                   id[HClass] = id[Class]; id[Class] = Loc;
                   id[HType]  = id[Type];  id[Type] = ty;
                   id[HVal]   = id[Val];   id[Val] = i++; // Assign a unique index to the parameter
                   next(); // Move to the next token
                   if (tk == ',') next();   // Handle comma-separated parameters
               }
               next(); // Move past the closing parenthesis
               // Ensure the function body starts with '{'
               if (tk != '{') { printf("%d: bad function definition\n", line); return -1; }
               loc = ++i; // Set the local variable offset (starting after parameters)
               next(); // Move to the next token
               // Parse local variable declarations inside the function
               while (tk == Int || tk == Char) {
                   bt = (tk == Int) ? INT : CHAR; // Set the base type for local variables
                   next(); // Move to the next token
                   while (tk != ';') {    // Parse multiple local variables separated by commas
                       ty = bt; // Set the current type to the base type
                       while (tk == Mul) { next(); ty = ty + PTR; }
                       // Ensure the local variable is a valid identifier
                       if (tk != Id) { printf("%d: bad local declaration\n", line); return -1; }
                       if (id[Class] == Loc) {  printf("%d: duplicate local definition\n", line); return -1; }
                       // Save the current class, type, and value of the identifier and update the identifier as a local variable
                       id[HClass] = id[Class]; id[Class] = Loc;
                       id[HType]  = id[Type];  id[Type] = ty;
                       id[HVal]   = id[Val];   id[Val] = ++i; // Assign a unique index to the local variable
                       next();  // Move to the next token
                       if (tk == ',') next();         // Handle comma-separated local variables
                   }
                   next();  // Move past the semicolon
               }
               *++e = ENT; // Emit the Enter instruction to set up the functions stack frame
               *++e = i - loc;  // Emit the size of the local variable area
               while (tk != '}') stmt(); // Parse the function body (statements inside '{' and '}')
               *++e = LEV;  // Emit Leave instruction to clean up the stack frame and return
               id = sym; // unwind symbol table locals .. reset the symbol table pointer
               while (id[Tk]) {
                   if (id[Class] == Loc)
                       id[Class] = id[HClass]; // Restore the original class
                   id[Type] = id[HType];   // Restore the original type
                   id[Val] = id[HVal];     // Restore the original value
                   id = id + Idsz; // Move to the next symbol table entry
               }
           }
           // Handle global variable declarations
           else {
               id[Class] = Glo; // Mark the identifier as a global variable
               id[Val] = (int)data; // Set the variable's address in the data area
               data = data + sizeof(int); // Allocate space for the variable in the data area
           }
           // Handle comma-separated global declarations
           if (tk == ',') next(); // If a comma is found, move to the next token for additional declarations
       }
       next(); // Move to the next token after the declaration block
   }
   if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; } // Check for the existence of the main function; if not defined, print error message and return -1
   if (src) return 0;  // If the source code is not null, return 0 indicating successful execution
   // Set up the stack for the virtual machine
   bp = sp = (int *)((int)sp + poolsz); // Initialize base pointer "bp" and stack pointer "sp" to the top of the stack
   *--sp = EXIT; // call exit if main returns
   *--sp = PSH; t = sp; // Push PSH instruction and save the stack pointer
   *--sp = argc;  // Push argc.. number of command-line arguments
   *--sp = (int)argv; // Push argv .. command-line arguments
   *--sp = (int)t; // Push the saved stack pointer onto the stack for later use
   // run...
   cycle = 0; // Initialize the cycle counter to keep track of the number of executed instructions
   while (1) {
       i = *pc++; ++cycle;  // Fetch the next instruction and increment the program counter (pc) then increment the cycle counter
       // Debugging: Print the current instruction and its details
       if (debug) {
           printf("%d> %.4s", cycle, // Print cycle number and instruction mnemonic
               &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[i * 5]);  // Look up instruction name
           if (i <= ADJ) printf(" %d\n", *pc); else printf("\n"); // Print operand if applicable
       }
       // Execute the fetched instruction
       if (i == LEA) a = (int)(bp + *pc++); // Load local address: a = bp + offset
       else if (i == IMM) a = *pc++; // Load immediate value: a = value
       else if (i == JMP) pc = (int *)*pc; // Jump to the address specified by the next instruction
       else if (i == JSR) { *--sp = (int)(pc + 1); pc = (int *)*pc; } // Jump to subroutine, saving the return address on the stack
       else if (i == BZ) pc = a ? pc + 1 : (int *)*pc; // Branch if zero: if (a == 0) pc = address
       else if (i == BNZ) pc = a ? (int *)*pc : pc + 1; // Branch if not zero: if (a != 0) pc = address
       else if (i == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; } // Enter a subroutine, saving the base pointer and adjusting the stack
       else if (i == ADJ) sp = sp + *pc++; // Adjust stack: sp = sp + value
       else if (i == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // Leave the subroutine, restoring the base pointer and program counter
       else if (i == LI) a = *(int *)a; // Load integer: a = *a
       else if (i == LC) a = *(char *)a; // Load character: a = *a
       else if (i == SI) *(int *)*sp++ = a; // Store integer: *sp = a
       else if (i == SC) a = *(char *)*sp++ = a; // Store character: *sp = a
       else if (i == PSH) *--sp = a; // Push value onto stack: *--sp = a
       // Arithmetic and logical operations
       else if (i == OR)  a = *sp++ |  a; // Bitwise OR: a = *sp | a
       else if (i == XOR) a = *sp++ ^  a; // Bitwise XOR: a = *sp ^ a
       else if (i == AND) a = *sp++ &  a; // Bitwise AND: a = *sp & a
       else if (i == EQ)  a = *sp++ == a; // Equal: a = (*sp == a)
       else if (i == NE)  a = *sp++ != a; // Not equal: a = (*sp != a)
       else if (i == LT)  a = *sp++ <  a; // Less than: a = (*sp < a)
       else if (i == GT)  a = *sp++ >  a; // Greater than: a = (*sp > a)
       else if (i == LE)  a = *sp++ <= a; // Less than or equal: a = (*sp <= a)
       else if (i == GE)  a = *sp++ >= a; // Greater than or equal: a = (*sp >= a)
       else if (i == SHL) a = *sp++ << a; // Shift left: a = *sp << a
       else if (i == SHR) a = *sp++ >> a; // Shift right: a = *sp >> a
       else if (i == ADD) a = *sp++ +  a; // Add: a = *sp + a
       else if (i == SUB) a = *sp++ -  a; // Subtract: a = *sp - a
       else if (i == MUL) a = *sp++ *  a; // Multiply: a = *sp * a
       else if (i == DIV) a = *sp++ /  a; // Divide: a = *sp / a
       else if (i == MOD) a = *sp++ %  a; // Modulo: a = *sp % a
       // System calls and library functions
       else if (i == OPEN) a = open((char *)sp[1], *sp); // Open a file and store the result in 'a'
       else if (i == READ) a = read(sp[2], (char *)sp[1], *sp); // Read from a file and store the result in 'a'
       else if (i == CLOS) a = close(*sp);  // Close a file and store the result in 'a'
       else if (i == PRTF) { // Print formatted string
           t = sp + pc[1]; // Calculate argument pointer
           a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); // Call printf
       }
       else if (i == MALC) a = (int)malloc(*sp); // Allocate memory: a = malloc(size) and store the address in 'a'
       else if (i == FREE) free((void *)*sp); // Free allocated memory: free(ptr)
       else if (i == MSET) a = (int)memset((char *)sp[2], sp[1], *sp); // Set memory to a specified value
       else if (i == MCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp); // Compare memory
       else if (i == EXIT) { // Exit program
           printf("exit(%d) cycle = %d\n", *sp, cycle); // Print exit status and cycle count
           return *sp; // Return exit status
       }
       else { // Handle unknown instructions
           printf("unknown instruction = %d! cycle = %d\n", i, cycle); // Print error message
           return -1; // Exit with error
       }
   }
}
