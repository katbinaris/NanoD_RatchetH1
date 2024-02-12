#ifndef _SQUARELINE_PROJECT_UI_H
#define _SQUARELINE_PROJECT_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined __has_include
  #if __has_include("lvgl.h")
    #include "lvgl.h"
  #elif __has_include("lvgl/lvgl.h")
    #include "lvgl/lvgl.h"
  #else
    #include "lvgl.h"
  #endif
#else
  #include "lvgl.h"
#endif

#include "ui_helpers.h"
#include "ui_events.h"
// SCREEN: ui_valueScreen
void ui_valueScreen_screen_init(void);
extern lv_obj_t *ui_valueScreen;
extern lv_obj_t *ui_profileName;
extern lv_obj_t *ui_profileDesc;
extern lv_obj_t *ui_posIndicator;
extern lv_obj_t *ui_posind;
extern lv_obj_t *ui_posindSha;
extern lv_obj_t *ui_slider;
extern lv_obj_t *ui_fauxDivisions;
extern lv_obj_t *ui_actionAreaSm;
extern lv_obj_t *ui_pressLab3;
extern lv_obj_t *ui_keyShift2;
extern lv_obj_t *ui_contextArea2;
extern lv_obj_t *ui_contextWindow1;
extern lv_obj_t *ui_cTitlle2;
extern lv_obj_t *ui_cDesc2;
extern lv_obj_t *ui_cNav2;
// SCREEN: ui_profSelectScreen
void ui_profSelectScreen_screen_init(void);
extern lv_obj_t *ui_profSelectScreen;
extern lv_obj_t *ui_selectWrapper1;
extern lv_obj_t *ui_profileList1;
extern lv_obj_t *ui_gradTop1;
extern lv_obj_t *ui_gradBottom1;
extern lv_obj_t *ui_actionArea;
extern lv_obj_t *ui_actionName;
extern lv_obj_t *ui_lowerAreaDesc;
extern lv_obj_t *ui_actionCombo;
extern lv_obj_t *ui_pressLab;
extern lv_obj_t *ui_keyFirst;
extern lv_obj_t *ui_plusLab;
extern lv_obj_t *ui_keyLast;
extern lv_obj_t *ui_profileCounter;
extern lv_obj_t *ui_pTitle;
extern lv_obj_t *ui_pCount;
extern lv_obj_t *ui_contextArea;
extern lv_obj_t *ui_contextWindow2;
extern lv_obj_t *ui_cTitlle1;
extern lv_obj_t *ui_cDesc1;
extern lv_obj_t *ui_cNav1;
// SCREEN: ui_genericPopupScreen
void ui_genericPopupScreen_screen_init(void);
extern lv_obj_t *ui_genericPopupScreen;
extern lv_obj_t *ui_ContextOutter2;
extern lv_obj_t *ui_cTitlle3;
extern lv_obj_t *ui_cDesc3;
extern lv_obj_t *ui_cNav3;
extern lv_obj_t *ui____initial_actions0;



LV_FONT_DECLARE( ui_font_SG12H16);
LV_FONT_DECLARE( ui_font_SG12h60);
LV_FONT_DECLARE( ui_font_SGK100h16);
LV_FONT_DECLARE( ui_font_SGK100h32);
LV_FONT_DECLARE( ui_font_SGdig60);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
