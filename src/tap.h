#include <ApplicationServices/ApplicationServices.h>

#define PRESSED_ARR_SIZE 512
#define DISPLAY_ARR_SIZE 16

#define LEFT_MOUSE_KEYCODE PRESSED_ARR_SIZE - 1
#define RIGHT_MOUSE_KEYCODE PRESSED_ARR_SIZE - 2
#define OTHER_MOUSE_KEYCODE PRESSED_ARR_SIZE - 3

struct ActionEvent {
  long timeDown;
  CGEventType type;
  unsigned short keyCode;
  CGPoint normalisedClickPoint;
  unsigned int dragDistance;
  bool isBuiltinDisplay;
  bool isMainDisplay;
  char *processName;
  struct timespec functionStart;
};

struct Display {
  CGRect bounds;
  bool isMain;
  bool isBuiltin;
};

struct DisplaysInfo {
  struct Display *displays;
  int displayCount;
};

struct PressedInfo {
  // The time when the key was pressed down.
  struct timespec *pressed;

  // Used to keep track of where the mouse was when it was clicked down.
  CGPoint clickPoint;
};

struct UserInfo {
  struct PressedInfo *pressed;
  struct DisplaysInfo *displaysInfo;
};

extern void action_event_delegate(struct ActionEvent *c_struct);

struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct DisplaysInfo *displayInfo, CGPoint point);

bool eventTypeIsMouse(CGEventType type);
bool eventTypeIsLeftMouse(CGEventType type);
bool eventTypeIsRightMouse(CGEventType type);
bool eventTypeIsOtherMouse(CGEventType type);
