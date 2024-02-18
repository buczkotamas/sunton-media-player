#include "gui.h"

static lv_style_t btnmatrix_bg;
static lv_style_t btnmatrix_btn;

lv_style_t *gui_style_btnmatrix_main(void)
{
    if (btnmatrix_bg.prop_cnt == 0)
    {
        lv_style_init(&btnmatrix_bg);
        lv_style_set_pad_all(&btnmatrix_bg, 0);
        lv_style_set_pad_gap(&btnmatrix_bg, 0);
        lv_style_set_clip_corner(&btnmatrix_bg, true);
        lv_style_set_border_color(&btnmatrix_bg, lv_palette_main(LV_PALETTE_GREY));
        lv_style_set_border_width(&btnmatrix_bg, 1);
        lv_style_set_radius(&btnmatrix_bg, LV_RADIUS_CIRCLE);
        lv_style_set_text_font(&btnmatrix_bg, UI_FONT_L);
    }
    return &btnmatrix_bg;
}

lv_style_t *gui_style_btnmatrix_items(void)
{
    if (btnmatrix_btn.prop_cnt == 0)
    {
        lv_style_init(&btnmatrix_btn);
        lv_style_set_radius(&btnmatrix_btn, 0);
        lv_style_set_border_width(&btnmatrix_btn, 1);
        lv_style_set_border_opa(&btnmatrix_btn, LV_OPA_50);
        lv_style_set_border_color(&btnmatrix_btn, lv_palette_main(LV_PALETTE_GREY));
        lv_style_set_border_side(&btnmatrix_btn, LV_BORDER_SIDE_INTERNAL);
        lv_style_set_radius(&btnmatrix_btn, 0);
    }
    return &btnmatrix_btn;
}