#ifndef MSG_WINDOW_H
#define MSG_WINDOW_H

#include "gui.h"

/**
 * @brief Show modal message window
 *
 * @param[in] modal         is it modal window or not
 * @param[in] bar           progress bar value, if bar < 0 then it's hidden
 * @param[in] ok            OK button, shown only if ok == true
 * @param[in] cancel        Cancel button, shown only if cancel == true
 * @param[in] hide_in_ms    Close automatically if hide_in_ms > 0 in hide_in_ms ms.
 * @param[in] fmt           Message format string
 */
void msg_window_show(bool modal, int bar, bool ok, bool cancel, int hide_in_ms, const char *fmt, ...);
void msg_window_show_volume(int volume);
void msg_window_show_text(const char *fmt, ...);
void msg_window_show_ok(const char *fmt, ...);
void msg_window_hide(void);

#endif