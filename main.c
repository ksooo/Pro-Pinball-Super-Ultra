#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <sys/mman.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <SDL.h>

static FT_Library library;

typedef int ETextAlign;

typedef struct {
  uint8_t pad_0x0[0x18];
  uint16_t m_nWidth;            // 0x18
  uint16_t m_nHeight;           // 0x1A
  uint32_t m_nBitsPerComponent; // 0x1C
  void *m_pData;                // 0x20
  uint8_t m_bHasAlpha;          // 0x28 or 0x29
  uint8_t m_bPreMulti;          // 0x29 or 0x28
} CCImage;

// Functions within Pro Pinball 1.2.3 for macOS
static void* CCImage_initWithString = 0x10013f48a;
static void *(*cpp_new)(unsigned long size) = 0x10019f2f4; // operator new[](unsigned long)
static void (*cpp_delete)(void *ptr) = 0x10019f2e8; // operator delete[](void*)

static uint8_t hw_input_state_original[12];
static uint64_t (*hw_input_state)(void) = 0x100074b05;

static void* set_digital_nudge_acceleration_multipliers = 0x1000bf0af;

static float* digital_nudge_x_acceleration_multiplier = 0x10035c688;
static float* digital_nudge_y_acceleration_multiplier = 0x10035c68c;

static void* __ingame_ui = 0x100354808; // Anonymous, but pointer to INGAME_UI
static float* __resume_timer_duration = 0x1001a0ba8; // Anonymous, time in seconds the game remains paused when leaving menu

static void unprotect(void* ptr, size_t size) {
  mprotect((void*)((uintptr_t)ptr & ~0xFFF), (size + 0xFFF) & ~0xFFF, PROT_WRITE | PROT_EXEC | PROT_READ);
}

static void install_detour(void* at, void* function) {
  uint64_t detour_address = (uint64_t)at;
  unprotect(at, 12);
  *(uint8_t *)(detour_address + 0) = 0x48;
  *(uint8_t *)(detour_address + 1) = 0xB8;
  *(uint64_t *)(detour_address + 2) = (uint64_t)function;
  *(uint8_t *)(detour_address + 10) = 0xFF;
  *(uint8_t *)(detour_address + 11) = 0xE0;
}

unsigned int pot(unsigned int v) {
  assert(v != 0);
  unsigned int r = 1;
  while (r < v) {
    r *= 2;
  }
  return r;
}

unsigned int max(unsigned int a, unsigned int b) {
  return (a > b) ? a : b;
}

void(*bloop)(int param_1) = 0x1000931a9;


// Not sure if the game has a function for this, so I wrote my own
static void insert_coin() {
  *(int32_t*)0x100384ad4 += 1;
  bloop(0);
}

static SDL_GameController *controller = NULL;

// These are the bits for the return value of hw_input_state
enum {
  HwInputLeftFlipper = 0x1,
  HwInputRightFlipper = 0x2,
  
  HwInputLaunch = 0x4,
  HwInputStart = 0x8,
  HwInputMagnosave = 0x20,
  
  HwInputNudgeUp = 0x40, 
  HwInputNudgeLeft = 0x80,
  HwInputNudgeRight = 0x100,
  
  HwInputPause = 0x200,
  
  HwInputOperatorRight = 0x1000,
  HwInputOperatorLeft = 0x2000,
  HwInputOperatorDown = 0x4000,
  HwInputOperatorUp = 0x8000
} HwInput;

