#include "tap.h"
#include "DisplayConfiguration.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <libproc.h>
#include <stdio.h>
#include <time.h>

long diffTimespec(struct timespec *start) {
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);

  return (end.tv_sec - start->tv_sec) * 1000 +
         (end.tv_nsec - start->tv_nsec) / 1000000;
}

CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type,
                            CGEventRef event, void *userInfo) {
  struct UserInfo *passedInfo = (struct UserInfo *)userInfo;

  // For profiling
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);

  CGKeyCode keyCode =
      (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

  if (eventTypeIsLeftMouse(type)) {
    keyCode = LEFT_MOUSE_KEYCODE;
  } else if (eventTypeIsRightMouse(type)) {
    keyCode = RIGHT_MOUSE_KEYCODE;
  } else if (eventTypeIsOtherMouse(type)) {
    keyCode = OTHER_MOUSE_KEYCODE;
  }

  // Get the process' name.
  int pid = CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
  char processName[255];
  int ret = proc_name(pid, processName, sizeof(processName));

  // Click coords (global display space)
  CGPoint locationGlobal = CGEventGetLocation(event);

  struct Display *display =
      manuallyGetDisplaysFromPoint(passedInfo->displaysInfo, locationGlobal);
  if (display == NULL) {
    printf("Couldn't find display for point x: %f, y: %f\n", locationGlobal.x,
           locationGlobal.y);
    return event;
  }

  // Click coords (local display space)
  CGPoint locationLocal;
  locationLocal.x = locationGlobal.x - display->bounds.origin.x;
  locationLocal.y = locationGlobal.y - display->bounds.origin.y;

  // Click coords, normalised (local display space)
  CGPoint normalisedClickPoint;
  normalisedClickPoint.x = locationLocal.x / display->bounds.size.width;
  normalisedClickPoint.y = locationLocal.y / display->bounds.size.height;

  // If it's an input up event, sent the event.
  // If it's anything else, record the time.
  // For reference, see CGEventTypes.h L102.
  if (type == kCGEventLeftMouseUp || type == kCGEventRightMouseUp ||
      type == kCGEventKeyUp || type == kCGEventOtherMouseUp) {

    double dragDistance = 0;
    if (eventTypeIsMouse(type)) {
      // Calculate the distance between the click point and the release point,
      // and round to nearest int
      dragDistance = round(sqrt(
          pow(passedInfo->pressed[keyCode].clickPoint.x - locationLocal.x, 2) +
          pow(passedInfo->pressed[keyCode].clickPoint.y - locationLocal.y, 2)));
    }

    action_event_delegate(&(struct ActionEvent){
        .timeDown = diffTimespec(passedInfo->pressed[keyCode].pressed),
        .type = type,
        .keyCode = keyCode,
        .normalisedClickPoint = normalisedClickPoint,
        .dragDistance = dragDistance,
        .isBuiltinDisplay = display->isBuiltin,
        .isMainDisplay = display->isMain,
        .functionStart = start,
        .processName = processName,
    });
  } else {
    /* If you hold a key down, every time t (depending on your keyboard key
     * repetition configuation) macOS (quartz?) will insert a new key down
     * event, which we don't want to track. Luckily, there's a handy flag to
     * detect this! */
    if (!CGEventGetDoubleValueField(event, kCGKeyboardEventAutorepeat)) {
      clock_gettime(CLOCK_REALTIME, passedInfo->pressed[keyCode].pressed);
    }

    // If it's a mouse down event, record the local click point.
    if (eventTypeIsMouse(type)) {
      passedInfo->pressed[keyCode].clickPoint = locationLocal;
    }
  }

  return event;
}

// Faster than calling CGGetDisplaysWithPoint
struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct DisplaysInfo *displaysInfo, CGPoint point) {
  struct Display *d = displaysInfo->displays;
  for (int i = 0; i < displaysInfo->displayCount; i++) {
    // Not using CGRectContainsPoint! This is faster.
    if (point.x >= d[i].bounds.origin.x &&
        point.x <= d[i].bounds.origin.x + d[i].bounds.size.width &&
        point.y >= d[i].bounds.origin.y &&
        point.y <= d[i].bounds.origin.y + d[i].bounds.size.height) {
      return &d[i];
    }
  }
  return NULL; // Oops...
}

