#include "../ui.h"

void ui_profSelectScreen_screen_init(void)
{
ui_profSelectScreen = lv_obj_create(NULL);
lv_obj_remove_flag( ui_profSelectScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_profSelectScreen, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profSelectScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profSelectScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_selectWrapper1 = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_selectWrapper1);
lv_obj_set_width( ui_selectWrapper1, lv_pct(100));
lv_obj_set_height( ui_selectWrapper1, lv_pct(50));
lv_obj_set_align( ui_selectWrapper1, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_selectWrapper1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_profileList1 = lv_roller_create(ui_selectWrapper1);
lv_roller_set_options( ui_profileList1, "LOW PASS FILTER\nHIGH PASS FILTER\nOSCILLAOR 1\nOSCILLATOR 2\nLFO ", LV_ROLLER_MODE_INFINITE );
lv_roller_set_selected( ui_profileList1, 2, LV_ANIM_OFF);
lv_obj_set_width( ui_profileList1, 236);
lv_obj_set_height( ui_profileList1, 120);
lv_obj_set_align( ui_profileList1, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_profileList1, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_text_color(ui_profileList1, lv_color_hex(0x484848), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileList1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileList1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileList1, 22, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profileList1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileList1, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profileList1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profileList1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_clip_corner(ui_profileList1, true, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_profileList1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_text_color(ui_profileList1, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileList1, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileList1, 1, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileList1, 22, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileList1, &ui_font_SG12H16, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_profileList1, 8, LV_PART_SELECTED| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_profileList1, lv_color_hex(0xFF7F00), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_profileList1, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

ui_gradTop1 = lv_obj_create(ui_selectWrapper1);
lv_obj_remove_style_all(ui_gradTop1);
lv_obj_set_height( ui_gradTop1, 20);
lv_obj_set_width( ui_gradTop1, lv_pct(100));
lv_obj_set_x( ui_gradTop1, 0 );
lv_obj_set_y( ui_gradTop1, -50 );
lv_obj_set_align( ui_gradTop1, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_gradTop1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_gradTop1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_gradTop1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(ui_gradTop1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_main_stop(ui_gradTop1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_stop(ui_gradTop1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_dir(ui_gradTop1, LV_GRAD_DIR_VER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_blend_mode(ui_gradTop1, LV_BLEND_MODE_MULTIPLY, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_gradBottom1 = lv_obj_create(ui_selectWrapper1);
lv_obj_remove_style_all(ui_gradBottom1);
lv_obj_set_height( ui_gradBottom1, 30);
lv_obj_set_width( ui_gradBottom1, lv_pct(100));
lv_obj_set_x( ui_gradBottom1, 0 );
lv_obj_set_y( ui_gradBottom1, 50 );
lv_obj_set_align( ui_gradBottom1, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_gradBottom1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_gradBottom1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_gradBottom1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(ui_gradBottom1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_main_stop(ui_gradBottom1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_stop(ui_gradBottom1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_dir(ui_gradBottom1, LV_GRAD_DIR_VER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_blend_mode(ui_gradBottom1, LV_BLEND_MODE_MULTIPLY, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_actionArea = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_actionArea);
lv_obj_set_width( ui_actionArea, 134);
lv_obj_set_height( ui_actionArea, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_actionArea, 0 );
lv_obj_set_y( ui_actionArea, -22 );
lv_obj_set_align( ui_actionArea, LV_ALIGN_BOTTOM_MID );
lv_obj_set_flex_flow(ui_actionArea,LV_FLEX_FLOW_ROW_WRAP);
lv_obj_set_flex_align(ui_actionArea, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
lv_obj_remove_flag( ui_actionArea, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_actionName = lv_obj_create(ui_actionArea);
lv_obj_remove_style_all(ui_actionName);
lv_obj_set_width( ui_actionName, lv_pct(100));
lv_obj_set_height( ui_actionName, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_actionName, LV_ALIGN_TOP_MID );
lv_obj_set_flex_flow(ui_actionName,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_actionName, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
lv_obj_remove_flag( ui_actionName, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_style_pad_left(ui_actionName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_actionName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_actionName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_actionName, 2, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_lowerAreaDesc = lv_label_create(ui_actionName);
lv_obj_set_height( ui_lowerAreaDesc, 16);
lv_obj_set_width( ui_lowerAreaDesc, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_lowerAreaDesc, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_lowerAreaDesc,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_lowerAreaDesc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_label_set_text(ui_lowerAreaDesc,"SHOW DESCRIPTION");
lv_obj_set_style_text_color(ui_lowerAreaDesc, lv_color_hex(0x9C9C9C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_lowerAreaDesc, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_lowerAreaDesc, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_lowerAreaDesc, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_lowerAreaDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_lowerAreaDesc, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_lowerAreaDesc, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_lowerAreaDesc, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_lowerAreaDesc, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_lowerAreaDesc, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_actionCombo = lv_obj_create(ui_actionArea);
lv_obj_remove_style_all(ui_actionCombo);
lv_obj_set_width( ui_actionCombo, lv_pct(100));
lv_obj_set_height( ui_actionCombo, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_actionCombo, LV_ALIGN_TOP_MID );
lv_obj_set_flex_flow(ui_actionCombo,LV_FLEX_FLOW_ROW);
lv_obj_set_flex_align(ui_actionCombo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_obj_remove_flag( ui_actionCombo, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags

ui_pressLab = lv_label_create(ui_actionCombo);
lv_obj_set_width( ui_pressLab, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_pressLab, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_pressLab, 0 );
lv_obj_set_y( ui_pressLab, -20 );
lv_obj_set_align( ui_pressLab, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_pressLab,"PRESS");
lv_obj_set_style_text_color(ui_pressLab, lv_color_hex(0x9C9C9C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_pressLab, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_pressLab, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_pressLab, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_pressLab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_pressLab, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_pressLab, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_pressLab, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_pressLab, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_pressLab, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_keyFirst = lv_label_create(ui_actionCombo);
lv_obj_set_width( ui_keyFirst, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_keyFirst, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_keyFirst, 0 );
lv_obj_set_y( ui_keyFirst, -20 );
lv_obj_set_align( ui_keyFirst, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_keyFirst,"SHIFT");
lv_obj_set_style_text_color(ui_keyFirst, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_keyFirst, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_keyFirst, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_keyFirst, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_keyFirst, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_keyFirst, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_keyFirst, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_keyFirst, lv_color_hex(0xCC6600), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_keyFirst, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_keyFirst, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_keyFirst, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_keyFirst, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_keyFirst, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_plusLab = lv_label_create(ui_actionCombo);
lv_obj_set_width( ui_plusLab, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_plusLab, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_plusLab, 0 );
lv_obj_set_y( ui_plusLab, -20 );
lv_obj_set_align( ui_plusLab, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_plusLab,"+");
lv_obj_set_style_text_color(ui_plusLab, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_plusLab, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_plusLab, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_plusLab, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_plusLab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_plusLab, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_plusLab, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_plusLab, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_plusLab, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_plusLab, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_keyLast = lv_label_create(ui_actionCombo);
lv_obj_set_width( ui_keyLast, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_keyLast, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_keyLast, 0 );
lv_obj_set_y( ui_keyLast, -20 );
lv_obj_set_align( ui_keyLast, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_keyLast,"F1");
lv_obj_set_style_text_color(ui_keyLast, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_keyLast, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_keyLast, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_keyLast, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_keyLast, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_keyLast, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_keyLast, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_keyLast, lv_color_hex(0x949494), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_keyLast, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_keyLast, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_keyLast, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_keyLast, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_keyLast, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_profileCounter = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_profileCounter);
lv_obj_set_width( ui_profileCounter, 100);
lv_obj_set_height( ui_profileCounter, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_profileCounter, 0 );
lv_obj_set_y( ui_profileCounter, 22 );
lv_obj_set_align( ui_profileCounter, LV_ALIGN_TOP_MID );
lv_obj_set_flex_flow(ui_profileCounter,LV_FLEX_FLOW_COLUMN_WRAP);
lv_obj_set_flex_align(ui_profileCounter, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
lv_obj_remove_flag( ui_profileCounter, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_pTitle = lv_label_create(ui_profileCounter);
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

ui_pCount = lv_label_create(ui_profileCounter);
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

ui_contextArea = lv_obj_create(ui_profSelectScreen);
lv_obj_remove_style_all(ui_contextArea);
lv_obj_set_width( ui_contextArea, lv_pct(100));
lv_obj_set_height( ui_contextArea, lv_pct(100));
lv_obj_set_align( ui_contextArea, LV_ALIGN_CENTER );
lv_obj_add_flag( ui_contextArea, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_remove_flag( ui_contextArea, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_style_bg_color(ui_contextArea, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_contextArea, 127, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_contextWindow2 = lv_obj_create(ui_contextArea);
lv_obj_remove_style_all(ui_contextWindow2);
lv_obj_set_width( ui_contextWindow2, lv_pct(70));
lv_obj_set_height( ui_contextWindow2, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_contextWindow2, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_contextWindow2,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_contextWindow2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
lv_obj_remove_flag( ui_contextWindow2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_contextWindow2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_contextWindow2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_contextWindow2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_contextWindow2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_contextWindow2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_contextWindow2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_color(ui_contextWindow2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_outline_opa(ui_contextWindow2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_width(ui_contextWindow2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_pad(ui_contextWindow2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_width(ui_contextWindow2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_spread(ui_contextWindow2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_x(ui_contextWindow2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_y(ui_contextWindow2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_contextWindow2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_contextWindow2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_contextWindow2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_contextWindow2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cTitlle1 = lv_label_create(ui_contextWindow2);
lv_obj_set_width( ui_cTitlle1, lv_pct(100));
lv_obj_set_height( ui_cTitlle1, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_cTitlle1, LV_ALIGN_CENTER );
lv_label_set_text(ui_cTitlle1,"DESCRIPTION");
lv_obj_set_style_text_letter_space(ui_cTitlle1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cTitlle1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cTitlle1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cTitlle1, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cTitlle1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cTitlle1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cTitlle1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cTitlle1, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cDesc1 = lv_label_create(ui_contextWindow2);
lv_obj_set_width( ui_cDesc1, lv_pct(100));
lv_obj_set_height( ui_cDesc1, LV_SIZE_CONTENT);   /// 40
lv_obj_set_align( ui_cDesc1, LV_ALIGN_CENTER );
lv_label_set_text(ui_cDesc1,"ADJUST HIGH PASS FILTER ON OSCILLATOR 1");
lv_obj_set_style_text_letter_space(ui_cDesc1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cDesc1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cDesc1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cDesc1, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cDesc1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cDesc1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cDesc1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cDesc1, 8, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cNav1 = lv_roller_create(ui_contextWindow2);
lv_roller_set_options( ui_cNav1, "COARSE DETENTS\nFINE DETENTS\nFREE ROATION", LV_ROLLER_MODE_INFINITE );
lv_roller_set_selected( ui_cNav1, 2, LV_ANIM_OFF);
lv_obj_set_height( ui_cNav1, 64);
lv_obj_set_width( ui_cNav1, lv_pct(100));
lv_obj_set_align( ui_cNav1, LV_ALIGN_CENTER );
lv_obj_add_flag( ui_cNav1, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_remove_flag( ui_cNav1, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_text_letter_space(ui_cNav1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cNav1, 8, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cNav1, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_cNav1, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_cNav1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_cNav1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_cNav1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_cNav1, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_cNav1, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

}
