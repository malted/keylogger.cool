#include <ApplicationServices/ApplicationServices.h>

#define PRESSED_ARR_SIZE 512
#define KEYCODE_MOUSE_LEFT PRESSED_ARR_SIZE - 1
#define KEYCODE_MOUSE_RIGHT PRESSED_ARR_SIZE - 2
#define KEYCODE_MOUSE_OTHER PRESSED_ARR_SIZE - 3
#define KEYCODE_SCROLL_WHEEL PRESSED_ARR_SIZE - 4
#define KEYCODE_MOUSE_MOVE PRESSED_ARR_SIZE - 5

#define DISPLAY_ARR_SIZE 16
#define MM_PER_INCH 25.4;

struct ActionEvent {
  long timeDown;
  CGEventType type;
  unsigned short keyCode;
  char *_Nonnull keyChar;
  CGPoint normalisedClickPoint;
  double mouseDistancePx;
  double mouseDistanceMm;
  double mouseAngle;
  double mouseSpeedKph;
  int scrollDeltaX;
  int scrollDeltaY;
  double scrollAngle;
  double scrollSpeedKph;
  double dragDistancePx;
  double dragDistanceMm;
  double dragAngle;
  double dragSpeedKph;
  bool isBuiltinDisplay;
  bool isMainDisplay;
  struct timespec functionStart;
  char *_Nonnull keyboardLayout;
  char *_Nonnull processName;
};

struct Display {
  CGRect bounds_points;
  CGSize size_mm;
  CGSize size_physical_px;
  bool isMain;
  bool isBuiltin;
};

struct DisplaysInfo {
  struct Display *_Nonnull displays;
  int displayCount;
};

struct PressedInfo {
  // The time when the key was pressed down.
  struct timespec *_Nonnull pressed;

  // Used to keep track of where the mouse was when it was clicked down.
  CGPoint clickPoint;
};

struct UserInfo {
  struct PressedInfo *_Nonnull pressed;
  struct DisplaysInfo *_Nonnull displaysInfo;
};

extern void action_event_delegate(struct ActionEvent *_Nonnull c_struct);

struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct DisplaysInfo *_Nonnull displayInfo, CGPoint point);

long diffTimespec(struct timespec *_Nonnull start);

double pointsToPx(double points, struct Display display);
double pointsToMm(double points, struct Display display);

bool eventTypeIsMouseClick(CGEventType type);
bool eventTypeIsMouseMove(CGEventType type);
bool eventTypeIsLeftMouse(CGEventType type);
bool eventTypeIsRightMouse(CGEventType type);
bool eventTypeIsOtherMouse(CGEventType type);
bool eventTypeIsScrollWheel(CGEventType type);
bool eventTypeIsKeyboard(CGEventType type);

char *_Nonnull keyCodeToChar(CGKeyCode keyCode);