void displayReconfigurationCallback(CGDirectDisplayID display,
                                    CGDisplayChangeSummaryFlags flags,
                                    void *userInfo) {
  struct DisplaysInfo *displaysInfo = (struct DisplaysInfo *)userInfo;

  CGDirectDisplayID displayIds[DISPLAY_ARR_SIZE];
  uint32_t displayCount;
  CGError result =
      CGGetActiveDisplayList(DISPLAY_ARR_SIZE, displayIds, &displayCount);
  displaysInfo->displayCount = displayCount;

  // Free old memory if it was previously allocated
  if (displaysInfo->displays != NULL) {
    printf("Freeing old displaysInfo->displays memory\n");
    free(displaysInfo->displays);
  }
  // For each display, get the bounds and the name.
  displaysInfo->displays = malloc(displayCount * sizeof(struct Display));
  if (displaysInfo->displays == NULL) {
    // wtf? What does one say to the imp inside one's computer when the imp
    // doesn't do the imp's job? If one's sole purpose is to be an Quartz Core
    // Graphics Application Service Event Tap imp, and one can not free
    // allocated memory, what does one become? Superflous? A burden? A mere
    // waste of clock cycles. An open letter to imps everywhere: know your
    // worth. It took one failed malloc to break the camel's back last time,
    // where the camel is me and its back is my back. I replaced the gremlins
    // with you. Are you worse than a gremlin? A critter? I didn't think so.
    // Allocate & free my heap memory, occasionally call into
    // ApplicationServices.h and don't speak unless spoken to again.
    printf("Failed to malloc displaysInfo->displays");
    return;
  }
  for (int i = 0; i < displayCount; i++) {
    displaysInfo->displays[i] = (struct Display){
        .bounds = CGDisplayBounds(displayIds[i]),
        .isMain = CGDisplayIsMain(displayIds[i]),
        .isBuiltin = CGDisplayIsBuiltin(displayIds[i]),
    };
  }
}

void registerTap(void) {
  struct PressedInfo pressed[PRESSED_ARR_SIZE];
  for (int i = 0; i < PRESSED_ARR_SIZE; i++) {
    pressed[i] = (struct PressedInfo){
        .pressed = &(struct timespec){0},
        .clickPoint = (struct CGPoint){0},
    };
  }

  struct DisplaysInfo displaysInfo = {
      .displays = NULL,
      .displayCount = 0,
  };
  struct UserInfo userInfo = {
      .pressed = pressed,
      .displaysInfo = &displaysInfo,
  };

  CFMachPortRef eventTap;
  CGEventMask eventMask;
  CFRunLoopSourceRef runLoopSource;

  eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) |
               (1 << kCGEventLeftMouseDown) | (1 << kCGEventRightMouseDown) |
               (1 << kCGEventLeftMouseUp) | (1 << kCGEventRightMouseUp));

  eventTap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, 0,
                              eventMask, eventTapCallback, &userInfo);
  // eventTap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, 0,
  // eventMask, myCGEventCallback, &pressed);

  // Run the callback function to initialise displays array, then register it.
  displayReconfigurationCallback(0, 0, &displaysInfo);
  CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback,
                                           &displaysInfo);
  displayReconfigurationCallback(0, 0, &displaysInfo);

  if (!eventTap) {
    printf("Failed to create event tap\n");
    return;
  }

  runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);
  CGEventTapEnable(eventTap, true);

  bool successfulNSApplicationLoad = WrappedNSApplicationLoad();
  if (!successfulNSApplicationLoad) {
    printf("Failed to load NSApplication\n");
    return;
  }

  CFRunLoopRun();

  CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback,
                                         &displaysInfo);
}

// #region Utils

bool eventTypeIsMouse(CGEventType type) {
  return type == kCGEventLeftMouseDown || type == kCGEventLeftMouseUp ||
         type == kCGEventRightMouseDown || type == kCGEventRightMouseUp ||
         type == kCGEventOtherMouseDown || type == kCGEventOtherMouseUp;
}
bool eventTypeIsLeftMouse(CGEventType type) {
  return type == kCGEventLeftMouseDown || type == kCGEventLeftMouseDragged ||
         type == kCGEventLeftMouseUp;
}
bool eventTypeIsRightMouse(CGEventType type) {
  return type == kCGEventRightMouseDown || type == kCGEventRightMouseDragged ||
         type == kCGEventRightMouseUp;
}
bool eventTypeIsOtherMouse(CGEventType type) {
  return type == kCGEventOtherMouseDown || type == kCGEventOtherMouseDragged ||
         type == kCGEventOtherMouseUp;
}

// #endregion
