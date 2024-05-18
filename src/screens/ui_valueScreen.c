#include "../ui.h"


void ui_valueScreen_screen_init(void)
{
ui_valueScreen = lv_obj_create(NULL);
lv_obj_clear_flag( ui_valueScreen, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_bg_color(ui_valueScreen, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_valueScreen, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_dataScreen = lv_obj_create(ui_valueScreen);
lv_obj_remove_style_all(ui_dataScreen);
lv_obj_set_width( ui_dataScreen, lv_pct(100));
lv_obj_set_height( ui_dataScreen, lv_pct(50));
lv_obj_set_align( ui_dataScreen, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_dataScreen,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_dataScreen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
lv_obj_clear_flag( ui_dataScreen, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_scrollbar_mode(ui_dataScreen, LV_SCROLLBAR_MODE_OFF);
lv_obj_set_style_pad_row(ui_dataScreen, 8, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_column(ui_dataScreen, 0, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_posIndicator = lv_obj_create(ui_dataScreen);
lv_obj_remove_style_all(ui_posIndicator);
lv_obj_set_width( ui_posIndicator, LV_SIZE_CONTENT);  /// 100
lv_obj_set_height( ui_posIndicator, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_posIndicator, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_posIndicator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_scrollbar_mode(ui_posIndicator, LV_SCROLLBAR_MODE_OFF);
lv_obj_set_style_pad_left(ui_posIndicator, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_posIndicator, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_posIndicator, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_posIndicator, 5, LV_PART_MAIN| LV_STATE_DEFAULT);

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

ui_profileName = lv_label_create(ui_dataScreen);
lv_obj_set_width( ui_profileName, lv_pct(78));
lv_obj_set_height( ui_profileName, LV_SIZE_CONTENT);   /// 1
lv_obj_set_x( ui_profileName, 0 );
lv_obj_set_y( ui_profileName, 41 );
lv_obj_set_align( ui_profileName, LV_ALIGN_CENTER );
lv_obj_set_flex_flow(ui_profileName,LV_FLEX_FLOW_ROW_WRAP);
lv_obj_set_flex_align(ui_profileName, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
lv_label_set_long_mode(ui_profileName,LV_LABEL_LONG_DOT);
lv_label_set_text(ui_profileName,"???");
lv_obj_clear_flag( ui_profileName, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_style_text_color(ui_profileName, lv_color_hex(0xFF7D00), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileName, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileName, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileName, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profileName, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileName, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_add_flag( ui_profileName, LV_OBJ_FLAG_HIDDEN );   // Hide Default

ui_profileDesc = lv_label_create(ui_dataScreen);
lv_obj_set_width( ui_profileDesc, 150);
lv_obj_set_height( ui_profileDesc, LV_SIZE_CONTENT);   /// 39
lv_obj_set_x( ui_profileDesc, -1 );
lv_obj_set_y( ui_profileDesc, -42 );
lv_obj_set_align( ui_profileDesc, LV_ALIGN_BOTTOM_MID );
lv_label_set_text(ui_profileDesc,"???");
lv_obj_clear_flag( ui_profileDesc, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_style_text_color(ui_profileDesc, lv_color_hex(0x9D9D9D), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_profileDesc, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_profileDesc, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_profileDesc, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_profileDesc, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_profileDesc, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_add_flag( ui_profileDesc, LV_OBJ_FLAG_HIDDEN );   // Hide Default

ui_Arc1 = lv_arc_create(ui_valueScreen);
lv_obj_set_width( ui_Arc1, lv_pct(98));
lv_obj_set_height( ui_Arc1, lv_pct(98));
lv_obj_set_align( ui_Arc1, LV_ALIGN_CENTER );
lv_obj_clear_flag( ui_Arc1, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN );    /// Flags
lv_obj_set_scrollbar_mode(ui_Arc1, LV_SCROLLBAR_MODE_OFF);
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

/* 
    Message modal
*/

ui_msgModal2 = lv_obj_create(ui_valueScreen);
lv_obj_remove_style_all(ui_msgModal2);
lv_obj_set_width( ui_msgModal2, 170);
lv_obj_set_height( ui_msgModal2, LV_SIZE_CONTENT);   /// 50
lv_obj_set_align( ui_msgModal2, LV_ALIGN_CENTER );
lv_obj_add_flag( ui_msgModal2, LV_OBJ_FLAG_HIDDEN);
lv_obj_set_flex_flow(ui_msgModal2,LV_FLEX_FLOW_COLUMN);
lv_obj_set_flex_align(ui_msgModal2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
lv_obj_clear_flag( ui_msgModal2, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_msgModal2, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_msgModal2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_msgModal2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_color(ui_msgModal2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_border_opa(ui_msgModal2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_border_width(ui_msgModal2, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_color(ui_msgModal2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_outline_opa(ui_msgModal2, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_width(ui_msgModal2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_outline_pad(ui_msgModal2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_width(ui_msgModal2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_spread(ui_msgModal2, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_x(ui_msgModal2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_shadow_ofs_y(ui_msgModal2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_msgModal2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_msgModal2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_msgModal2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_msgModal2, 12, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_msgTitle2 = lv_label_create(ui_msgModal2);
lv_obj_set_width( ui_msgTitle2, 170);
lv_obj_set_height( ui_msgTitle2, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_msgTitle2, LV_ALIGN_CENTER );
lv_label_set_text(ui_msgTitle2,"MESSAGE RECEIVED");
lv_obj_set_style_text_letter_space(ui_msgTitle2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_msgTitle2, 4, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_msgTitle2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_msgTitle2, &ui_font_SG12H16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_msgTitle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_msgTitle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_msgTitle2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_msgTitle2, 6, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_msgDesc2 = lv_label_create(ui_msgModal2);
lv_obj_set_width( ui_msgDesc2, 170);
lv_obj_set_height( ui_msgDesc2, LV_SIZE_CONTENT);   /// 40
lv_obj_set_align( ui_msgDesc2, LV_ALIGN_CENTER );
lv_label_set_text(ui_msgDesc2,"I'VE JUST RECEIVE A LOVELY MESSAGE FROM RICHARD. THANK YOU RICHARD!");
lv_obj_set_style_text_letter_space(ui_msgDesc2, 1, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_msgDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_msgDesc2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_msgDesc2, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_left(ui_msgDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_right(ui_msgDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_top(ui_msgDesc2, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_pad_bottom(ui_msgDesc2, 8, LV_PART_MAIN| LV_STATE_DEFAULT);

/* 
    Idle Overlay
*/

ui_IdleCat = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_IdleCat, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_IdleCat, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_IdleCat, LV_ALIGN_CENTER );
lv_label_set_text(ui_IdleCat,"A");
lv_obj_set_style_text_color(ui_IdleCat, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_IdleCat, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_IdleCat, &ui_font_IdleCat, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_add_flag( ui_IdleCat, LV_OBJ_FLAG_HIDDEN );   // Hide Default

ui_IdleCatShadow = lv_label_create(ui_valueScreen);
lv_obj_set_width( ui_IdleCatShadow, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_IdleCatShadow, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_IdleCatShadow, LV_ALIGN_CENTER );
lv_label_set_text(ui_IdleCatShadow,"E");
lv_obj_set_style_text_color(ui_IdleCatShadow, lv_color_hex(0x9C9C9C), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_IdleCatShadow, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_IdleCatShadow, &ui_font_IdleCat, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_add_flag( ui_IdleCatShadow, LV_OBJ_FLAG_HIDDEN );   // Hide Default

}
