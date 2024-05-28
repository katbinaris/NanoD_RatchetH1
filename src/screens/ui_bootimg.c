
#include "../ui.h"

void ui_bootimg_screen_init(void)
{
ui_bootimg = lv_obj_create(NULL);
lv_obj_clear_flag( ui_bootimg, LV_OBJ_FLAG_SCROLLABLE );    /// Flags
lv_obj_set_style_radius(ui_bootimg, 0, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(ui_bootimg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_bg_opa(ui_bootimg, 255, LV_PART_MAIN| LV_STATE_DEFAULT);

ui_bootMsg = lv_label_create(ui_bootimg);
lv_obj_set_width( ui_bootMsg, LV_SIZE_CONTENT);  /// 1
lv_obj_set_height( ui_bootMsg, LV_SIZE_CONTENT);   /// 1
lv_obj_set_align( ui_bootMsg, LV_ALIGN_CENTER );
lv_label_set_text(ui_bootMsg,"NANO_D++\nIS BOOTING...");
lv_obj_set_style_text_color(ui_bootMsg, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT );
lv_obj_set_style_text_opa(ui_bootMsg, 255, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_letter_space(ui_bootMsg, 2, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_line_space(ui_bootMsg, 3, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_align(ui_bootMsg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN| LV_STATE_DEFAULT);
lv_obj_set_style_text_font(ui_bootMsg, &ui_font_SGK100h16, LV_PART_MAIN| LV_STATE_DEFAULT);

lv_obj_add_event_cb(ui_bootimg, ui_event_bootimg, LV_EVENT_ALL, NULL);

}
