#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////


// SCREEN: ui_valueScreen
void ui_valueScreen_screen_init(void);
lv_obj_t *ui_valueScreen;
lv_obj_t *ui_profileName;
lv_obj_t *ui_profileDesc;
lv_obj_t *ui_posIndicator;
lv_obj_t *ui_posind;
lv_obj_t *ui_posindSha;
lv_obj_t *ui_slider;
lv_obj_t *ui_fauxDivisions;
lv_obj_t *ui_actionAreaSm;
lv_obj_t *ui_pressLab3;
lv_obj_t *ui_keyShift2;
lv_obj_t *ui_contextArea2;
lv_obj_t *ui_contextWindow1;
lv_obj_t *ui_cTitlle2;
lv_obj_t *ui_cDesc2;
lv_obj_t *ui_cNav2;


// SCREEN: ui_profSelectScreen
void ui_profSelectScreen_screen_init(void);
lv_obj_t *ui_profSelectScreen;
lv_obj_t *ui_selectWrapper1;
lv_obj_t *ui_profileList1;
lv_obj_t *ui_gradTop1;
lv_obj_t *ui_gradBottom1;
lv_obj_t *ui_actionArea;
lv_obj_t *ui_actionName;
lv_obj_t *ui_lowerAreaDesc;
lv_obj_t *ui_actionCombo;
lv_obj_t *ui_pressLab;
lv_obj_t *ui_keyFirst;
lv_obj_t *ui_plusLab;
lv_obj_t *ui_keyLast;
lv_obj_t *ui_profileCounter;
lv_obj_t *ui_pTitle;
lv_obj_t *ui_pCount;
lv_obj_t *ui_contextArea;
lv_obj_t *ui_contextWindow2;
lv_obj_t *ui_cTitlle1;
lv_obj_t *ui_cDesc1;
lv_obj_t *ui_cNav1;


// SCREEN: ui_genericPopupScreen
void ui_genericPopupScreen_screen_init(void);
lv_obj_t *ui_genericPopupScreen;
lv_obj_t *ui_ContextOutter2;
lv_obj_t *ui_cTitlle3;
lv_obj_t *ui_cDesc3;
lv_obj_t *ui_cNav3;
lv_obj_t *ui____initial_actions0;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////

///////////////////// SCREENS ////////////////////

void ui_init( void )
{
lv_disp_t *dispp = lv_disp_get_default();
lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
lv_disp_set_theme(dispp, theme);
ui_valueScreen_screen_init();
ui_profSelectScreen_screen_init();
ui_genericPopupScreen_screen_init();
ui____initial_actions0 = lv_obj_create(NULL);
lv_disp_load_scr( ui_profSelectScreen);
}