//FIXME: Listen for events so we don't drop any short presses in low-FPS?
static uint64_t hw_input_state_hook(void) {
  // Revert patch to the original function
  memcpy(hw_input_state, hw_input_state_original, sizeof(hw_input_state_original));
  
  // Call the original function
  uint64_t state_original = hw_input_state();
#if 0
  printf("Original hw_input_state returned 0x%016" PRIX64 "\n", state_original);
#endif
  
  // Re-apply patch for later
  install_detour(hw_input_state, hw_input_state_hook);

  // Get SDL input
  SDL_PumpEvents();
  Uint8* s = SDL_GetKeyboardState(NULL);
  SDL_GameControllerUpdate();
  
  uint64_t state = 0;
  
  static bool insert_coin_held = true;
  if (s[SDL_SCANCODE_C]) {
    if (!insert_coin_held) {
      printf("Inserting coin\n");
      insert_coin();
    }
    insert_coin_held = true;
  } else {
    insert_coin_held = false;
  }

  if (controller) {
    Sint16 lt = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    Sint16 rt = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

    if (lt > 0x1000) { state |= HwInputLeftFlipper; }
    if (rt > 0x1000) { state |= HwInputRightFlipper; }

    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) { state |= HwInputLaunch; }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START)) { state |= HwInputStart; }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) { state |= HwInputMagnosave; }
    
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK)) { state |= HwInputPause; }
    
    Sint16 nx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 ny = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    
    if (nx < -0x6000) { state |= HwInputNudgeLeft; }
    if (nx > +0x6000) { state |= HwInputNudgeRight; }
    
    if (ny < -0x6000) { state |= HwInputNudgeUp; }
    if (ny > +0x6000) { state |= HwInputNudgeUp; }

    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) { state |= HwInputOperatorRight; }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) { state |= HwInputOperatorLeft; }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) { state |= HwInputOperatorDown; }
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) { state |= HwInputOperatorUp; }
  }
  
#if 0    
  printf("Hook adds 0x%016" PRIX64 "\n", state);
#endif

  return state_original | state;
}

 
typedef struct {
  int32_t x;
  int32_t y;
} Location;



typedef struct {
  // 1 = playfield
  // 2 = drain
  // 3 = ball through? [while forcing ball into it?]
  // 4 = ball through? [before start of game?]
  // ...
  // 13 = ball through?
  // 14 = crystal before being rejected (during mount rushmore)
  // 15 = crystal
  // 16 = back of orbit / sideramp / continent start
  // 17 = drop to bumpers? (happens for brief moment after 16)
  // 18 = regular lock (bottom?)
  // 19 = regular lock (center?)
  // ...
  // 25 = arm to lock
  uint32_t layer; // [0] Location of ball

  
  uint32_t unk4; // [1] Some timer / counter
  
  uint32_t unk8; // [2]
  
  uint32_t unkC; // [3] Some timer / counter since ball idle?
  
  int32_t pos_x; // [4]
  int32_t pos_y; // [5]
  int32_t pos_z; // [6]
  int32_t pos_z_second; // [7]
  
  int32_t vel_x; // [8]
  int32_t vel_y; // [9]
  
  uint32_t unk28; // [10]
  uint32_t unk2C; // [11]
  uint32_t unk30; // [12]
  uint32_t unk34; // [13]
  uint32_t unk38; // [14]
  uint32_t unk3C; // [15]
  uint32_t unk40; // [16]
  
  // 0 = playfield
  // 1 = lower ramp
  // 2 = lower left habitrail?
  // 3 = higher ramps
  uint32_t z_level; // [17]
  
  // 0 = on playfield
  // 1 = falling / gravity
  // 2 = held / no gravity
  uint32_t unk48; // [18] Falling state?
  
  // 0 = nothing
  // 1 = rubber
  // 4 = inlane
  // 6 = ramps
  // 7 = habitrails
  uint32_t unk4C; // [19] Wall collision type
  
  uint32_t unk50; // [20]
  uint32_t unk54; // [21]
  uint32_t unk58; // [22]
  uint32_t unk5C; // [23]
  uint32_t unk60; // [24]
  uint32_t unk64; // [25]
  uint32_t unk68; // [26]
  uint32_t unk6C; // [27]
  uint32_t unk70; // [28]
  
  // Probably rotation and spin
  float unk74; // [29]
  float unk78; // [30]
  float unk7C; // [31]
  float unk80; // [32]
  float unk84; // [33]
  
  uint32_t unk88; // [34] Some index?
} Ball;



uint8_t ball_mov[6*4];

