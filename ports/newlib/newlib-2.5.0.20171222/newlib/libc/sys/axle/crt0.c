#include <fcntl.h>
 
extern void exit(int code);
extern int main(int argc, char** argv);
 
void _start(int argc, char** argv) {
    _init_signal();
    int exit_code = main(argc, argv);
    exit(exit_code);
}
