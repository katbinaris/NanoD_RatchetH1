#include "../ui.h"

void ui_genericPopupScreen_screen_init(void)
{
ui_genericPopupScreen = lv_obj_create(NULL);
lv_obj_remove_flag( ui_genericPopupScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_genericPopupScreen, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_genericPopupScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_genericPopupScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_ContextOutter2 = lv_obj_create(ui_genericPopupScreen);
lv_obj_remove_style_all(ui_ContextOutter2);
lv_obj_set_width( ui_ContextOutter2, lv_pct(70));
lv_obj_set_height( ui_ContextOutter2, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_ContextOutter2, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_ContextOutter2,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_ContextOutter2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
lv_obj_remove_flag( ui_ContextOutter2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_ContextOutter2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_ContextOutter2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_ContextOutter2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_ContextOutter2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_ContextOutter2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_ContextOutter2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_color(ui_ContextOutter2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_outline_opa(ui_ContextOutter2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_width(ui_ContextOutter2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_pad(ui_ContextOutter2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_width(ui_ContextOutter2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_spread(ui_ContextOutter2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_x(ui_ContextOutter2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_y(ui_ContextOutter2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_ContextOutter2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_ContextOutter2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_ContextOutter2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_ContextOutter2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cTitlle3 = lv_label_create(ui_ContextOutter2);
lv_obj_set_width( ui_cTitlle3, lv_pct(100));
lv_obj_set_height( ui_cTitlle3, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_cTitlle3, LV_ALIGN_CENTER );
lv_label_set_text(ui_cTitlle3,"GENERIC POP-UP ");

lv_obj_set_style_text_letter_space(ui_cTitlle3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cTitlle3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cTitlle3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cTitlle3, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cTitlle3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cTitlle3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cTitlle3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cTitlle3, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cDesc3 = lv_label_create(ui_ContextOutter2);
lv_obj_set_width( ui_cDesc3, lv_pct(100));
lv_obj_set_height( ui_cDesc3, LV_SIZE_CONTENT);   /// 40
lv_obj_set_align( ui_cDesc3, LV_ALIGN_CENTER );
lv_label_set_text(ui_cDesc3,"GENERIC DESCRIPTION FOR THE POP-UP / CONTEXT WINDOW THAT CAN BE AN ERROR OR NOTIFICATION OF CHANGES.");
lv_obj_set_style_text_letter_space(ui_cDesc3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cDesc3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cDesc3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cDesc3, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cDesc3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cDesc3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cDesc3, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cDesc3, 8, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cNav3 = lv_roller_create(ui_ContextOutter2);
lv_roller_set_options( ui_cNav3, "COARSE DETENTS\nFINE DETENTS\nFREE ROATION", LV_ROLLER_MODE_INFINITE );
lv_roller_set_selected( ui_cNav3, 2, LV_ANIM_OFF);
lv_obj_set_height( ui_cNav3, 64);
lv_obj_set_width( ui_cNav3, lv_pct(100));
lv_obj_set_align( ui_cNav3, LV_ALIGN_CENTER );
lv_obj_add_flag( ui_cNav3, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_remove_flag( ui_cNav3, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_text_letter_space(ui_cNav3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cNav3, 8, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cNav3, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_cNav3, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_cNav3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_cNav3, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_cNav3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_cNav3, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_cNav3, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

}
