#include "Core.h"
#if defined CC_BUILD_PS3
#include "Window.h"
#include "Platform.h"
#include "Input.h"
#include "Event.h"
#include "Graphics.h"
#include "String.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "ExtMath.h"
#include "Logger.h"
#include <io/pad.h>
#include <io/kb.h> 
#include <sysutil/sysutil.h>
#include <sysutil/video.h>

static cc_bool launcherMode;
static padInfo  pad_info;
static padData  pad_data;
static KbInfo   kb_info;
static KbData   kb_data;
static KbConfig kb_config;

struct _DisplayData DisplayInfo;
struct _WinData WindowInfo;
// no DPI scaling on PS Vita
int Display_ScaleX(int x) { return x; }
int Display_ScaleY(int y) { return y; }

static void sysutil_callback(u64 status, u64 param, void* usrdata) {
	switch (status) {
		case SYSUTIL_EXIT_GAME:
			Event_RaiseVoid(&WindowEvents.Closing);
			WindowInfo.Exists = false;
			break;
	}
}

void Window_Init(void) {
	sysUtilRegisterCallback(0, sysutil_callback, NULL);
	
	videoState state;
	videoResolution resolution;
	
	videoGetState(0, 0, &state);
	videoGetResolution(state.displayMode.resolution, &resolution);
      
	DisplayInfo.Width  = resolution.width;
	DisplayInfo.Height = resolution.height;
	DisplayInfo.Depth  = 4; // 32 bit
	DisplayInfo.ScaleX = 1;
	DisplayInfo.ScaleY = 1;
	
	WindowInfo.Width   = resolution.width;
	WindowInfo.Height  = resolution.height;
	WindowInfo.Focused = true;
	WindowInfo.Exists  = true;

	Input.Sources = INPUT_SOURCE_GAMEPAD;
	DisplayInfo.ContentOffsetX = 10;
	DisplayInfo.ContentOffsetY = 10;

	ioPadInit(MAX_PORT_NUM);
	ioKbInit(MAX_KB_PORT_NUM);
	ioKbGetConfiguration(0, &kb_config);
}

void Window_Create2D(int width, int height) { 
	launcherMode = true;
	Gfx_Create(); // launcher also uses RSX to draw
}

void Window_Create3D(int width, int height) { 
	launcherMode = false; 
}

void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { } // TODO sceClipboardGetText
void Clipboard_SetText(const cc_string* value) { } // TODO sceClipboardSetText

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }

void Window_Close(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}


/*########################################################################################################################*
*--------------------------------------------------Keyboard processing----------------------------------------------------*
*#########################################################################################################################*/
static KbMkey old_mods;
#define ToggleMod(field, btn) if (diff._KbMkeyU._KbMkeyS. field) Input_Set(btn, mods->_KbMkeyU._KbMkeyS. field);
static void ProcessKBModifiers(KbMkey* mods) {
	KbMkey diff;
	diff._KbMkeyU.mkeys = mods->_KbMkeyU.mkeys ^ old_mods._KbMkeyU.mkeys;
	
	ToggleMod(l_alt,   CCKEY_LALT);
	ToggleMod(r_alt,   CCKEY_RALT);
	ToggleMod(l_ctrl,  CCKEY_LCTRL);
	ToggleMod(r_ctrl,  CCKEY_RCTRL);
	ToggleMod(l_shift, CCKEY_LSHIFT);
	ToggleMod(r_shift, CCKEY_RSHIFT);
	ToggleMod(l_win,   CCKEY_LWIN);
	ToggleMod(r_win,   CCKEY_RWIN);
	
	old_mods = *mods;
}
// TODO ProcessKBButtons()
// TODO: Call at init ioKbSetCodeType(0, KB_CODETYPE_RAW); ioKbSetReadMode(0, KB_RMODE_INPUTCHAR);
static void ProcessKBTextInput(void) {
	for (int i = 0; i < kb_data.nb_keycode; i++)
	{
		int rawcode = kb_data.keycode[i];
		if (!rawcode) continue;
		int unicode = ioKbCnvRawCode(kb_config.mapping, kb_data.mkey, kb_data.led, rawcode);
		
		char C = unicode;
		//Platform_Log4("%i --> %i / %h / %r", &rawcode, &unicode, &unicode, &C);
	}
}


