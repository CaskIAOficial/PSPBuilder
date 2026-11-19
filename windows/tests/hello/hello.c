#include <pspkernel.h>
#include <pspdebug.h>

PSP_MODULE_INFO("pspbuilder_test", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

int main(void) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("PSP Builder Windows smoke test OK.\n");
    sceKernelSleepThread();
    return 0;
}
