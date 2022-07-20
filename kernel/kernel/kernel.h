#ifndef KERNEL_H
#define KERNEL_H

// https://stackoverflow.com/questions/195975/how-to-make-a-char-string-from-a-c-macros-value
#define _STR_VALUE(arg)      #arg
#define _FUNCTION_NAME(name) _STR_VALUE(name)

// NOTE: This function name is special-cased in the kernel crash handler. 
#define AMC_EXEC_TRAMPOLINE_NAME _amc_exec_trampoline
#define AMC_EXEC_TRAMPOLINE_NAME_STR _FUNCTION_NAME(AMC_EXEC_TRAMPOLINE_NAME)

// NOTE: This function name is special-cased in the kernel crash handler.
#define FS_SERVER_EXEC_TRAMPOLINE_NAME _fs_server_trampoline
#define FS_SERVER_EXEC_TRAMPOLINE_NAME_STR _FUNCTION_NAME(FS_SERVER_EXEC_TRAMPOLINE_NAME)

// TODO(PT): Move this to a different file? Where?
void FS_SERVER_EXEC_TRAMPOLINE_NAME(uint32_t arg1, uint32_t arg2, uint32_t arg3);

#endif
