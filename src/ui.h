#pragma once

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

// SCREEN: ui_bootimg
void ui_bootimg_screen_init(void);
void ui_event_bootimg( lv_event_t * e);
extern lv_obj_t *ui_bootimg;
extern lv_obj_t *ui_bootMsg;

// SCREEN: ui_valueScreen
void ui_valueScreen_screen_init(void);
extern lv_obj_t *ui_valueScreen;
extern lv_obj_t *ui_Container2;
extern lv_obj_t *ui_profileName;
extern lv_obj_t *ui_profileDesc;
extern lv_obj_t *ui_posIndicator;
extern lv_obj_t *ui_posind;
extern lv_obj_t *ui_posindSha;
extern lv_obj_t *ui_Arc1;
// extern lv_obj_t *ui_actpos;
// extern lv_obj_t *ui_IdleAnim;
extern lv_obj_t *ui_IdleCat;
extern lv_obj_t *ui_IdleCatShadow; 

// SCREEN: ui_profSelectScreen
void ui_profSelectScreen_screen_init(void);
extern lv_obj_t *ui_profSelectScreen;
extern lv_obj_t *ui_selWrap;
extern lv_obj_t *ui_profList;
extern lv_obj_t *ui_profCount;
extern lv_obj_t *ui_pTitle;
extern lv_obj_t *ui_pCount;
extern lv_obj_t *ui_arrInd;
extern lv_obj_t *ui_arrReg;
extern lv_obj_t *ui_arrSha;
// SCREEN: ui_genericPopupScreen
void ui_genericPopupScreen_screen_init(void);
extern lv_obj_t *ui_genericPopupScreen;
extern lv_obj_t *ui_ContextOutter2;
extern lv_obj_t *ui_cTitlle3;
extern lv_obj_t *ui_cDesc3;
extern lv_obj_t *ui_cNav3;
extern lv_obj_t *ui____initial_actions0;
// SCREEN: ui_IdleAnim
// void ui_IdleAnim_screen_init(void);

// extern lv_obj_t *ui_IdleQuote;
// extern lv_obj_t *ui____initial_actions0;


LV_FONT_DECLARE( ui_font_SG12H16);
LV_FONT_DECLARE( ui_font_SG12h60);
LV_FONT_DECLARE( ui_font_SGK100h16);
LV_FONT_DECLARE( ui_font_SGK100h32);
LV_FONT_DECLARE( ui_font_SGdig60);
LV_FONT_DECLARE( ui_font_IdleCat);
LV_FONT_DECLARE( ui_font_idleArrow28);

void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
