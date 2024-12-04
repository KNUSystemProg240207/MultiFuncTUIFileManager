#ifndef POPUP_WINDOW_H
#define POPUP_WINDOW_H

void initPopupWindow(void);
void showPopupWindow(const char *title);
void hidePopupWindow(void);
void delPopupWindow(void);

void updatePopupWindow(void);

void putCharToPopup(char ch);
void popCharFromPopup(void);
void getStringFromPopup(char *buffer);

#endif
