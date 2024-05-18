#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////

// SCREEN: ui_bootimg
void ui_bootimg_screen_init(void);
void ui_event_bootimg( lv_event_t * e);
lv_obj_t *ui_bootimg;
lv_obj_t *ui_bootMsg;

// SCREEN: ui_valueScreen
void ui_valueScreen_screen_init(void);
lv_obj_t *ui_valueScreen;
lv_obj_t *ui_dataScreen;
lv_obj_t *ui_posIndicator;
lv_obj_t *ui_posind;
lv_obj_t *ui_posindSha;
lv_obj_t *ui_profileName;
lv_obj_t *ui_profileDesc;
lv_obj_t *ui_Arc1;
lv_obj_t *ui_msgModal2;
lv_obj_t *ui_msgTitle2;
lv_obj_t *ui_msgDesc2;
lv_obj_t *ui_IdleCat;
lv_obj_t *ui_IdleCatShadow;




// SCREEN: ui_profSelectScreen
void ui_profSelectScreen_screen_init(void);
lv_obj_t *ui_profSelectScreen;
lv_obj_t *ui_selWrap;
lv_obj_t *ui_profList;
lv_obj_t *ui_profCount;
lv_obj_t *ui_pTitle;
lv_obj_t *ui_pCount;
lv_obj_t *ui_arrInd;
lv_obj_t *ui_arrReg;
lv_obj_t *ui_arrSha;


// SCREEN: ui_genericPopupScreen
void ui_genericPopupScreen_screen_init(void);
lv_obj_t *ui_genericPopupScreen;
lv_obj_t *ui_ContextOutter2;
lv_obj_t *ui_cTitlle3;
lv_obj_t *ui_cDesc3;
lv_obj_t *ui_cNav3;
lv_obj_t *ui____initial_actions0;

// SCREEN: ui_IdleAnim
// void ui_IdleAnim_screen_init(void);

// lv_obj_t *ui_IdleQuote;
// lv_obj_t *ui____initial_actions0;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////
// void showModal_Animation( lv_obj_t *TargetObject, int delay)
// {
// ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_malloc(sizeof(ui_anim_user_data_t));
// PropertyAnimation_0_user_data->target = TargetObject;
// PropertyAnimation_0_user_data->val = -1;
// lv_anim_t PropertyAnimation_0;
// lv_anim_init(&PropertyAnimation_0);
// lv_anim_set_time(&PropertyAnimation_0, 200);
// lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
// lv_anim_set_custom_exec_cb(&PropertyAnimation_0, _ui_anim_callback_set_height );
// lv_anim_set_values(&PropertyAnimation_0, 0, LV_SIZE_CONTENT );
// lv_anim_set_path_cb( &PropertyAnimation_0, lv_anim_path_ease_in);
// lv_anim_set_delay( &PropertyAnimation_0, delay + 0 );
// lv_anim_set_deleted_cb( &PropertyAnimation_0, _ui_anim_callback_free_user_data );
// lv_anim_set_playback_time(&PropertyAnimation_0, 0);
// lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
//  lv_anim_set_repeat_count(&PropertyAnimation_0, 0);
// lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
// lv_anim_set_early_apply( &PropertyAnimation_0, false );
// lv_anim_start(&PropertyAnimation_0);
// ui_anim_user_data_t *PropertyAnimation_1_user_data = lv_malloc(sizeof(ui_anim_user_data_t));
// PropertyAnimation_1_user_data->target = TargetObject;
// PropertyAnimation_1_user_data->val = -1;
// lv_anim_t PropertyAnimation_1;
// lv_anim_init(&PropertyAnimation_1);
// lv_anim_set_time(&PropertyAnimation_1, 200);
// lv_anim_set_user_data(&PropertyAnimation_1, PropertyAnimation_1_user_data);
// lv_anim_set_custom_exec_cb(&PropertyAnimation_1, _ui_anim_callback_set_width );
// lv_anim_set_values(&PropertyAnimation_1, 0, 170 );
// lv_anim_set_path_cb( &PropertyAnimation_1, lv_anim_path_ease_in);
// lv_anim_set_delay( &PropertyAnimation_1, delay + 0 );
// lv_anim_set_deleted_cb( &PropertyAnimation_1, _ui_anim_callback_free_user_data );
// lv_anim_set_playback_time(&PropertyAnimation_1, 0);
// lv_anim_set_playback_delay(&PropertyAnimation_1, 0);
//  lv_anim_set_repeat_count(&PropertyAnimation_1, 0);
// lv_anim_set_repeat_delay(&PropertyAnimation_1, 0);
// lv_anim_set_early_apply( &PropertyAnimation_1, false );
// lv_anim_start(&PropertyAnimation_1);
// }
///////////////////// FUNCTIONS ////////////////////
void ui_event_bootimg( lv_event_t * e) {
    lv_event_code_t event_code = lv_event_get_code(e);lv_obj_t * target = lv_event_get_target(e);
if ( event_code == LV_EVENT_SCREEN_LOADED) {
      _ui_screen_change( &ui_valueScreen, LV_SCR_LOAD_ANIM_FADE_OUT, 330, 3300, &ui_valueScreen_screen_init);
}
}

///////////////////// SCREENS ////////////////////

void ui_init( void )
{
lv_disp_t *dispp = lv_disp_get_default();
lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
lv_disp_set_theme(dispp, theme);
ui_bootimg_screen_init();
ui_valueScreen_screen_init();
ui_profSelectScreen_screen_init();
ui_genericPopupScreen_screen_init();
// ui_IdleAnim_screen_init();
ui____initial_actions0 = lv_obj_create(NULL);
lv_disp_load_scr( ui_bootimg);
}
