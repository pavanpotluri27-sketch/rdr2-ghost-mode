// Ghost Mode for Red Dead Redemption 2 (story mode): become a see-through, glowing ghost.
// Townsfolk flee, your punch sends people flying, a left click with empty hands blasts everyone in front.
// Everything is on hotkeys set in GhostMode.ini next to the .asi. Needs ScriptHookRDR2 + an ASI loader (dinput8.dll).
// Build: zig cc -target x86_64-windows-gnu -shared -O2 -s -o GhostMode.asi ghostmode.c
// (IS_GOD branches are leftovers of a sibling mode and are always off in this build.)
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

#define MOD_NAME   "GhostMode"
#define MOD_TITLE  "Ghost Mode"
#define IS_GOD     0

typedef void (*fnScriptRegister)(HMODULE, void (*)(void));
typedef void (*fnScriptUnregister)(HMODULE);
typedef void (*fnNativeInit)(uint64_t);
typedef void (*fnNativePush64)(uint64_t);
typedef uint64_t *(*fnNativeCall)(void);
typedef void (*fnScriptWait)(DWORD);
typedef int (*fnGetAllPeds)(int *, int);
typedef void (*KeyboardHandler)(DWORD, WORD, BYTE, BOOL, BOOL, BOOL, BOOL);
typedef void (*fnKbReg)(KeyboardHandler);

static fnScriptRegister pScriptRegister;
static fnScriptUnregister pScriptUnregister;
static fnNativeInit pNativeInit;
static fnNativePush64 pNativePush64;
static fnNativeCall pNativeCall;
static fnScriptWait pScriptWait;
static fnGetAllPeds pGetAllPeds;
static fnKbReg pKbRegister, pKbUnregister;

#define H_PLAYER_PED_ID           0x096275889B8E0EE0ULL
#define H_PLAYER_ID               0x217E9DC48139933DULL
#define H_GET_ENTITY_COORDS       0xA86D5F069399F44DULL
#define H_GET_ENTITY_MODEL        0xDA76A9F39210D365ULL
#define H_IS_ENTITY_DEAD          0x7D5B1F88E7504BBAULL
#define H_DOES_ENTITY_EXIST       0xD42BD6EB2E0F1677ULL
#define H_IS_ENTITY_A_PED         0xCF8176912DDA4EA5ULL
#define H_IS_PED_HUMAN            0xB980061DA992779DULL
#define H_IS_PED_A_PLAYER         0x12534C348C6CB68BULL
#define H_IS_PED_ON_MOUNT         0x460BC76A0E10655EULL
#define H_IS_PED_IN_COMBAT        0x4859F1FC66A6278EULL
#define H_IS_PAUSE_MENU_ACTIVE    0x535384D6067BA42EULL
#define H_GET_HASH_KEY            0xFD340785ADF8CFB7ULL
#define H_GET_CURRENT_PED_WEAPON  0x3A87E44BB9A01D54ULL
#define H_PLAY_AMBIENT_SPEECH     0x8E04FEDD28D42462ULL
#define H_CLEAR_PED_TASKS         0xE1EF3C1216AFF2CDULL
#define H_TASK_LOOK_AT_ENTITY     0x69F4BE8C8CC4796CULL
#define H_GET_OFFSET_IN_WORLD     0x1899F328B0E12848ULL
#define H_DELETE_PED              0xCC0EF140F99365C5ULL
#define H_SET_ENTITY_INVINCIBLE   0xA5C38736C426FCB8ULL
#define H_SET_ENTITY_ALPHA        0x0DF7692B1D9E7BA7ULL
#define H_RESET_ENTITY_ALPHA      0x744B9EF44779D9ABULL
#define H_SET_PED_SCALE           0x25ACFC650B65C538ULL
#define H_FREEZE_ENTITY           0x7D9EFB7AD6B19754ULL
#define H_DRAW_LIGHT              0xD2D9E04C0DF927F4ULL
#define H_VAR_STRING              0xFA925AC00EB830B9ULL
#define H_BG_SET_TEXT_COLOR       0x16FA5CE47F184F1EULL
#define H_BG_SET_TEXT_SCALE       0xA1253A3C870B6843ULL
#define H_DISPLAY_TEXT            0xD79334A4BB99BAD1ULL
#define H_BG_DISPLAY_TEXT         0x16794E044C9EFB58ULL
#define WEAPON_UNARMED            0xA2719263u
#define INPUT_ATTACK              0x07CE1E61u
#define INPUT_MELEE_ATTACK        0xB2F377E8u

static char g_dir[MAX_PATH], g_logPath[MAX_PATH];
static int g_log = 1;

static uint64_t f2u(float f) { uint64_t u = 0; memcpy(&u, &f, sizeof f); return u; }
static float u2f(uint64_t u) { float f; memcpy(&f, &u, sizeof f); return f; }
static uint64_t call0(uint64_t h) { pNativeInit(h); return *pNativeCall(); }
static uint64_t call1(uint64_t h, uint64_t a) { pNativeInit(h); pNativePush64(a); return *pNativeCall(); }
static uint64_t call2(uint64_t h, uint64_t a, uint64_t b) { pNativeInit(h); pNativePush64(a); pNativePush64(b); return *pNativeCall(); }

typedef struct { float x, y, z; } V3;
static V3 readV3(uint64_t *r) { V3 v = { u2f(r[0]), u2f(r[1]), u2f(r[2]) }; return v; }

static V3 entityCoords(int e)
{
	pNativeInit(H_GET_ENTITY_COORDS); pNativePush64((uint32_t)e); pNativePush64(1); pNativePush64(0);
	return readV3(pNativeCall());
}

static V3 offsetOf(int e, float x, float y, float z)
{
	pNativeInit(H_GET_OFFSET_IN_WORLD); pNativePush64((uint32_t)e); pNativePush64(f2u(x)); pNativePush64(f2u(y)); pNativePush64(f2u(z));
	return readV3(pNativeCall());
}

static int exists(int e) { return e && call1(H_DOES_ENTITY_EXIST, (uint32_t)e); }
static int aliveHuman(int q) { return exists(q) && call1(H_IS_PED_HUMAN, (uint32_t)q) && !call1(H_IS_ENTITY_DEAD, (uint32_t)q); }

