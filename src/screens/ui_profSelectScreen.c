
#include "../ui.h"

void ui_profSelectScreen_screen_init(void)
{
ui_profSelectScreen = lv_obj_create(NULL);
lv_obj_clear_flag( ui_profSelectScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_profSelectScreen, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profSelectScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profSelectScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_selWrap = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_selWrap);
lv_obj_set_width( ui_selWrap, lv_pct(100));
lv_obj_set_height( ui_selWrap, lv_pct(81));
lv_obj_set_align( ui_selWrap, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_selWrap, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_profList = lv_roller_create(ui_selWrap);
lv_roller_set_options( ui_profList, "LOW PASS FILTER\nHIGH PASS FILTER\nOSCILLAOR 1\nOSCILLATOR 2\nLFO ", LV_ROLLER_MODE_NORMAL );
lv_roller_set_selected( ui_profList, 0, LV_ANIM_OFF);
lv_obj_set_width( ui_profList, 229);
lv_obj_set_height( ui_profList, 120);
lv_obj_set_x( ui_profList, 32 );
lv_obj_set_y( ui_profList, 0 );
lv_obj_set_align( ui_profList, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_profList, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_text_color(ui_profList, lv_color_hex(0x484848), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profList, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profList, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profList, 22, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profList, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_decor(ui_profList, LV_TEXT_DECOR_NONE, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profList, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profList, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profList, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_clip_corner(ui_profList, true, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_profList, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_text_color(ui_profList, lv_color_hex(0xFF7D00), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profList, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profList, 1, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profList, 22, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profList, LV_TEXT_ALIGN_AUTO, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profList, &ui_font_SG12H16, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profList, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profList, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);



ui_profCount = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_profCount);
lv_obj_set_width( ui_profCount, 100);
lv_obj_set_height( ui_profCount, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_profCount, 0 );
lv_obj_set_y( ui_profCount, 186 );
lv_obj_set_align( ui_profCount, LV_ALIGN_TOP_MID );
lv_obj_set_flex_flow(ui_profCount,LV_FLEX_FLOW_COLUMN_WRAP);
lv_obj_set_flex_align(ui_profCount, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
lv_obj_clear_flag( ui_profCount, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_border_color(ui_profCount, lv_color_hex(0x626262), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_profCount, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_profCount, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_side(ui_profCount, LV_BORDER_SIDE_TOP, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_profCount, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_profCount, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_profCount, 8, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_profCount, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_pTitle = lv_label_create(ui_profCount);
lv_obj_set_width( ui_pTitle, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_pTitle, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_pTitle, 0 );
lv_obj_set_y( ui_pTitle, 20 );
lv_obj_set_align( ui_pTitle, LV_ALIGN_TOP_MID );
lv_label_set_text(ui_pTitle,"PROFILES");
lv_obj_set_style_text_color(ui_pTitle, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_pTitle, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_pTitle, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_pTitle, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_pTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_pTitle, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_pCount = lv_label_create(ui_profCount);
lv_obj_set_width( ui_pCount, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_pCount, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_pCount, 0 );
lv_obj_set_y( ui_pCount, 34 );
lv_obj_set_align( ui_pCount, LV_ALIGN_TOP_MID );
lv_label_set_text(ui_pCount,"12/64");
lv_obj_set_style_text_color(ui_pCount, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_pCount, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_pCount, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_pCount, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_pCount, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_pCount, &ui_font_SGK100h32, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_arrInd = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_arrInd);
lv_obj_set_width( ui_arrInd, 26);
lv_obj_set_height( ui_arrInd, 26);
lv_obj_set_x( ui_arrInd, 12 );
lv_obj_set_y( ui_arrInd, -1 );
lv_obj_set_align( ui_arrInd, LV_ALIGN_LEFT_MID );
lv_obj_clear_flag( ui_arrInd, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_arrReg = lv_label_create(ui_arrInd);
lv_obj_set_width( ui_arrReg, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_arrReg, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_arrReg, LV_ALIGN_CENTER );
lv_label_set_text(ui_arrReg,"F");
lv_obj_set_style_text_color(ui_arrReg, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_arrReg, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_arrReg, &ui_font_idleArrow28, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_arrSha = lv_label_create(ui_arrInd);
lv_obj_set_width( ui_arrSha, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_arrSha, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_arrSha, LV_ALIGN_CENTER );
lv_label_set_text(ui_arrSha,"G");
lv_obj_set_style_text_color(ui_arrSha, lv_color_hex(0xACAEAC), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_arrSha, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_arrSha, &ui_font_idleArrow28, LV_PART_MAIN| LV_STATE_DEFAULT);


}
