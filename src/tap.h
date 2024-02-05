#include <ApplicationServices/ApplicationServices.h>
#include <stdint.h>
#include <sys/_types/_u_int.h>
#include <sys/types.h>
#include <time.h>

#define PRESSED_ARR_SIZE 512
#define KEYCODE_MOUSE_LEFT PRESSED_ARR_SIZE - 1
#define KEYCODE_MOUSE_RIGHT PRESSED_ARR_SIZE - 2
#define KEYCODE_MOUSE_OTHER PRESSED_ARR_SIZE - 3
#define KEYCODE_SCROLL_WHEEL PRESSED_ARR_SIZE - 4
#define KEYCODE_MOUSE_MOVE PRESSED_ARR_SIZE - 5

#define DISPLAY_ARR_SIZE 16
#define KEYBOARD_LAYOUTNAME_MAXSIZE 64
#define STAGED_EVENTS_ARR_SIZE 512
#define MM_PER_INCH 25.4;

#define ERR_TAP_DISABLED_USERINPUT 4225
#define ERR_TAP_DISABLED_TIMEOUT 4226
#define ERR_DISPLAY_NOTFOUND_FROM_POINT 4227

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

struct BaseEvent {
  CGEventType type;
  bool isBuiltinDisplay;
  bool isMainDisplay;
  char *_Nonnull processName;
  char *_Nonnull processPath;
};

struct KeyboardEvent {
  struct BaseEvent baseEvent;
  unsigned int timeDownMs;
  unsigned short keyCode;
  char *_Nonnull keyChar;
  char *_Nonnull keyboardLayout;
  bool repeated;
};

struct MouseClickEvent {
  struct BaseEvent baseEvent;
  unsigned int timeDownMs;
  CGPoint normalisedClickPoint;
  unsigned int dragDistancePx;
  float dragDistanceMm;
  float dragAngle;
  float dragVelocityKph;
};

struct MouseMoveEvent {
  struct BaseEvent baseEvent;
  unsigned int distancePx;
  float distanceMm;
  unsigned int angle;
  float velocityKph;
};

struct ScrollEvent {
  struct BaseEvent baseEvent;
  int deltaX;
  int deltaY;
  float angle;
  float velocityKph;
};

struct Display {
  CGRect bounds_points;
  CGSize size_mm;
  CGSize size_physical_px;
  bool isMain;
  bool isBuiltin;
};

struct PressedInfo {
  // The time when the key was pressed down.
  struct timespec pressed;

  // Used to keep track of where the mouse was when it was clicked down.
  CGPoint clickPoint;
};

struct CustomTapPayload {
  struct PressedInfo pressed[512];

  struct Display *_Nonnull displays;
  int displayCount;
};

extern void action_event_delegate(struct ActionEvent *_Nonnull c_struct);
extern void
pass_mouse_move_events(struct MouseMoveEvent (*)[STAGED_EVENTS_ARR_SIZE],
                       int mouseMoveEventCount, struct timespec);

extern void log_error(char *_Nonnull message);
extern void log_warn(char *_Nonnull message);
extern void log_info(char *_Nonnull message);
extern void log_debug(char *_Nonnull message);
extern void log_trace(char *_Nonnull message);

struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct CustomTapPayload *_Nonnull displayInfo, CGPoint point);

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
