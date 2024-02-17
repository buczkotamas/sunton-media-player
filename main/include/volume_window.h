#ifndef VOLUME_WINDOW_H
#define VOLUME_WINDOW_H

#include "ui.h"

void volume_window_show(int volume);

void loading_window_show_fmt(const char *fmt, ...);
void loading_window_show(const char* msg);
void loading_window_hide(void);

#endif