static void shoot(Ball* ball, Location* from, Location* to, float speed) {
  float f = 0.1f;
  float eps = 1.0e-5f;
  float from_x = from->x * f;
  float from_y = from->y * f;
  float to_x = to->x * f;
  float to_y = to->y * f;
  float delta_x = to_x - from_x;
  float delta_y = to_y - from_y;
  
  printf("delta is %f %f\n", delta_x, delta_y);  
  
  float delta_sl = delta_x * delta_x + delta_y * delta_y;
  if (delta_sl > eps) {
    float delta_l = sqrtf(delta_sl);
    delta_x = delta_x / delta_l * speed;
    delta_y = delta_y / delta_l * speed;
  }
  
  printf("normalized delta is %f %f\n", delta_x, delta_y);  
  
  //FIXME: Assert that ball is on playfield?
  ball->layer = 1;
  ball->pos_x = from->x;
  ball->pos_y = from->y;
  ball->pos_z = 0;
  ball->pos_z_second = ball->pos_z;
  ball->vel_x = delta_x / f;
  ball->vel_y = delta_y / f;
  ball->z_level = 0;
}

static void set_digital_nudge_acceleration_multipliers_hook(float x, float y) {
  // This function doesn't really impact the nudge very much; nudge strength mostly depends on length of a nudge
#if 0
  printf("Game wanted to set nudge to %f, %f\n", x, y);
#endif
  *digital_nudge_x_acceleration_multiplier = 1.0f;
  *digital_nudge_y_acceleration_multiplier = 1.0f;
  
  
  SDL_PumpEvents();
  Uint8* s = SDL_GetKeyboardState(NULL);
  
  
  int* acs = 0x100388780;
  uint32_t* balls = 0x1003887b0; // off-by-4? - we'll display one more just to be safe
  int ac = *acs; // Get first ball from active ball table
  
  
  Ball* ball = &balls[0x8c / 4 * ac];
   
  Location right_flipper = { 12845067, 7758857 };
  Location right_upper_flipper = { 17229452, 21452418 };
  Location left_flipper = { 7208987, 7758864 };
    
  Location right_orbit = { 17767282, 23165143 };

  Location right_ramp = { 15629877, 23814211 };

  Location crystal = { 10076926, 28030764 };


  Location center_ramp = { 6532203, 31295397 };

  Location left_ramp = { 5549541, 21263751 };
  Location left_orbit = { 3609768, 20051757 };


  Location sideramp = { 6732304, 27471462 };


  if (s[SDL_SCANCODE_1]) {
    printf("Shooting left orbit\n");
    shoot(ball, &right_flipper, &left_orbit, 2000000.0f);
  }
  if (s[SDL_SCANCODE_2]) {
    printf("Shooting left ramp\n");
    shoot(ball, &right_flipper, &left_ramp, 2000000.0f);
  }
  if (s[SDL_SCANCODE_3]) {
    printf("Shooting sideramp\n");
    shoot(ball, &right_upper_flipper, &sideramp, 2000000.0f);
  }
  if (s[SDL_SCANCODE_4]) {
    printf("Shooting center ramp\n");
    shoot(ball, &right_flipper, &center_ramp, 2000000.0f);
  }
  if (s[SDL_SCANCODE_5]) {
    printf("Shooting crystal\n");
    shoot(ball, &left_flipper, &crystal, 2000000.0f);
  }
  if (s[SDL_SCANCODE_6]) {
    printf("Shooting right ramp\n");
    shoot(ball, &left_flipper, &right_ramp, 2000000.0f);
  }
  if (s[SDL_SCANCODE_7]) {
    printf("Shooting right orbit\n");
    shoot(ball, &left_flipper, &right_orbit, 2000000.0f);
  }
 
 
  if (s[SDL_SCANCODE_N]) {
    printf("Location stored = { %d, %d };\n", ball->pos_x, ball->pos_y);
    memcpy(ball_mov, &balls[0x8c / 4 * ac + 4], sizeof(ball_mov));
  }
  if (s[SDL_SCANCODE_M]) {
    printf("loaded\n");
    memcpy(&balls[0x8c / 4 * ac + 4], ball_mov, sizeof(ball_mov));
  }

  if (s[SDL_SCANCODE_D]) {
    for(int i = 0; i < 0x8c / 4 + 1; i++) {
      int32_t v = balls[0x8C / 4 * ac + i];
      printf("[%d] 0x%08X; %d; %f\n", i, (uint32_t)v, v, *(float*)&v);
    }
  }

}