/*########################################################################################################################*
*----------------------------------------------------Input processing-----------------------------------------------------*
*#########################################################################################################################*/
static void HandleButtons(padData* data) {
	//Platform_Log2("BUTTONS: %h (%h)", &data->button[2], &data->button[0]);
	Input_SetNonRepeatable(CCPAD_A, data->BTN_TRIANGLE);
	Input_SetNonRepeatable(CCPAD_B, data->BTN_SQUARE);
	Input_SetNonRepeatable(CCPAD_X, data->BTN_CROSS);
	Input_SetNonRepeatable(CCPAD_Y, data->BTN_CIRCLE);
      
	Input_SetNonRepeatable(CCPAD_START,  data->BTN_START);
	Input_SetNonRepeatable(CCPAD_SELECT, data->BTN_SELECT);

	Input_SetNonRepeatable(CCPAD_LEFT,   data->BTN_LEFT);
	Input_SetNonRepeatable(CCPAD_RIGHT,  data->BTN_RIGHT);
	Input_SetNonRepeatable(CCPAD_UP,     data->BTN_UP);
	Input_SetNonRepeatable(CCPAD_DOWN,   data->BTN_DOWN);
	
	Input_SetNonRepeatable(CCPAD_L,  data->BTN_L1);
	Input_SetNonRepeatable(CCPAD_R,  data->BTN_R1);
	Input_SetNonRepeatable(CCPAD_ZL, data->BTN_L2);
	Input_SetNonRepeatable(CCPAD_ZR, data->BTN_R2);
}

static void HandleJoystick_Left(int x, int y) {
	if (Math_AbsI(x) <= 32) x = 0;
	if (Math_AbsI(y) <= 32) y = 0;	
	
	if (x == 0 && y == 0) return;
	Input.JoystickMovement = true;
	Input.JoystickAngle    = Math_Atan2(x, -y);
}

static void HandleJoystick_Right(int x, int y, double delta) {
	float scale = (delta * 60.0) / 64.0f;
	
	if (Math_AbsI(x) <= 32) x = 0;
	if (Math_AbsI(y) <= 32) y = 0;
	
	Event_RaiseRawMove(&PointerEvents.RawMoved, x * scale, y * scale);	
}

static void ProcessPadInput(double delta, padData* pad) {
	HandleButtons(pad);
	HandleJoystick_Left( pad->ANA_L_H - 0x80, pad->ANA_L_V - 0x80);
	HandleJoystick_Right(pad->ANA_R_H - 0x80, pad->ANA_R_V - 0x80, delta);
}


void Window_ProcessEvents(double delta) {
	Input.JoystickMovement = false;
	
	ioPadGetInfo(&pad_info);
	if (pad_info.status[0]) {
		ioPadGetData(0, &pad_data);
		ProcessPadInput(delta, &pad_data);
	}
	
	// TODO set InputSource keyboard
	ioKbGetInfo(&kb_info);
	if (kb_info.status[0]) {
		int res = ioKbRead(0, &kb_data);
		if (res == 0 && kb_data.nb_keycode > 0) {
			ProcessKBModifiers(&kb_data.mkey);
			ProcessKBTextInput();
		}
	}
}

void Cursor_SetPosition(int x, int y) { } // Makes no sense for PS Vita

void Window_EnableRawMouse(void)  { Input.RawMode = true; }
void Window_UpdateRawMouse(void)  {  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }


/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
static struct Bitmap fb_bmp;
static u32 fb_offset;

extern u32* Gfx_AllocImage(u32* offset, s32 w, s32 h);
extern void Gfx_TransferImage(u32 offset, s32 w, s32 h);

void Window_AllocFramebuffer(struct Bitmap* bmp) {
	u32* pixels = Gfx_AllocImage(&fb_offset, bmp->width, bmp->height);
	bmp->scan0  = pixels;
	fb_bmp      = *bmp;
	
	Gfx_ClearCol(PackedCol_Make(0x40, 0x60, 0x80, 0xFF));
}

void Window_DrawFramebuffer(Rect2D r) {
	// TODO test
	Gfx_BeginFrame();
	Gfx_Clear();
	// TODO: Only transfer dirty region instead of the entire bitmap
	Gfx_TransferImage(fb_offset, fb_bmp.width, fb_bmp.height);
	Gfx_EndFrame();
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	//Mem_Free(bmp->scan0);
	/* TODO free framebuffer */
}


/*########################################################################################################################*
*------------------------------------------------------Soft keyboard------------------------------------------------------*
*#########################################################################################################################*/
void Window_OpenKeyboard(struct OpenKeyboardArgs* args) { /* TODO implement */ }
void Window_SetKeyboardText(const cc_string* text) { }
void Window_CloseKeyboard(void) { /* TODO implement */ }


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}
#endif