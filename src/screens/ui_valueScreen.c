#include "../ui.h"

void ui_valueScreen_screen_init(void)
{
ui_valueScreen = lv_obj_create(NULL);
lv_obj_remove_flag( ui_valueScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_valueScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_valueScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_profileName = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_profileName, 220);
lv_obj_set_height( ui_profileName, 20);
lv_obj_set_x( ui_profileName, 0 );
lv_obj_set_y( ui_profileName, -12 );
lv_obj_set_align( ui_profileName, LV_ALIGN_CENTER );
lv_label_set_long_mode(ui_profileName,LV_LABEL_LONG_SCROLL_CIRCULAR);
lv_label_set_text(ui_profileName,"HIGH PASS FILTER KORG");
lv_obj_set_style_text_color(ui_profileName, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileName, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileName, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileName, &ui_font_SGK100h32, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_profileDesc = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_profileDesc, 150);
lv_obj_set_height( ui_profileDesc, 60);
lv_obj_set_x( ui_profileDesc, 0 );
lv_obj_set_y( ui_profileDesc, -28 );
lv_obj_set_align( ui_profileDesc, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_profileDesc,"ADJUST HIGH PASS FILTER ON OSCILLATOR 1");
lv_obj_set_style_text_color(ui_profileDesc, lv_color_hex(0x9D9D9D), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileDesc, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileDesc, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileDesc, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profileDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileDesc, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_posIndicator = lv_obj_create(ui_valueScreen);
lv_obj_remove_style_all(ui_posIndicator);
lv_obj_set_width( ui_posIndicator, LV_SIZE_CONTENT);  /// 100
lv_obj_set_height( ui_posIndicator, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_posIndicator, 3 );
lv_obj_set_y( ui_posIndicator, 38 );
lv_obj_set_align( ui_posIndicator, LV_ALIGN_TOP_MID );
lv_obj_remove_flag( ui_posIndicator, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_posind = lv_label_create(ui_posIndicator);
lv_obj_set_height( ui_posind, 48);
lv_obj_set_width( ui_posind, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_posind, LV_ALIGN_CENTER );
lv_label_set_text(ui_posind,"123");
lv_obj_set_style_text_color(ui_posind, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_posind, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_posind, &ui_font_SG12h60, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_posindSha = lv_label_create(ui_posIndicator);
lv_obj_set_height( ui_posindSha, 48);
lv_obj_set_width( ui_posindSha, LV_SIZE_CONTENT);  /// 1
lv_obj_set_align( ui_posindSha, LV_ALIGN_CENTER );
lv_label_set_text(ui_posindSha,"123");
lv_obj_set_style_text_color(ui_posindSha, lv_color_hex(0x9C9C9C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_posindSha, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_posindSha, &ui_font_SGdig60, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_slider = lv_slider_create(ui_valueScreen);
lv_slider_set_range(ui_slider, 0,127);
lv_slider_set_value( ui_slider, 12, LV_ANIM_OFF);
if (lv_slider_get_mode(ui_slider)==LV_SLIDER_MODE_RANGE ) lv_slider_set_left_value( ui_slider, 0, LV_ANIM_OFF);
lv_obj_set_width( ui_slider, 210);
lv_obj_set_height( ui_slider, 8);
lv_obj_set_x( ui_slider, 0 );
lv_obj_set_y( ui_slider, 16 );
lv_obj_set_align( ui_slider, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_slider, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_radius(ui_slider, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_slider, lv_color_hex(0x4C4C4C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_slider, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_radius(ui_slider, 0, LV_PART_INDICATOR| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_slider, lv_color_hex(0xFF8300), LV_PART_INDICATOR | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_slider, 255, LV_PART_INDICATOR| LV_STATE_DEFAULT);

lv_obj_set_style_radius(ui_slider, 0, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_slider, 255, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_slider, 0, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_slider, 0, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_slider, 0, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_slider, 0, LV_PART_KNOB| LV_STATE_DEFAULT);

ui_fauxDivisions = lv_label_create(ui_slider);
lv_obj_set_width( ui_fauxDivisions, LV_SIZE_CONTENT);  /// 150
lv_obj_set_height( ui_fauxDivisions, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_fauxDivisions, -1 );
lv_obj_set_y( ui_fauxDivisions, 0 );
lv_obj_set_align( ui_fauxDivisions, LV_ALIGN_CENTER );
lv_label_set_long_mode(ui_fauxDivisions,LV_LABEL_LONG_CLIP);
lv_label_set_text(ui_fauxDivisions,"IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII");
lv_obj_set_style_text_color(ui_fauxDivisions, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_fauxDivisions, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_fauxDivisions, -4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_fauxDivisions, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_fauxDivisions, &ui_font_SGK100h32, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_actionAreaSm = lv_obj_create(ui_valueScreen);
lv_obj_remove_style_all(ui_actionAreaSm);
lv_obj_set_width( ui_actionAreaSm, 130);
lv_obj_set_height( ui_actionAreaSm, 50);
lv_obj_set_align( ui_actionAreaSm, LV_ALIGN_BOTTOM_MID );
lv_obj_set_flex_flow(ui_actionAreaSm,LV_FLEX_FLOW_ROW_WRAP);
lv_obj_set_flex_align(ui_actionAreaSm, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
lv_obj_remove_flag( ui_actionAreaSm, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

ui_pressLab3 = lv_label_create(ui_actionAreaSm);
lv_obj_set_width( ui_pressLab3, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_pressLab3, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_pressLab3, 0 );
lv_obj_set_y( ui_pressLab3, -20 );
lv_obj_set_align( ui_pressLab3, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_pressLab3,"CHANGE MODE:");
lv_obj_set_style_text_color(ui_pressLab3, lv_color_hex(0x9C9C9C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_pressLab3, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_pressLab3, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_pressLab3, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_pressLab3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_pressLab3, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_pressLab3, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_pressLab3, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_pressLab3, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_pressLab3, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_keyShift2 = lv_label_create(ui_actionAreaSm);
lv_obj_set_width( ui_keyShift2, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_keyShift2, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_keyShift2, 0 );
lv_obj_set_y( ui_keyShift2, -20 );
lv_obj_set_align( ui_keyShift2, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_keyShift2,"SHIFT");
lv_obj_set_style_text_color(ui_keyShift2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_keyShift2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_keyShift2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_keyShift2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_keyShift2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_keyShift2, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_keyShift2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_keyShift2, lv_color_hex(0xCC6600), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_keyShift2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_keyShift2, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_keyShift2, 6, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_keyShift2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_keyShift2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_contextArea2 = lv_obj_create(ui_valueScreen);
lv_obj_remove_style_all(ui_contextArea2);
lv_obj_set_width( ui_contextArea2, lv_pct(100));
lv_obj_set_height( ui_contextArea2, lv_pct(100));
lv_obj_set_align( ui_contextArea2, LV_ALIGN_CENTER );
lv_obj_add_flag( ui_contextArea2, LV_OBJ_FLAG_HIDDEN );   /// Flags
lv_obj_remove_flag( ui_contextArea2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_style_bg_color(ui_contextArea2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_contextArea2, 127, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_contextWindow1 = lv_obj_create(ui_contextArea2);
lv_obj_remove_style_all(ui_contextWindow1);
lv_obj_set_width( ui_contextWindow1, lv_pct(70));
lv_obj_set_height( ui_contextWindow1, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_contextWindow1, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_contextWindow1,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_contextWindow1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
lv_obj_remove_flag( ui_contextWindow1, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_contextWindow1, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_contextWindow1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_contextWindow1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_contextWindow1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_contextWindow1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_contextWindow1, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_color(ui_contextWindow1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_outline_opa(ui_contextWindow1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_width(ui_contextWindow1, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_pad(ui_contextWindow1, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_width(ui_contextWindow1, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_spread(ui_contextWindow1, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_x(ui_contextWindow1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_y(ui_contextWindow1, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_contextWindow1, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_contextWindow1, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_contextWindow1, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_contextWindow1, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cTitlle2 = lv_label_create(ui_contextWindow1);
lv_obj_set_width( ui_cTitlle2, lv_pct(100));
lv_obj_set_height( ui_cTitlle2, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_cTitlle2, LV_ALIGN_CENTER );
lv_label_set_text(ui_cTitlle2,"CHANGE MODE");
lv_obj_set_style_text_letter_space(ui_cTitlle2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cTitlle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cTitlle2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cTitlle2, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cTitlle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cTitlle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cTitlle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cTitlle2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cDesc2 = lv_label_create(ui_contextWindow1);
lv_obj_set_width( ui_cDesc2, lv_pct(100));
lv_obj_set_height( ui_cDesc2, LV_SIZE_CONTENT);   /// 40
lv_obj_set_align( ui_cDesc2, LV_ALIGN_CENTER );
lv_label_set_text(ui_cDesc2,"SOME DESCRIPTION THAT IS NOT TOO LONG");
lv_obj_set_style_text_letter_space(ui_cDesc2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_cDesc2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cDesc2, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_cDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_cDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_cDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_cDesc2, 8, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_cNav2 = lv_roller_create(ui_contextWindow1);
lv_roller_set_options( ui_cNav2, "COARSE DETENTS\nFINE DETENTS\nFREE ROATION", LV_ROLLER_MODE_INFINITE );
lv_roller_set_selected( ui_cNav2, 2, LV_ANIM_OFF);
lv_obj_set_height( ui_cNav2, 64);
lv_obj_set_width( ui_cNav2, lv_pct(100));
lv_obj_set_align( ui_cNav2, LV_ALIGN_CENTER );
lv_obj_remove_flag( ui_cNav2, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_obj_set_style_text_letter_space(ui_cNav2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_cNav2, 8, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_cNav2, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_radius(ui_cNav2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_cNav2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_cNav2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_cNav2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_bg_color(ui_cNav2, lv_color_hex(0x000000), LV_PART_SELECTED | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_cNav2, 255, LV_PART_SELECTED| LV_STATE_DEFAULT);

}
