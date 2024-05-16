#include "../ui.h"


void ui_valueScreen_screen_init(void)
{
ui_valueScreen = lv_obj_create(NULL);
lv_obj_clear_flag( ui_valueScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_valueScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_valueScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);



ui_profileName = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_profileName, 188);
lv_obj_set_height( ui_profileName, 23);
lv_obj_set_x( ui_profileName, 0 );
lv_obj_set_y( ui_profileName, 27 );
lv_obj_set_align( ui_profileName, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_profileName,LV_FLEX_FLOW_ROW_WRAP);
lv_obj_set_flex_align(ui_profileName, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_label_set_long_mode(ui_profileName,LV_LABEL_LONG_DOT);
lv_label_set_text(ui_profileName,"PAGE SCROLL");
lv_obj_set_style_text_color(ui_profileName, lv_color_hex(0xFF7D00), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileName, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileName, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profileName, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileName, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_profileDesc = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_profileDesc, 150);
lv_obj_set_height( ui_profileDesc, 39);
lv_obj_set_x( ui_profileDesc, -1 );
lv_obj_set_y( ui_profileDesc, -42 );
lv_obj_set_align( ui_profileDesc, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_profileDesc,"GOOGLE CHROME PAGE SCROLLING ");
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
lv_obj_set_x( ui_posIndicator, 1 );
lv_obj_set_y( ui_posIndicator, 76 );
lv_obj_set_align( ui_posIndicator, LV_ALIGN_TOP_MID );
lv_obj_clear_flag( ui_posIndicator, LV_OBJ_FLAG_SCROLLABLE );    /// Flags

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
lv_obj_set_style_text_color(ui_posindSha, lv_color_hex(0xAFAFAF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_posindSha, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_posindSha, &ui_font_SGdig60, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_Arc1 = lv_arc_create(ui_valueScreen);
lv_obj_set_width( ui_Arc1, lv_pct(96));
lv_obj_set_height( ui_Arc1, lv_pct(96));
lv_obj_set_align( ui_Arc1, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Arc1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE );    /// Flags
lv_arc_set_bg_angles(ui_Arc1,89,88);
lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0x282828), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_arc_opa(ui_Arc1, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_arc_width(ui_Arc1, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_arc_rounded(ui_Arc1, false, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_set_style_arc_color(ui_Arc1, lv_color_hex(0xFF7D00), LV_PART_INDICATOR | LV_STATE_DEFAULT );
lv_obj_set_style_arc_opa(ui_Arc1, 255, LV_PART_INDICATOR| LV_STATE_DEFAULT);
lv_obj_set_style_arc_width(ui_Arc1, 2, LV_PART_INDICATOR| LV_STATE_DEFAULT);
lv_obj_set_style_arc_rounded(ui_Arc1, false, LV_PART_INDICATOR| LV_STATE_DEFAULT);

lv_obj_set_style_radius(ui_Arc1, 12, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_Arc1, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_Arc1, 255, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_Arc1, lv_color_hex(0x000000), LV_PART_KNOB | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_Arc1, 255, LV_PART_KNOB| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_Arc1, 5, LV_PART_KNOB| LV_STATE_DEFAULT);


}
