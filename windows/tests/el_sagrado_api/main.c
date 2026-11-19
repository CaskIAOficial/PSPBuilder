#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>

PSP_MODULE_INFO("pspbuilder_api_test", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/*
 * Link-time regression fixture based on the PSP APIs used by EL SAGRADO
 * Fase 18. Keeping references in a used data section forces the linker to
 * resolve these symbols without copying or modifying the user's game source.
 */
typedef void (*generic_psp_api)(void);
static generic_psp_api volatile api_refs[] __attribute__((used)) = {
    (generic_psp_api)pspDebugScreenEnableBackColor,
    (generic_psp_api)pspDebugScreenInit,
    (generic_psp_api)pspDebugScreenPrintf,
    (generic_psp_api)pspDebugScreenSetOffset,
    (generic_psp_api)pspDebugScreenSetTextColor,
    (generic_psp_api)pspDebugScreenSetXY,
    (generic_psp_api)sceAudioChRelease,
    (generic_psp_api)sceAudioChReserve,
    (generic_psp_api)sceAudioOutputBlocking,
    (generic_psp_api)sceCtrlPeekBufferPositive,
    (generic_psp_api)sceCtrlSetSamplingCycle,
    (generic_psp_api)sceCtrlSetSamplingMode,
    (generic_psp_api)sceDisplaySetFrameBuf,
    (generic_psp_api)sceDisplayWaitVblankStart,
    (generic_psp_api)sceGeEdramGetAddr,
    (generic_psp_api)sceIoClose,
    (generic_psp_api)sceIoOpen,
    (generic_psp_api)sceIoRead,
    (generic_psp_api)sceIoWrite,
    (generic_psp_api)sceKernelCreateCallback,
    (generic_psp_api)sceKernelCreateThread,
    (generic_psp_api)sceKernelExitGame,
    (generic_psp_api)sceKernelRegisterExitCallback,
    (generic_psp_api)sceKernelSleepThreadCB,
    (generic_psp_api)sceKernelStartThread
};

int main(void) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("PSP Builder API link test OK.\n");
    sceKernelSleepThread();
    return 0;
}
