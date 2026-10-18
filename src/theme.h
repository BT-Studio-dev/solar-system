/* theme.h -- runtime visual themes for the shell and the viewport.
 *
 * The palette used to be a set of compile-time macros; it is now a table so the
 * app can switch between the Windows 7 Aero look, a light "Aero Day" chart, an
 * XP-era Luna scheme, an aurora palette and a high-contrast mode at runtime.
 * The familiar C_* names remain as accessors so drawing code stays readable.
 */
#ifndef SSS_THEME_H
#define SSS_THEME_H

#include "core.h"

typedef struct {
    const char *id;    /* stable id used in settings.ini */
    const char *name;  /* shown in menus */
    const char *note;  /* one-line description */
    int dark;          /* dark shell + dark viewport */

    /* Shell */
    SsColor window_top, window_bottom;
    SsColor titlebar_top, titlebar_bottom, titlebar_line, title_rgb, title_sub;
    SsColor titlebar_sheen;

    /* Panels */
    SsColor panel_top, panel_bottom, panel_border, panel_inner, header;
    SsColor text, text_dim, text_muted, text_faint;
    SsColor card_top, card_bottom, card_border;

    /* Accents */
    SsColor accent, accent_hi, accent_deep, green, amber, red, purple;

    /* Transport bar and buttons */
    SsColor transport_top, transport_bottom, transport_line, divider;
    SsColor btn_top, btn_bottom, btn_border;
    SsColor btn_top_hover, btn_bottom_hover, btn_border_hover;
    SsColor btn_top_down, btn_bottom_down;
    SsColor play_top, play_bottom, play_border;

    /* Capture glass (Win7 Aero close button) */
    SsColor close_hover, close_down;

    /* Viewport */
    SsColor scene_top, scene_bottom, nebula_a, nebula_b;
    SsColor star_cool, star_warm, glow;
    SsColor orbit, orbit_hot;
    SsColor label, label_muted, label_shadow;
    SsColor hud_title, hud_dim;
    SsColor chip_bg, chip_border, chip_text, chip_dim;
    int star_alpha;   /* scales the starfield's alpha */
    int vignette;     /* edge darkening strength */

    /* foreground colours that depend on the accents above */
    SsColor on_accent;   /* text/icons on an accent-filled button */
    SsColor play_icon;   /* play/pause glyph on the transport button */
    SsColor ghost;       /* flat button at rest */
    SsColor ghost_hot;
    SsColor ghost_down;
    SsColor track;       /* slider groove */
    SsColor knob;        /* slider thumb */
    SsColor overlay_top; /* floating pills and toasts over the scene */
    SsColor overlay_bottom;
    SsColor overlay_border;
    SsColor overlay_text;
    SsColor overlay_dim;
} SsTheme;

int sss_theme_count(void);
const SsTheme *sss_theme_at(int index);
const SsTheme *sss_theme(void);
int sss_theme_index(void);
void sss_theme_set(int index);
/* 1 when the id matches a theme (case-insensitive); does not switch. */
int sss_theme_find(const char *id);
/* Theme selection for hosts that do not use App (e.g. the installer wizard). */
int sss_theme_pref_load(const wchar_t *ini_path);
void sss_theme_pref_save(const wchar_t *ini_path, int index);

/* Shell */
#define C_WINDOW_TOP (sss_theme()->window_top)
#define C_WINDOW_BOTTOM (sss_theme()->window_bottom)
#define C_TITLEBAR_TOP (sss_theme()->titlebar_top)
#define C_TITLEBAR_BOTTOM (sss_theme()->titlebar_bottom)
#define C_TITLEBAR_LINE (sss_theme()->titlebar_line)
#define C_TITLE_RGB (sss_theme()->title_rgb)
#define C_TITLE_SUB (sss_theme()->title_sub)

/* Panels */
#define C_PANEL_TOP (sss_theme()->panel_top)
#define C_PANEL_BOTTOM (sss_theme()->panel_bottom)
#define C_PANEL_BORDER (sss_theme()->panel_border)
#define C_PANEL_INNER (sss_theme()->panel_inner)
#define C_HEADER (sss_theme()->header)
#define C_TEXT (sss_theme()->text)
#define C_TEXT_DIM (sss_theme()->text_dim)
#define C_TEXT_MUTED (sss_theme()->text_muted)
#define C_TEXT_FAINT (sss_theme()->text_faint)
#define C_CARD_TOP (sss_theme()->card_top)
#define C_CARD_BOTTOM (sss_theme()->card_bottom)
#define C_CARD_BORDER (sss_theme()->card_border)

/* Accents */
#define C_ACCENT (sss_theme()->accent)
#define C_ACCENT_HI (sss_theme()->accent_hi)
#define C_ACCENT_DEEP (sss_theme()->accent_deep)
#define C_GREEN (sss_theme()->green)
#define C_AMBER (sss_theme()->amber)
#define C_RED (sss_theme()->red)
#define C_PURPLE (sss_theme()->purple)

/* Transport bar and buttons */
#define C_TRANSPORT_TOP (sss_theme()->transport_top)
#define C_TRANSPORT_BOTTOM (sss_theme()->transport_bottom)
#define C_TRANSPORT_LINE (sss_theme()->transport_line)
#define C_DIVIDER (sss_theme()->divider)
#define C_BTN_TOP (sss_theme()->btn_top)
#define C_BTN_BOTTOM (sss_theme()->btn_bottom)
#define C_BTN_BORDER (sss_theme()->btn_border)
#define C_BTN_TOP_HOVER (sss_theme()->btn_top_hover)
#define C_BTN_BOTTOM_HOVER (sss_theme()->btn_bottom_hover)
#define C_BTN_BORDER_HOVER (sss_theme()->btn_border_hover)
#define C_BTN_TOP_DOWN (sss_theme()->btn_top_down)
#define C_BTN_BOTTOM_DOWN (sss_theme()->btn_bottom_down)
#define C_PLAY_TOP (sss_theme()->play_top)
#define C_PLAY_BOTTOM (sss_theme()->play_bottom)
#define C_PLAY_BORDER (sss_theme()->play_border)

/* Capture glass (Win7 Aero close button) */
#define C_CLOSE_HOVER (sss_theme()->close_hover)
#define C_CLOSE_DOWN (sss_theme()->close_down)

/* Geometry */
#define TITLEBAR_H 36.0f
#define TRANSPORT_H 52.0f
#define STATUS_H 26.0f
#define PANEL_W 316.0f
#define PAD 14.0f
#define RADIUS_PANEL 8.0f
#define RADIUS_BUTTON 5.0f
#define RADIUS_CHIP 13.0f

#endif