static void logMsg(const char *fmt, ...)
{
	if (!g_log) return;
	FILE *f = fopen(g_logPath, "a");
	if (!f) return;
	SYSTEMTIME t; GetLocalTime(&t);
	fprintf(f, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
	va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
	fputc('\n', f);
	fclose(f);
}

// ---------- settings (.ini) ----------
static int g_alpha, g_spirit, g_haze, g_outfit, g_scare, g_powers, g_horse, g_noWanted;
static float g_glow, g_glowSize, g_scale;
static int g_color[3];
static float g_sizes[4] = { 1.0f, 1.15f, 1.3f, 2.0f };
static int g_sizeIdx;
static const int g_alphaSteps[3] = { 51, 102, 153 }; // faint / normal / solid (the game uses steps of 51)
static char g_gestureDict[96] = "ai_gestures@gen_male@standing@speaker", g_gestureClip[64] = "neutral_point_r_001";
static uint32_t g_protect[256];
static int g_protectCount;

// ---------- hotkeys ----------
enum { K_TOGGLE, K_SIZE, K_LOOK, K_HAZE, K_HOVER, K_COUNT };
static const char *g_keyNames[K_COUNT] = { "Toggle", "Size", "SeeThrough", "Tails", "Hover" };
static struct { int vk, ctrl, shift, alt; } g_keys[K_COUNT];
static volatile LONG g_keyReq[K_COUNT];

// key names: 0-9, A-Z, F1-F12, NUM0-NUM9, INSERT, DELETE, HOME, END, PAGEUP, PAGEDOWN, BACKSPACE, NONE,
// optionally with CTRL+ / SHIFT+ / ALT+ in front
static void parseKey(int k, const char *txt)
{
	char b[48]; int n = 0;
	for (const char *s = txt; *s && n < 47; s++) if (*s != ' ') b[n++] = (char)toupper((unsigned char)*s);
	b[n] = 0;
	g_keys[k].vk = g_keys[k].ctrl = g_keys[k].shift = g_keys[k].alt = 0;
	char *p = b;
	for (;;) {
		if (!strncmp(p, "CTRL+", 5)) { g_keys[k].ctrl = 1; p += 5; }
		else if (!strncmp(p, "SHIFT+", 6)) { g_keys[k].shift = 1; p += 6; }
		else if (!strncmp(p, "ALT+", 4)) { g_keys[k].alt = 1; p += 4; }
		else break;
	}
	static const struct { const char *n; int vk; } named[] = {
		{ "INSERT", VK_INSERT }, { "DELETE", VK_DELETE }, { "HOME", VK_HOME }, { "END", VK_END },
		{ "PAGEUP", VK_PRIOR }, { "PAGEDOWN", VK_NEXT }, { "BACKSPACE", VK_BACK } };
	for (int i = 0; i < (int)(sizeof named / sizeof named[0]); i++) if (!strcmp(p, named[i].n)) { g_keys[k].vk = named[i].vk; return; }
	if (p[0] == 'F' && p[1] >= '1' && p[1] <= '9') { int f = atoi(p + 1); if (f >= 1 && f <= 12) g_keys[k].vk = VK_F1 + f - 1; return; }
	if (!strncmp(p, "NUM", 3) && p[3] >= '0' && p[3] <= '9' && !p[4]) { g_keys[k].vk = VK_NUMPAD0 + p[3] - '0'; return; }
	if (p[0] && !p[1] && isalnum((unsigned char)p[0])) g_keys[k].vk = p[0];
	// anything else (NONE, empty) = no key
}

static void onKey(DWORD key, WORD rep, BYTE sc, BOOL ext, BOOL alt, BOOL wasDown, BOOL isUp)
{
	(void)rep; (void)sc; (void)ext; (void)wasDown;
	if (!isUp) return; // act once, when the key is let go
	int ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0, shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	for (int k = 0; k < K_COUNT; k++) {
		if (!g_keys[k].vk || (DWORD)g_keys[k].vk != key) continue;
		if (g_keys[k].ctrl != ctrl || g_keys[k].shift != shift || g_keys[k].alt != (alt != 0)) continue;
		InterlockedExchange(&g_keyReq[k], 1);
	}
}

static int keyPressed(int k) { return InterlockedExchange(&g_keyReq[k], 0) != 0; }

// ---------- on-screen message (top centre, game font) ----------
static char g_note[96];
static ULONGLONG g_noteUntil;

static void note(const char *fmt, ...)
{
	va_list a; va_start(a, fmt); vsnprintf(g_note, sizeof g_note, fmt, a); va_end(a);
	g_noteUntil = GetTickCount64() + 2500;
	logMsg("%s", g_note);
}

static void drawNote(void)
{
	if (GetTickCount64() > g_noteUntil || !g_note[0]) return;
	static char t[256];
	snprintf(t, sizeof t, "<FONT FACE='$title' COLOR='#%02X%02X%02X'>%s</FONT>", 200 + g_color[0] / 5, 200 + g_color[1] / 5, 255, g_note);
	pNativeInit(H_BG_SET_TEXT_COLOR); pNativePush64(255); pNativePush64(255); pNativePush64(255); pNativePush64(255); pNativeCall();
	call2(H_BG_SET_TEXT_SCALE, f2u(0.55f), f2u(0.55f));
	pNativeInit(H_VAR_STRING); pNativePush64(10); pNativePush64((uint64_t)"LITERAL_STRING"); pNativePush64((uint64_t)t);
	const char *inner = (const char *)*pNativeCall();
	pNativeInit(H_VAR_STRING); pNativePush64(42); pNativePush64((uint64_t)"COLOR_STRING"); pNativePush64(0); pNativePush64((uint64_t)inner);
	const char *s = (const char *)*pNativeCall();
	float w = (float)strlen(g_note) * 0.0076f; // the game can't measure text: estimate for centring
	pNativeInit(H_DISPLAY_TEXT); pNativePush64((uint64_t)s); pNativePush64(f2u(0.5f - w / 2)); pNativePush64(f2u(0.06f)); pNativeCall();
}

// ---------- people ----------
// ScriptHook's ped list is sometimes empty for a while; then ask the game for the people near the player.
static int getPeds(int *out, int max)
{
	int n = pGetAllPeds(out, max);
	if (n > 0) return n;
	static int64_t buf[2 + 2 * 64];
	memset(buf, 0, sizeof buf);
	((int *)buf)[0] = 64;
	int player = (int)call0(H_PLAYER_PED_ID);
	pNativeInit(0x23F8F5FC7E8C4A6BULL); // GET_PED_NEARBY_PEDS(player, buf, -1, 0)
	pNativePush64((uint32_t)player); pNativePush64((uint64_t)buf); pNativePush64((uint64_t)(int64_t)-1); pNativePush64(0);
	pNativeCall();
	int n2 = 0;
	const int *w = (const int *)buf;
	for (int k = 1; k < (int)(sizeof(buf) / (sizeof(int))) && n2 < max; k++) {
		int q = w[k], dup = 0;
		if (q <= 0 || q == player) continue;
		for (int j = 0; j < n2; j++) if (out[j] == q) { dup = 1; break; }
		if (dup || !exists(q) || !call1(H_IS_ENTITY_A_PED, (uint32_t)q)) continue;
		out[n2++] = q;
	}
	return n2;
}

#define NEAR_MAX 20
static struct { int ped; float d; } g_near[NEAR_MAX]; // people within 30 m, nearest first
static int g_nearCount;

static void scanNear(int player, V3 pp)
{
	static int peds[1024];
	int n = getPeds(peds, 1024);
	g_nearCount = 0;
	for (int i = 0; i < n; i++) {
		int q = peds[i];
		if (q == player || !aliveHuman(q) || call1(H_IS_PED_A_PLAYER, (uint32_t)q)) continue;
		V3 c = entityCoords(q);
		float dx = c.x - pp.x, dy = c.y - pp.y, dz = c.z - pp.z, d = sqrtf(dx * dx + dy * dy + dz * dz);
		if (d >= 30.0f) continue;
		int k;
		if (g_nearCount < NEAR_MAX) k = g_nearCount++;
		else if (d < g_near[NEAR_MAX - 1].d) k = NEAR_MAX - 1;
		else continue;
		while (k > 0 && g_near[k - 1].d > d) { g_near[k] = g_near[k - 1]; k--; }
		g_near[k].ped = q; g_near[k].d = d;
	}
}

// gang / camp story characters (ProtectModels in the .ini) never run or get disintegrated - they stare instead
static int isFriend(int q)
{
	uint32_t m = (uint32_t)call1(H_GET_ENTITY_MODEL, (uint32_t)q);
	for (int i = 0; i < g_protectCount; i++) if (g_protect[i] == m) return 1;
	return 0;
}

static void clearTasks(int ped)
{
	pNativeInit(H_CLEAR_PED_TASKS); pNativePush64((uint32_t)ped); pNativePush64(0); pNativePush64(0); pNativeCall();
}

static void lookAt(int ped, int target, int ms)
{
	pNativeInit(H_TASK_LOOK_AT_ENTITY);
	pNativePush64((uint32_t)ped); pNativePush64((uint32_t)target);
	pNativePush64((uint64_t)(int64_t)ms); pNativePush64(2048); pNativePush64(3); pNativePush64(0);
	pNativeCall();
}

// one of the ped's own recorded lines; the game refuses repeats with STANDARD params, so retry forced
static void shockedSpeech(int ped, int listener)
{
	static const char *lines[] = { "GENERIC_SHOCKED_HIGH", "GENERIC_SHOCKED_MED", "SCREAM_SHOCKED", "GENERIC_FRIGHTENED_HIGH" };
	static const char *params[] = { "SPEECH_PARAMS_STANDARD", "SPEECH_PARAMS_FORCE_NORMAL" };
	for (int l = 0; l < 4; l++)
		for (int i = 0; i < 2; i++) {
			uint64_t p[8] = { (uint64_t)lines[l], 0, 1, (uint32_t)call1(H_GET_HASH_KEY, (uint64_t)params[i]), (uint32_t)listener, 0, 1, 1 };
			if ((int)call2(H_PLAY_AMBIENT_SPEECH, (uint32_t)ped, (uint64_t)p)) return;
		}
}

// ---------- particle effects (the game's own: Agnes Dowd's ghost haze, the Online spirit-animal effects) ----------
#define FX_GHOST_ASSET  "scr_agnesdowd_ghost"
#define FX_GHOST_HAZE   "scr_ped_ghost_haze_stage1"
#define FX_SPIRIT_ASSET "scr_net_camp"
#define FX_SPIRIT_FADE  "scr_net_spirit_animal_fade"
#define FX_SPIRIT_REACT "scr_net_spirit_animal_fire_react"

static int fxReady(const char *asset)
{
	uint32_t h = (uint32_t)call1(H_GET_HASH_KEY, (uint64_t)asset);
	if (call1(0x65BB72F29138F5D6ULL, h)) return 1;  // HAS_NAMED_PTFX_ASSET_LOADED
	call1(0xF2B2353BBC0D4E8FULL, h);                // REQUEST_NAMED_PTFX_ASSET
	return 0;
}

static int fxLoop(const char *asset, const char *name, int entity, float scale)
{
	if (!fxReady(asset)) return 0;
	call1(0xA10DB07FC234DD12ULL, (uint64_t)asset);  // USE_PARTICLE_FX_ASSET
	pNativeInit(0xBD41E1440CE39800ULL);             // START_PARTICLE_FX_LOOPED_ON_ENTITY
	pNativePush64((uint64_t)name); pNativePush64((uint32_t)entity);
	pNativePush64(f2u(0)); pNativePush64(f2u(0)); pNativePush64(f2u(0));
	pNativePush64(f2u(0)); pNativePush64(f2u(0)); pNativePush64(f2u(0));
	pNativePush64(f2u(scale)); pNativePush64(0); pNativePush64(0); pNativePush64(0);
	return (int)*pNativeCall();
}

static void fxOnce(const char *asset, const char *name, int entity, float ox, float oy, float oz, float scale)
{
	if (!fxReady(asset)) return;
	call1(0xA10DB07FC234DD12ULL, (uint64_t)asset);
	pNativeInit(0xFF4C64C513388C12ULL);             // START_PARTICLE_FX_NON_LOOPED_ON_ENTITY
	pNativePush64((uint64_t)name); pNativePush64((uint32_t)entity);
	pNativePush64(f2u(ox)); pNativePush64(f2u(oy)); pNativePush64(f2u(oz));
	pNativePush64(f2u(0)); pNativePush64(f2u(0)); pNativePush64(f2u(0));
	pNativePush64(f2u(scale)); pNativePush64(0); pNativePush64(0); pNativePush64(0);
	pNativeCall();
}

static void fxAt(const char *asset, const char *name, V3 p, float scale)
{
	if (!fxReady(asset)) return;
	call1(0xA10DB07FC234DD12ULL, (uint64_t)asset);
	pNativeInit(0x2E80BF72EF7C87ACULL);             // START_PARTICLE_FX_NON_LOOPED_AT_COORD
	pNativePush64((uint64_t)name); pNativePush64(f2u(p.x)); pNativePush64(f2u(p.y)); pNativePush64(f2u(p.z));
	pNativePush64(f2u(0)); pNativePush64(f2u(0)); pNativePush64(f2u(0));
	pNativePush64(f2u(scale)); pNativePush64(0); pNativePush64(0); pNativePush64(0);
	pNativeCall();
}

static int fxAlive(int h) { return h && call1(0x9DD5AFF561E88F2AULL, (uint32_t)h); } // DOES_PARTICLE_FX_LOOPED_EXIST
static void fxStop(int *h) { if (fxAlive(*h)) call2(0x22970F3A088B133BULL, (uint32_t)*h, 0); *h = 0; } // STOP_PARTICLE_FX_LOOPED

// ---------- flying people (punch / blast) ----------
// Launched people go ragdoll and lose gravity for a moment, so they float up and away, then crash down.
static struct { int ped; int frames; float vx, vy, vz; ULONGLONG gravityAt; } g_fly[24];

static void unseat(int ped)
{
	if (call2(0x997ABD671D25CA0BULL, (uint32_t)ped, 0)) call1(0x45BBCBA77C29A841ULL, (uint32_t)ped); // IS_PED_IN_ANY_VEHICLE -> KNOCK_PED_OFF_VEHICLE
	else if (call1(H_IS_PED_ON_MOUNT, (uint32_t)ped)) {
		pNativeInit(0x5337B721C51883A9ULL); pNativePush64((uint32_t)ped); pNativePush64(1); pNativePush64(1); pNativeCall(); // _REMOVE_PED_FROM_MOUNT
	}
}

static void launch(int ped, float dirX, float dirY, float power)
{
	int slot = 0;
	for (int i = 0; i < 24; i++) {
		if (g_fly[i].ped == ped) return;
		if (!g_fly[i].ped) slot = i;
	}
	unseat(ped);
	pNativeInit(0xAE99FB955581844AULL); // SET_PED_TO_RAGDOLL
	pNativePush64((uint32_t)ped); pNativePush64(4000); pNativePush64(7000); pNativePush64(0); pNativePush64(0); pNativePush64(0); pNativePush64(0);
	pNativeCall();
	call2(0x0CEDB728A1083FA7ULL, (uint32_t)ped, 0); // SET_ENTITY_HAS_GRAVITY off
	fxOnce(FX_SPIRIT_ASSET, FX_SPIRIT_REACT, ped, 0, 0, 0.3f, 1.3f);
	g_fly[slot].ped = ped;
	g_fly[slot].frames = 12;
	g_fly[slot].vx = dirX * power; g_fly[slot].vy = dirY * power; g_fly[slot].vz = 3.0f + power * 0.35f;
	g_fly[slot].gravityAt = GetTickCount64() + 1600;
}

static void flyFrame(void)
{
	ULONGLONG now = GetTickCount64();
	for (int i = 0; i < 24; i++) {
		int ped = g_fly[i].ped;
		if (!ped) continue;
		if (!exists(ped)) { g_fly[i].ped = 0; continue; }
		if (g_fly[i].frames > 0) {
			g_fly[i].frames--;
			pNativeInit(0x1C99BB7B6E96D16FULL); // SET_ENTITY_VELOCITY
			pNativePush64((uint32_t)ped); pNativePush64(f2u(g_fly[i].vx)); pNativePush64(f2u(g_fly[i].vy)); pNativePush64(f2u(g_fly[i].vz));
			pNativeCall();
		}
		if (now >= g_fly[i].gravityAt) {
			call2(0x0CEDB728A1083FA7ULL, (uint32_t)ped, 1);
			g_fly[i].ped = 0;
		}
	}
}

// ---------- disintegrate (Blue Man) ----------
static int g_probe, g_zapPed;
static ULONGLONG g_zapAt;
static struct { int ped; ULONGLONG at; } g_vanish[16];

static void startZap(int player, int target)
{
	// raise a hand first (the game's own talking gesture on the upper body), the burst follows
	if (call1(0x27FF6FE8009B40CAULL, (uint64_t)g_gestureDict)) { // HAS_ANIM_DICT_LOADED
		pNativeInit(0xEA47FE3719165B94ULL); // TASK_PLAY_ANIM
		pNativePush64((uint32_t)player); pNativePush64((uint64_t)g_gestureDict); pNativePush64((uint64_t)g_gestureClip);
		pNativePush64(f2u(4.0f)); pNativePush64(f2u(-4.0f)); pNativePush64((uint64_t)(int64_t)-1);
		pNativePush64(16 | 32); // upper body + secondary: works while walking or hovering
		pNativePush64(f2u(0)); pNativePush64(0); pNativePush64(0); pNativePush64(0); pNativePush64(0); pNativePush64(0);
		pNativeCall();
	} else
		call1(0xA862A2AD321F94B4ULL, (uint64_t)g_gestureDict); // REQUEST_ANIM_DICT
	g_zapPed = target; g_zapAt = GetTickCount64() + 650;
}

static void disintegrate(int player)
{
	pNativeInit(0x0252D2B5582957A6ULL); pNativePush64(2); // GET_GAMEPLAY_CAM_ROT
	V3 rot = readV3(pNativeCall());
	V3 cam = readV3((pNativeInit(0x595320200B98596EULL), pNativeCall())); // GET_GAMEPLAY_CAM_COORD
	float pitch = rot.x * 0.0174533f, yaw = rot.z * 0.0174533f;
	float fx = -sinf(yaw) * cosf(pitch), fy = cosf(yaw) * cosf(pitch), fz = sinf(pitch);
	static int peds[1024];
	int n = getPeds(peds, 1024), best = 0;
	float bestMiss = 1e9f;
	for (int i = 0; i < n; i++) {
		int q = peds[i];
		if (q == player || !aliveHuman(q) || isFriend(q)) continue;
		V3 c = entityCoords(q);
		float vx = c.x - cam.x, vy = c.y - cam.y, vz = c.z + 0.3f - cam.z;
		float t = vx * fx + vy * fy + vz * fz;
		if (t < 1.0f || t > 55.0f) continue;
		float mx = vx - t * fx, my = vy - t * fy, mz = vz - t * fz, miss = sqrtf(mx * mx + my * my + mz * mz);
		if (miss > 1.3f + t * 0.07f || miss >= bestMiss) continue;
		bestMiss = miss; best = q;
	}
	if (best) { startZap(player, best); return; }
	// nobody in the ped list: a capsule "beam" along the line of sight finds whoever it touches
	pNativeInit(0x28579D1B8F8AAC80ULL); // START_SHAPE_TEST_CAPSULE
	pNativePush64(f2u(cam.x)); pNativePush64(f2u(cam.y)); pNativePush64(f2u(cam.z));
	pNativePush64(f2u(cam.x + fx * 60.0f)); pNativePush64(f2u(cam.y + fy * 60.0f)); pNativePush64(f2u(cam.z + fz * 60.0f));
	pNativePush64(f2u(0.7f)); pNativePush64(0x1FF); pNativePush64((uint32_t)player); pNativePush64(7);
	g_probe = (int)*pNativeCall();
}

static void probeFrame(int player)
{
	if (!g_probe) return;
	int hit = 0, ent = 0;
	uint64_t endc[3], norm[3];
	pNativeInit(0xEDE8AC7C5108FB1DULL); // GET_SHAPE_TEST_RESULT
	pNativePush64((uint32_t)g_probe); pNativePush64((uint64_t)&hit); pNativePush64((uint64_t)endc); pNativePush64((uint64_t)norm); pNativePush64((uint64_t)&ent);
	int status = (int)*pNativeCall();
	if (status == 1) return; // still running
	g_probe = 0;
	if (exists(ent) && call1(H_IS_ENTITY_A_PED, (uint32_t)ent)) {
		if (!call1(H_IS_PED_HUMAN, (uint32_t)ent)) ent = (int)call2(0xB676EFDA03DADA52ULL, (uint32_t)ent, 0); // hit a horse: its rider
		if (ent && ent != player && aliveHuman(ent) && !isFriend(ent)) { startZap(player, ent); return; }
	}
	logMsg("disintegrate: nobody in sight");
}

static void zapFrame(int player)
{
	static int stage;
	if (!g_zapPed || GetTickCount64() < g_zapAt) return;
	int q = g_zapPed;
	if (!exists(q)) { g_zapPed = 0; stage = 0; return; }
	if (stage == 0) { // sparks first, while they can still be seen inside them
		V3 c = entityCoords(q);
		c.z += 0.3f;
		if (call1(H_IS_PED_ON_MOUNT, (uint32_t)q) || call2(0x997ABD671D25CA0BULL, (uint32_t)q, 0)) unseat(q);
		else call2(H_FREEZE_ENTITY, (uint32_t)q, 1);
		fxAt(FX_SPIRIT_ASSET, FX_SPIRIT_REACT, c, 2.6f);
		fxAt(FX_SPIRIT_ASSET, "scr_net_spirit_animal_fade_out", c, 2.0f);
		stage = 1;
		g_zapAt = GetTickCount64() + 350;
		return;
	}
	stage = 0;
	g_zapPed = 0;
	call1(0x176CECF6F920D707ULL, (uint32_t)player);  // CLEAR_PED_SECONDARY_TASK: hand down
	call2(0x1794B4FCC84D812FULL, (uint32_t)q, 0);    // SET_ENTITY_VISIBLE false
	pNativeInit(0xAC2767ED8BDFAB15ULL); pNativePush64((uint32_t)q); pNativePush64(0); pNativePush64((uint32_t)player); pNativeCall(); // SET_ENTITY_HEALTH 0
	for (int i = 0; i < 16; i++) if (!g_vanish[i].ped) { g_vanish[i].ped = q; g_vanish[i].at = GetTickCount64() + 1500; break; }
	logMsg("disintegrated ped %d", q);
}

static void vanishFrame(void)
{
	ULONGLONG now = GetTickCount64();
	for (int i = 0; i < 16; i++) {
		int q = g_vanish[i].ped;
		if (!q || now < g_vanish[i].at) continue;
		if (exists(q)) {
			pNativeInit(0xDC19C288082E586EULL); pNativePush64((uint32_t)q); pNativePush64(1); pNativePush64(1); pNativeCall(); // SET_ENTITY_AS_MISSION_ENTITY
			call1(H_DELETE_PED, (uint64_t)&q);
		}
		g_vanish[i].ped = 0;
	}
}

// ---------- powers ----------
static int ctlJust(uint32_t control)
{
	int r = (int)call2(0x580417101DDB492FULL, 0, control) != 0;  // IS_CONTROL_JUST_PRESSED
	return r | ((int)call2(0x91AEF906BCA88877ULL, 0, control) != 0); // IS_DISABLED_CONTROL_JUST_PRESSED
}

static void powers(int player, V3 p)
{
	if (!g_powers) return;
	float fx = u2f(call1(0xDB0954E9960F6457ULL, (uint32_t)player)), fy = u2f(call1(0x9A5C073ECBDA7EE7ULL, (uint32_t)player)); // forward x/y
	// punch (melee key, F): whoever is right in front flies
	if (ctlJust(INPUT_MELEE_ATTACK)) {
		int best = 0; float bestD = 2.6f, bx = 0, by = 0;
		for (int i = 0; i < g_nearCount; i++) {
			int q = g_near[i].ped;
			if (g_near[i].d > bestD || !aliveHuman(q)) continue;
			V3 c = entityCoords(q);
			float dx = c.x - p.x, dy = c.y - p.y, len = sqrtf(dx * dx + dy * dy) + 0.001f;
			if ((dx * fx + dy * fy) / len < 0.3f) continue;
			best = q; bestD = g_near[i].d; bx = dx / len; by = dy / len;
		}
		if (best) launch(best, bx, by, 15.0f);
	}
	uint32_t weapon = WEAPON_UNARMED;
	pNativeInit(H_GET_CURRENT_PED_WEAPON);
	pNativePush64((uint32_t)player); pNativePush64((uint64_t)&weapon); pNativePush64(0); pNativePush64(0); pNativePush64(0);
	pNativeCall();
	if (IS_GOD) {
		// he never uses guns: holster it, block the attack button, left click is always the disintegrate
		if (weapon != WEAPON_UNARMED && weapon) {
			pNativeInit(0xADF692B254977C0CULL); // SET_CURRENT_PED_WEAPON(player, unarmed, equipNow, ...)
			pNativePush64((uint32_t)player); pNativePush64(WEAPON_UNARMED); pNativePush64(1); pNativePush64(0); pNativePush64(0); pNativePush64(0);
			pNativeCall();
		}
		pNativeInit(0xFE99B66D079CF6BCULL); pNativePush64(0); pNativePush64(INPUT_ATTACK); pNativePush64(0); pNativeCall(); // DISABLE_CONTROL_ACTION
		if (call2(0x91AEF906BCA88877ULL, 0, INPUT_ATTACK)) disintegrate(player);
		return;
	}
	// ghost blast: left click with empty hands throws everyone in front and beside him
	if (weapon != WEAPON_UNARMED && weapon) return;
	if (!call2(0x580417101DDB492FULL, 0, INPUT_ATTACK)) return;
	fxOnce(FX_SPIRIT_ASSET, FX_SPIRIT_REACT, player, 0.25f, 0.5f, 0.35f, 1.0f);
	int hit = 0;
	for (int i = 0; i < g_nearCount; i++) {
		int q = g_near[i].ped;
		if (g_near[i].d > 25.0f || !aliveHuman(q)) continue;
		V3 c = entityCoords(q);
		float dx = c.x - p.x, dy = c.y - p.y, len = sqrtf(dx * dx + dy * dy) + 0.001f;
		if ((dx * fx + dy * fy) / len < -0.3f) continue;
		launch(q, dx / len, dy / len, 16.0f - g_near[i].d * 0.4f);
		hit++;
	}
	logMsg("blast: %d thrown", hit);
}

// ---------- the look ----------
static int g_on, g_hover, g_horse_, g_fxHaze, g_fxSpirit;

static char g_outfitFile[MAX_PATH]; // a Rampage "Saved Outfits" .xml put on when the mode turns on
static int g_strip, g_restore;
static uint32_t g_saved[96];        // the clothes he wore before, put back when the mode turns off
static int g_savedN;

static void updateVariation(int player)
{
	pNativeInit(0xCC8CA3E88256E58FULL); // _UPDATE_PED_VARIATION
	pNativePush64((uint32_t)player); pNativePush64(0); pNativePush64(1); pNativePush64(1); pNativePush64(1); pNativePush64(0);
	pNativeCall();
}

static void applyItem(int player, uint32_t h)
{
	pNativeInit(0xD3A7B003ED343FD9ULL); // _APPLY_SHOP_ITEM_TO_PED(ped, hash, immediately, isMp, p4)
	pNativePush64((uint32_t)player); pNativePush64(h); pNativePush64(1); pNativePush64(0); pNativePush64(0);
	pNativeCall();
}

// clothing categories only - never heads, bodies, hair, beards, eyes or teeth
static void stripClothes(int player)
{
	static const char *clothing[] = { "hats", "shirts_full", "vests", "coats", "coats_closed", "ponchos", "cloaks",
		"pants", "chaps", "boots", "spurs", "spats", "gloves", "gauntlets", "neckwear", "neckties", "masks", "eyewear",
		"suspenders", "belts", "belt_buckles", "gunbelts", "gunbelt_accs", "holsters_left", "holsters_crossdraw",
		"holsters_knife", "satchels", "loadouts", "accessories", "badges", "armor", "jewelry_rings_left",
		"jewelry_rings_right", "jewelry_bracelets", "aprons", "dresses", "skirts" };
	for (int i = 0; i < (int)(sizeof clothing / sizeof clothing[0]); i++) {
		pNativeInit(0xDF631E4BCE1B1FC4ULL); // REMOVE_SHOP_ITEM_FROM_PED_BY_CATEGORY(ped, category, 0, 0)
		pNativePush64((uint32_t)player); pNativePush64(call1(H_GET_HASH_KEY, (uint64_t)clothing[i])); pNativePush64(0); pNativePush64(0);
		pNativeCall();
	}
}

static void saveClothes(int player)
{
	int n = (int)call1(0x90403E8107B60E81ULL, (uint32_t)player); // _GET_NUM_COMPONENTS_IN_PED
	g_savedN = 0;
	for (int i = 0; i < n && g_savedN < 96; i++) {
		int flag = 0; uint32_t state = 0;
		pNativeInit(0x77BA37622E22023BULL); // _GET_SHOP_ITEM_COMPONENT_AT_INDEX(ped, i, resolve, &flag, &state)
		pNativePush64((uint32_t)player); pNativePush64((uint64_t)i); pNativePush64(1); pNativePush64((uint64_t)&flag); pNativePush64((uint64_t)&state);
		uint32_t h = (uint32_t)*pNativeCall();
		if (h) g_saved[g_savedN++] = h;
	}
	logMsg("saved %d items he was wearing", g_savedN);
}

static void restoreClothes(int player)
{
	if (!g_savedN) return;
	stripClothes(player);
	for (int i = 0; i < g_savedN; i++) applyItem(player, g_saved[i]);
	updateVariation(player);
	logMsg("put back %d items", g_savedN);
	g_savedN = 0;
}

static unsigned long xmlNum(const char *s, const char *tag, const char *end)
{
	const char *p = strstr(s, tag);
	return p && p < end ? strtoul(p + strlen(tag), NULL, 0) : 0;
}

// Rampage outfit file: <LoadComponent Hash="0x..."/> = shop items, <MetaTag> = drawable/albedo/normal/material/palette + 3 tints
static int loadOutfitXml(int player)
{
	FILE *f = fopen(g_outfitFile, "rb");
	if (!f) { logMsg("outfit file not found: %s", g_outfitFile); return 0; }
	static char x[65536];
	size_t n = fread(x, 1, sizeof x - 1, f);
	fclose(f);
	x[n] = 0;
	int comps = 0, tags = 0;
	for (const char *p = strstr(x, "<LoadComponent"); p; p = strstr(p + 1, "<LoadComponent")) {
		const char *q = strstr(p, "Hash=\"");
		if (!q) break;
		uint32_t h = (uint32_t)strtoul(q + 6, NULL, 0);
		if (h) { applyItem(player, h); comps++; }
	}
	updateVariation(player);
	for (const char *p = strstr(x, "<MetaTag>"); p; p = strstr(p + 1, "<MetaTag>")) {
		const char *e = strstr(p, "</MetaTag>");
		if (!e) break;
		uint32_t d = (uint32_t)xmlNum(p, "<Drawable>", e);
		if (!d) continue;
		pNativeInit(0xBC6DF00D7A4A6819ULL); // _SET_META_PED_TAG
		pNativePush64((uint32_t)player); pNativePush64(d);
		pNativePush64((uint32_t)xmlNum(p, "<Albedo>", e)); pNativePush64((uint32_t)xmlNum(p, "<Normal>", e));
		pNativePush64((uint32_t)xmlNum(p, "<Material>", e)); pNativePush64((uint32_t)xmlNum(p, "<Palette>", e));
		pNativePush64(xmlNum(p, "<PrimaryColor>", e)); pNativePush64(xmlNum(p, "<SecondaryColor>", e)); pNativePush64(xmlNum(p, "<TertiaryColor>", e));
		pNativeCall();
		tags++;
	}
	updateVariation(player);
	logMsg("outfit %s: %d items, %d tags", g_outfitFile, comps, tags);
	return 1;
}

static void applyOutfit(int player)
{
	if (g_outfitFile[0]) {
		if (g_restore) saveClothes(player);
		if (g_strip) stripClothes(player);
		if (!loadOutfitXml(player)) note("%s: outfit file missing", MOD_TITLE);
		return;
	}
	if (g_outfit < 0) return;
	pNativeInit(0x77FF8D35EEC6BBC4ULL); pNativePush64((uint32_t)player); pNativePush64(g_outfit); pNativePush64(0); pNativeCall(); // _EQUIP_META_PED_OUTFIT_PRESET
	updateVariation(player);
}

static void setHorse(int player, int on)
{
	int m = (on && g_horse && call1(H_IS_PED_ON_MOUNT, (uint32_t)player)) ? (int)call1(0xE7E11B8DCBED1058ULL, (uint32_t)player) : 0; // GET_MOUNT
	if (m != g_horse_) {
		if (exists(g_horse_)) call1(H_RESET_ENTITY_ALPHA, (uint32_t)g_horse_);
		g_horse_ = m;
	}
}

static void setAlpha(int e)
{
	if (g_alpha >= 255) { call1(H_RESET_ENTITY_ALPHA, (uint32_t)e); return; }
	pNativeInit(H_SET_ENTITY_ALPHA); pNativePush64((uint32_t)e); pNativePush64(g_alpha); pNativePush64(0); pNativeCall();
}

static void setOn(int on, int player)
{
	int me = (int)call0(H_PLAYER_ID);
	if (on) {
		// only one power at a time: tell the other mod (same process) to switch off
		SetEnvironmentVariableA("RDR2_POWER_ACTIVE", MOD_NAME);
		g_scale = g_sizes[g_sizeIdx];
		setAlpha(player);
		call2(H_SET_PED_SCALE, (uint32_t)player, f2u(g_scale));
		call2(H_SET_ENTITY_INVINCIBLE, (uint32_t)player, 1);
		call2(0xB5EC6BDAEBCA454CULL, (uint32_t)me, f2u(0.0f)); // SET_PLAYER_NOISE_MULTIPLIER
		applyOutfit(player);
		note("%s ON", MOD_TITLE);
	} else {
		fxStop(&g_fxHaze); fxStop(&g_fxSpirit);
		setHorse(player, 0);
		if (g_hover) { g_hover = 0; call2(H_FREEZE_ENTITY, (uint32_t)player, 0); }
		call1(H_RESET_ENTITY_ALPHA, (uint32_t)player);
		call2(H_SET_PED_SCALE, (uint32_t)player, f2u(1.0f));
		call2(H_SET_ENTITY_INVINCIBLE, (uint32_t)player, 0);
		call2(0xB5EC6BDAEBCA454CULL, (uint32_t)me, f2u(1.0f));
		if (g_restore) restoreClothes(player);
		if (g_noWanted) {
			call1(0xD7FA719CB54866C2ULL, f2u(1.0f));       // SET_WANTED_LEVEL_MULTIPLIER: law back to normal
			call2(0x8674D138391FFB1BULL, (uint32_t)me, 0); // _SET_DISABLE_PLAYER_WANTED_LEVEL off
		}
		note("%s off", MOD_TITLE);
	}
	g_on = on;
}

static void glow(int e, const float (*spots)[3], int n, float scale, float range, const int *rgb)
{
	for (int i = 0; i < n; i++) {
		V3 l = offsetOf(e, spots[i][0] * scale, spots[i][1] * scale, spots[i][2] * scale);
		pNativeInit(H_DRAW_LIGHT);
		pNativePush64(f2u(l.x)); pNativePush64(f2u(l.y)); pNativePush64(f2u(l.z));
		pNativePush64(rgb[0]); pNativePush64(rgb[1]); pNativePush64(rgb[2]);
		pNativePush64(f2u(range * scale)); pNativePush64(f2u(g_glow));
		pNativeCall();
	}
}

static void handleKeys(int player, V3 p)
{
	if (keyPressed(K_TOGGLE)) setOn(!g_on, player);
	if (!g_on) { for (int k = 1; k < K_COUNT; k++) keyPressed(k); return; } // other keys only work while on
	if (keyPressed(K_SIZE)) {
		g_sizeIdx = (g_sizeIdx + 1) % 4;
		g_scale = g_sizes[g_sizeIdx];
		call2(H_SET_PED_SCALE, (uint32_t)player, f2u(g_scale));
		fxStop(&g_fxHaze); fxStop(&g_fxSpirit); // restarted at the new size
		static const char *names[] = { "normal", "big", "huge", "giant" };
		note("Size: %s", names[g_sizeIdx]);
	}
	if (keyPressed(K_LOOK)) {
		int i = 0;
		while (i < 3 && g_alphaSteps[i] != g_alpha) i++;
		g_alpha = g_alphaSteps[(i + 1) % 3];
		setAlpha(player);
		note("See-through: %s", g_alpha == 51 ? "faint" : g_alpha == 102 ? "normal" : "solid");
	}
	if (keyPressed(K_HAZE)) {
		g_haze = !g_haze;
		note("Ghost tails %s", g_haze ? "on" : "off");
	}
	if (keyPressed(K_HOVER)) { // stand in the air ~3 m up, or come back down
		g_hover = !g_hover;
		if (g_hover) {
			pNativeInit(0x239A3351AC1DA385ULL); // SET_ENTITY_COORDS_NO_OFFSET
			pNativePush64((uint32_t)player); pNativePush64(f2u(p.x)); pNativePush64(f2u(p.y)); pNativePush64(f2u(p.z + 3.0f));
			pNativePush64(0); pNativePush64(0); pNativePush64(0);
			pNativeCall();
		}
		call2(H_FREEZE_ENTITY, (uint32_t)player, g_hover);
		note(g_hover ? "Hovering" : "Down");
	}
}

static void frame(int player)
{
	static ULONGLONG nextKeep, nextScare, nextScan;
	static struct { int ped; ULONGLONG at; } stared[24];
	V3 p = entityCoords(player);
	ULONGLONG now = GetTickCount64();
	flyFrame(); // finish flights even after the mode ends
	vanishFrame();
	if (now >= nextScan) {
		nextScan = now + 250;
		char other[32] = "";
		GetEnvironmentVariableA("RDR2_POWER_ACTIVE", other, sizeof other);
		if (g_on && strcmp(other, MOD_NAME)) setOn(0, player); // the other power was switched on
	}
	handleKeys(player, p);
	if (!g_on) return;
	static ULONGLONG nextNear;
	if (now >= nextNear) { nextNear = now + 250; scanNear(player, p); }
	powers(player, p);
	probeFrame(player);
	zapFrame(player);

	// glow ON the body: small short-range lights hugging chest and head from all sides
	static const float spots[][3] = { { 0, 0.45f, 0.35f }, { 0, -0.45f, 0.35f }, { 0.45f, 0, 0.35f }, { -0.45f, 0, 0.35f },
	                                  { 0, 0.35f, 0.85f }, { 0, -0.35f, 0.85f }, { 0, 0, 1.25f } };
	glow(player, spots, 7, g_scale, g_glowSize, g_color);
	setHorse(player, 1);
	if (exists(g_horse_)) {
		static const float hs[][3] = { { 0, 0.9f, 1.1f }, { 0, -0.9f, 1.0f }, { 0.55f, 0, 1.0f }, { -0.55f, 0, 1.0f }, { 0, 1.3f, 1.6f }, { 0, 0, 0.5f } };
		glow(g_horse_, hs, 6, 1.0f, 1.1f, g_color);
	}

	if (now >= nextKeep) { // cutscenes, horses, outfit changes and death can undo the look
		nextKeep = now + 1500;
		setAlpha(player);
		if (exists(g_horse_)) setAlpha(g_horse_);
		call2(H_SET_PED_SCALE, (uint32_t)player, f2u(g_scale));
		call2(H_SET_ENTITY_INVINCIBLE, (uint32_t)player, 1);
		call1(0x8FE22675A5A45817ULL, (uint32_t)player); // CLEAR_PED_BLOOD_DAMAGE
		call1(0x9C720776DAA43E7EULL, (uint32_t)player); // CLEAR_PED_WETNESS
		call1(0x6585D955A68452A5ULL, (uint32_t)player); // CLEAR_PED_ENV_DIRT
		if (g_noWanted) { // a wanted player makes the whole town hide indoors - and the law can't hold a ghost
			int me = (int)call0(H_PLAYER_ID);
			call1(0x4E4B996C928C7AA6ULL, (uint32_t)me);      // CLEAR_PLAYER_WANTED_LEVEL
			call1(0xD7FA719CB54866C2ULL, f2u(0.0f));         // SET_WANTED_LEVEL_MULTIPLIER(0)
			call2(0x8674D138391FFB1BULL, (uint32_t)me, 1);   // _SET_DISABLE_PLAYER_WANTED_LEVEL
		}
		if (g_haze && !fxAlive(g_fxHaze)) g_fxHaze = fxLoop(FX_GHOST_ASSET, FX_GHOST_HAZE, player, g_scale);
		if (!g_haze) fxStop(&g_fxHaze);
		if (g_spirit && !fxAlive(g_fxSpirit)) g_fxSpirit = fxLoop(FX_SPIRIT_ASSET, FX_SPIRIT_FADE, player, g_scale);
		if (!g_spirit) fxStop(&g_fxSpirit);
		fxReady(FX_SPIRIT_ASSET); // keep the burst loaded so the first hit isn't late
		if (IS_GOD) call1(0xA862A2AD321F94B4ULL, (uint64_t)g_gestureDict); // REQUEST_ANIM_DICT for the raised hand
	}

	if (g_scare && now >= nextScare) { // townsfolk run; camp/story characters stop, stare and gasp
		nextScare = now + 1000;
		static int peds[1024];
		int n = getPeds(peds, 1024);
		for (int i = 0; i < n; i++) {
			int q = peds[i];
			if (q == player || !aliveHuman(q) || call1(H_IS_PED_A_PLAYER, (uint32_t)q)) continue;
			V3 c = entityCoords(q);
			float dx = c.x - p.x, dy = c.y - p.y, d2 = dx * dx + dy * dy;
			int hostile = (int)call2(H_IS_PED_IN_COMBAT, (uint32_t)q, (uint32_t)player) != 0;
			if (!hostile && isFriend(q)) {
				if (d2 > 8.0f * 8.0f) continue;
				int slot = -1, oldest = 0;
				for (int k = 0; k < 24; k++) {
					if (stared[k].ped == q) slot = k;
					if (stared[k].at < stared[oldest].at) oldest = k;
				}
				if (slot >= 0 && now - stared[slot].at < 30000) continue;
				if (slot < 0) slot = oldest;
				stared[slot].ped = q; stared[slot].at = now;
				lookAt(q, player, 6000);
				shockedSpeech(q, player);
				continue;
			}
			if (d2 > 18.0f * 18.0f) continue;
			if (hostile) clearTasks(q);                                         // even lawmen drop the fight and run
			else if (call1(0xBBCCE00B381F8482ULL, (uint32_t)q)) continue;      // IS_PED_FLEEING already
			pNativeInit(0xFD45175A6DFD7CE9ULL); // TASK_FLEE_PED
			pNativePush64((uint32_t)q); pNativePush64((uint32_t)player); pNativePush64(0); pNativePush64(0);
			pNativePush64(f2u(-1.0f)); pNativePush64((uint64_t)(int64_t)-1); pNativePush64(0);
			pNativeCall();
		}
	}
}

static void scriptMain(void)
{
	// story characters to protect, by model name -> hash (needs the game running, so done here)
	char ini[MAX_PATH], buf[4096];
	snprintf(ini, sizeof ini, "%s%s.ini", g_dir, MOD_NAME);
	GetPrivateProfileStringA("Protect", "Models", "", buf, sizeof buf, ini);
	g_protectCount = 0;
	for (char *t = strtok(buf, ", \t"); t && g_protectCount < 256; t = strtok(NULL, ", \t"))
		g_protect[g_protectCount++] = (uint32_t)call1(H_GET_HASH_KEY, (uint64_t)t);
	logMsg("%s loaded, %d protected story models", MOD_TITLE, g_protectCount);
	for (;;) {
		if (!call0(H_IS_PAUSE_MENU_ACTIVE)) {
			frame((int)call0(H_PLAYER_PED_ID));
			drawNote();
		}
		pScriptWait(0);
	}
}

static float iniF(const char *ini, const char *sec, const char *key, const char *def)
{
	char b[32];
	GetPrivateProfileStringA(sec, key, def, b, sizeof b, ini);
	return (float)atof(b);
}

static void loadIni(void)
{
	char ini[MAX_PATH], b[64];
	snprintf(ini, sizeof ini, "%s%s.ini", g_dir, MOD_NAME);
	static const char *ghostKeys[K_COUNT] = { "F10", "PAGEUP", "PAGEDOWN", "INSERT", "END" };
	static const char *godKeys[K_COUNT] = { "F7", "PAGEUP", "NONE", "NONE", "END" };
	const char **defKeys = IS_GOD ? godKeys : ghostKeys;
	for (int k = 0; k < K_COUNT; k++) {
		GetPrivateProfileStringA("Keys", g_keyNames[k], defKeys[k], b, sizeof b, ini);
		char *c = strchr(b, ';'); if (c) *c = 0; // allow comments after the value
		parseKey(k, b);
	}
	g_alpha  = GetPrivateProfileIntA("Look", "SeeThrough", IS_GOD ? 255 : 102, ini);
	g_glow   = iniF(ini, "Look", "GlowStrength", IS_GOD ? "20" : "6");
	g_glowSize = iniF(ini, "Look", "GlowSize", IS_GOD ? "1.2" : "0.9");
	char of[MAX_PATH];
	GetPrivateProfileStringA("Look", "OutfitFile", "", of, sizeof of, ini);
	{ char *c = strchr(of, ';'); if (c) *c = 0; size_t L = strlen(of); while (L && (of[L - 1] == ' ' || of[L - 1] == '\t')) of[--L] = 0; }
	if (of[0]) snprintf(g_outfitFile, sizeof g_outfitFile, "%s%s", strchr(of, ':') ? "" : g_dir, of); // relative = next to the .asi
	g_strip   = GetPrivateProfileIntA("Look", "StripClothes", 1, ini);
	g_restore = GetPrivateProfileIntA("Look", "PutClothesBackWhenOff", 1, ini);
	g_spirit = GetPrivateProfileIntA("Look", "Sparkle", IS_GOD ? 0 : 1, ini);
	g_haze   = GetPrivateProfileIntA("Look", "GhostTails", 0, ini);
	g_outfit = GetPrivateProfileIntA("Look", "OutfitPreset", IS_GOD ? -1 : 7, ini);
	g_horse  = GetPrivateProfileIntA("Look", "GlowingHorse", 1, ini);
	GetPrivateProfileStringA("Look", "GlowColor", IS_GOD ? "60 130 255" : "150 195 255", b, sizeof b, ini);
	sscanf(b, "%d %d %d", &g_color[0], &g_color[1], &g_color[2]);
	g_sizeIdx = GetPrivateProfileIntA("Look", "StartSize", 0, ini) & 3;
	g_scare  = GetPrivateProfileIntA("Powers", "PeopleFlee", 1, ini);
	g_powers = GetPrivateProfileIntA("Powers", "Powers", 1, ini);
	g_noWanted = GetPrivateProfileIntA("Powers", "NoWantedLevel", 1, ini);
	GetPrivateProfileStringA("Powers", "HandGesture", "ai_gestures@gen_male@standing@speaker neutral_point_r_001", b, sizeof b, ini);
	sscanf(b, "%95s %63s", g_gestureDict, g_gestureClip);
	g_log = GetPrivateProfileIntA("Other", "Log", 1, ini);
	snprintf(g_logPath, sizeof g_logPath, "%s%s.log", g_dir, MOD_NAME);
	DeleteFileA(g_logPath); // fresh log every game start
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
	(void)r;
	if (reason == DLL_PROCESS_ATTACH) {
		GetModuleFileNameA(h, g_dir, MAX_PATH);
		char *s = strrchr(g_dir, '\\'); if (s) s[1] = 0;
		loadIni();
		HMODULE sh = LoadLibraryA("ScriptHookRDR2.dll");
		if (!sh) return TRUE;
		pScriptRegister   = (fnScriptRegister)GetProcAddress(sh, "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
		pScriptUnregister = (fnScriptUnregister)GetProcAddress(sh, "?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
		pNativeInit       = (fnNativeInit)GetProcAddress(sh, "?nativeInit@@YAX_K@Z");
		pNativePush64     = (fnNativePush64)GetProcAddress(sh, "?nativePush64@@YAX_K@Z");
		pNativeCall       = (fnNativeCall)GetProcAddress(sh, "?nativeCall@@YAPEA_KXZ");
		pScriptWait       = (fnScriptWait)GetProcAddress(sh, "?scriptWait@@YAXK@Z");
		pGetAllPeds       = (fnGetAllPeds)GetProcAddress(sh, "?worldGetAllPeds@@YAHPEAHH@Z");
		if (!pScriptRegister || !pNativeInit || !pNativePush64 || !pNativeCall || !pScriptWait || !pGetAllPeds) return TRUE;
		pKbRegister       = (fnKbReg)GetProcAddress(sh, "?keyboardHandlerRegister@@YAXP6AXKGEHHHH@Z@Z");
		pKbUnregister     = (fnKbReg)GetProcAddress(sh, "?keyboardHandlerUnregister@@YAXP6AXKGEHHHH@Z@Z");
		pScriptRegister(h, scriptMain);
		if (pKbRegister) pKbRegister(onKey);
	} else if (reason == DLL_PROCESS_DETACH) {
		if (pScriptUnregister) pScriptUnregister(h);
		if (pKbUnregister) pKbUnregister(onKey);
	}
	return TRUE;
}
