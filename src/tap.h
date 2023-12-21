#include <ApplicationServices/ApplicationServices.h>

#define PRESSED_ARR_SIZE 300
#define DISPLAY_ARR_SIZE 16

struct ActionEvent {
  long timeDown;
  CGEventType type;
  short keyCode;
  CGPoint normalisedClickPoint;
  bool isBuiltinDisplay;
  bool isMainDisplay;
  struct timespec functionStart;
  char processName[255];
};

struct Display {
  CGRect bounds;
  bool isMain;
  bool isBuiltin;
};

struct DisplayInfo {
  struct Display *displays;
  int displayCount;
};

struct UserInfo {
  struct timespec *pressed;
  struct DisplayInfo *displayInfo;
};

extern void action_event_delegate(struct ActionEvent *c_struct);

struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct DisplayInfo *displayInfo, CGPoint point);
