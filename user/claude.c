#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char* interpret_command(char *input) {
    //eventually extend to delegate to an appropriate agent
    printf("Interpreting command...\n");
    char *llm_response = "#include \"user.h\"\n"
                         "int main() {\n"
                         "    printf(\"Listing files in the current directory:\\n\");\n"
                         "    char *args[] = {\"ls\", 0};\n"
                         "    exec(\"ls\", args);\n"
                         "    return 0;\n"
                         "}\n";

    return llm_response;
    // call llm api with input (hardcoded for now)
    // return response
}

void write_code_to_file(const char *code) {
    printf("writing output to file...\n");
    int fd = open("temp.c", O_CREATE | O_WRONLY);
    if (fd < 0) {
        printf("Error: Could not create temp.c\n");
        exit(1);
    }
    write(fd, code, strlen(code)); //write the llms outputted code to the file temp.c
    close(fd);
}

void compile_code() {
    printf("Compiling temp.c...\n");

    // Run the compiler (riscv64-unknown-elf-gcc)
    if (fork() == 0) {  // Child process
        char *args[] = {
            "riscv64-unknown-elf-gcc",  // Compiler
            "temp.c",                   // Source file
            "-o", "temp",               // Output file
            "-nostdlib",                // Avoid standard libraries
            "-ffreestanding",           // For kernel code
            0                           // End of arguments
        };
        exec(args[0], args);  // Run the compiler
        printf("Error: Could not execute compiler.\n");
        exit(1);
    } else {
        wait(0);
    }

    /*
    printf("Compiling temp.c...\n");

    // Set up arguments to call the cross-compiler (e.g., tcc or custom compiler in xv6)
    char *compiler = "tcc"; // Replace with your compiler binary name (e.g., "gcc" or "tcc")
    char *args[] = {"tcc", "-o", "temp", "temp.c", 0}; // Compile temp.c to temp

    // Execute the compiler
    if (exec(compiler, args) < 0) {
        printf("Error: Compilation failed.\n");
        exit(1);
    }
    // If successful, this process will be replaced by the compiler process
    // and won't return here unless compilation fails.
    */
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: claude \"<natural language command>\"\n");
        exit(1);
    }

    char *command = argv[1];

    // call the llm to interpret the nl command - hardcoded response for now
    char *llm_response = interpret_command(command);

    

    write_code_to_file(llm_response);

    // compile the code
    compile_code();

    char *args[] = {"temp", 0};
    exec("temp", args);

    // If exec fails, the program returns here
    printf("Error: Could not execute temp.\n");
    exit(1);

}