// Reimplementation of cocos2dx 2.6.6 CCImage::initWithString (platform/CCImage.h) MacOS backend
static bool CCImage_initWithString_hook(CCImage *_this, const char *pText,
                                   int nWidth, int nHeight,
                                   ETextAlign eAlignMask, const char *pFontName,
                                   int nSize) {
  int error;

  assert(!strcmp(pFontName, "Oswald-Regular"));
  // assert(nWidth == 0);
  // assert(nHeight == 0);
  assert(nSize > 0);
  assert((eAlignMask == (0x10 | 0x2 | 0x1))    // kAlignTop
      || (eAlignMask == (0x20 | 0x10 | 0x2))); // kAlignRight

  printf("Trying to create texture for '%s', in font '%s', size %d, image %dx%d; align: %d\n",
         pText, pFontName, nSize, nWidth, nHeight, eAlignMask);

  unsigned int POTWide = 0;
  unsigned int POTHigh = 0;

#if 0
	// Allocate a buffer for the texture
	unsigned char* dataNew = cpp_new(textureSize); //  new unsigned char[texture_size]
	if (dataNew) {
	    return false;
	}
#endif

  FT_Face face;

  error = FT_New_Face(library, "/Applications/Pro Pinball.app/Contents/Resources/fonts/Oswald-Regular.ttf", 0, &face);
  if (error == FT_Err_Unknown_File_Format) {
    // The font file could be opened and read, but it appears that its font format is unsupported
    assert(false);
  } else if (error) {
    // Another error code means that the font file could not be opened or read, or that it is broken...
    assert(false);
  }

  FT_Select_Charmap(face, ft_encoding_unicode);

  FT_GlyphSlot slot = face->glyph;

  unsigned int line_height = nSize; // FIXME: Can we retrieve line_height elsewhere?

#if 0
error = FT_Set_Char_Size(
          face,    /* handle to face object           */
          0,       /* char_width in 1/64th of points  */
          16*64,   /* char_height in 1/64th of points */
          300,     /* horizontal device resolution    */
          300 );   /* vertical device resolution      */
#else
  error = FT_Set_Pixel_Sizes(face,         /* handle to face object */
                             0,            /* pixel_width           */
                             line_height); /* pixel_height          */
#endif
  if (error) {
    assert(false);
  }

  // Used for finding the buffer size
  // FIXME: Shouldn't be necessary, we can use the real vars directly
  int bound_w = 0;
  int bound_h = 0;

  // Offset for alignment
  unsigned int dx = 0;
  unsigned int dy = 0;

  // This will be the pointer to the pixel data
  uint8_t *data = NULL;

  // Get number of glyphs
  unsigned int num_chars = strlen(pText);

  // Do 2 passes:
  //   1. Find the size of the buffer
  //   2. Draw to the buffer
  for (int pass = 0; pass < 2; pass++) {

    if (pass == 1) {
      // Pro Pinball assumes RGBA
      size_t textureSize = POTWide * POTHigh * 4;

      // Allocate buffer and fill it with transparent pixels
      data = cpp_new(textureSize);
      memset(data, 0x00, textureSize);
    }

    // Offset for the current glyph
    unsigned int pen_x = 0;
    unsigned int pen_y = 0;

    // Loop over all glyphs
    for (unsigned int n = 0; n < num_chars; n++) {

      // Handle linebreaks
      if (pText[n] == '\n') {
        pen_x = 0;
        pen_y += line_height;
        continue;
      }

      // Load glyph image into the slot (erase previous one)
      error = FT_Load_Char(face, pText[n], FT_LOAD_RENDER);
      if (error) {
        //FIXME: Handle errors somehow?
        continue;
      }

      // Now, draw to our target surface
      FT_Bitmap *bm = &slot->bitmap;
      assert(bm->pixel_mode == FT_PIXEL_MODE_GRAY);
      assert(bm->num_grays == 256);
      uint8_t *src_data = bm->buffer;
      printf("glyph is %dx%d at %d, %d + %d, %d\n", bm->width, bm->rows, pen_x, pen_y, slot->bitmap_left, slot->bitmap_top);
      for (unsigned int y = 0; y < bm->rows; y++) {

        int ay = dy + pen_y + y + line_height - slot->bitmap_top;
        if ((ay < 0) || (ay >= POTHigh)) {
          bound_h = max(ay, bound_h);
          if (pass == 1) {
            continue;
          }
        }

        for (unsigned int x = 0; x < bm->width; x++) {

          int ax = dx + pen_x + x + slot->bitmap_left;
          if (pass == 0) {
            if (ax < 0) {
              ax = 0;
            }
          }
          
          // FIXME: This is a dirty hack for negative bitmap_left; we should move the entire space
          if ((ax < 0) || (ax >= POTWide)) {
            bound_w = max(ax, bound_w);
            continue;
          }

          // Copy glyph to our output
          if (pass == 1) {
            uint8_t gray = src_data[y * bm->pitch + x];
            for (unsigned int channel = 0; channel < 4; channel++) {
              data[(ay * POTWide + ax) * 4 + channel] = gray; // FIXME: Mix?
            }
          }
        }
      }

      // Increment pen position
      pen_x += slot->advance.x >> 6;
    }

    // In the first pass, we have to generate the size of the buffer and alignment offsets
    POTWide = bound_w; // pot(bound_w);
    POTHigh = bound_h; // pot(bound_h);
    if (eAlignMask == 0x13) {
      dx = (POTWide - bound_w) / 2;
      dy = POTHigh - bound_h;
    } else if (eAlignMask == 0x32) {
      dx = POTWide - bound_w;
      dy = (POTHigh - bound_h) / 2;
    } else {
      assert(false);
    }

    printf("Allocated %dx%d surface for %dx%d content\n", POTWide, POTHigh,
           bound_w, bound_h);
  }

  FT_Done_Face(face);

  _this->m_nHeight = (short)POTHigh;
  _this->m_nWidth = (short)POTWide;
  _this->m_nBitsPerComponent = 8; // info.bitsPerComponent;
  _this->m_bHasAlpha = true;      // info.hasAlpha;
  _this->m_bPreMulti = true;      // info.isPremultipliedAlpha;
  if (_this->m_pData) {
    cpp_delete(_this->m_pData);
  }
  _this->m_pData = data;

  return true;
}

