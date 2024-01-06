#include <ApplicationServices/ApplicationServices.h>

#define PRESSED_ARR_SIZE 512
#define KEYCODE_MOUSE_LEFT PRESSED_ARR_SIZE - 1
#define KEYCODE_MOUSE_RIGHT PRESSED_ARR_SIZE - 2
#define KEYCODE_MOUSE_OTHER PRESSED_ARR_SIZE - 3

#define DISPLAY_ARR_SIZE 16
#define MM_PER_INCH 25.4;

struct ActionEvent {
  long timeDown;
  CGEventType type;
  unsigned short keyCode;
  char *keyChar;
  CGPoint normalisedClickPoint;
  double mouseDistancePx;
  double mouseDistanceMm;
  int scrollDeltaX;
  int scrollDeltaY;
  double dragDistancePx;
  double dragDistanceMm;
  bool isBuiltinDisplay;
  bool isMainDisplay;
  char *processName;
  char *keyboardLayout;
  struct timespec functionStart;
};

struct Display {
  CGRect bounds_points;
  CGSize size_mm;
  CGSize size_physical_px;
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

long diffTimespec(struct timespec *start);

double pointsToPx(double points, struct Display display);
double pointsToMm(double points, struct Display display);

bool eventTypeIsMouseClick(CGEventType type);
bool eventTypeIsMouseMove(CGEventType type);
bool eventTypeIsLeftMouse(CGEventType type);
bool eventTypeIsRightMouse(CGEventType type);
bool eventTypeIsOtherMouse(CGEventType type);
bool eventTypeIsScrollWheel(CGEventType type);
bool eventTypeIsKeyboard(CGEventType type);

char *keyCodeToChar(CGKeyCode keyCode);