__attribute__((constructor)) void inject(void) {
  if (*(unsigned long long *)0x10013fbb6 != 0x4589480005f739e8) {
    printf("Couldn't find Pro Pinball?!\n");
    return;
  }

  int error;
  error = FT_Init_FreeType(&library);
  if (error) {
    assert(false);
  }
  // FIXME: Do FT_Done_FreeType( library ); in destructor?
  
  // We also want gamepad input, even if our soon-to-be-created SDL window isn't in focus
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    
  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER|SDL_INIT_HAPTIC) != 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
  }

  // Some SDL features need a window, so we create a hidden one
  //FIXME: Repurpose for configuration?
  SDL_Window* window = SDL_CreateWindow(
    "Pro Pinball Super-Ultra",
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    10, 10,
    SDL_WINDOW_HIDDEN
  );

  // Open the first available controller
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      controller = SDL_GameControllerOpen(i);
      if (controller) {
        printf("Controller '%s' opened!\n", SDL_GameControllerName(controller));
        break;
      } else {
        fprintf(stderr, "Could not open gamecontroller %i: %s\n", i, SDL_GetError());
      }
    }
  }
  
  // Fix broken text rendering
  install_detour(CCImage_initWithString, CCImage_initWithString_hook);
  
  // Support gamepad
  memcpy(hw_input_state_original, hw_input_state, sizeof(hw_input_state_original));
  install_detour(hw_input_state, hw_input_state_hook);
  
  // Hook the nudge function, which also handles auto-shot
  install_detour(set_digital_nudge_acceleration_multipliers, set_digital_nudge_acceleration_multipliers_hook);

  // Shorter resume time when returning from menu
  //FIXME: Could also multiply the value at 0x100052572 to get the 3,2,1 back
  unprotect(__resume_timer_duration, 4);
  *__resume_timer_duration = 1.0f;
}
