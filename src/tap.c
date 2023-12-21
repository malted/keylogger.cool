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

  // Click coords (global display space)
  CGPoint location = CGEventGetLocation(event);

  struct Display *display =
      manuallyGetDisplaysFromPoint(passedInfo->displayInfo, location);

  // Between 0-1, no matter the screen size.
  CGPoint normalisedClickPoint;
  if (display != NULL) {
    normalisedClickPoint.x =
        (location.x - display->bounds.origin.x) / display->bounds.size.width;
    normalisedClickPoint.y =
        (location.y - display->bounds.origin.y) / display->bounds.size.height;
  }

  // Get the process' name.
  int pid = CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
  char processName[255];
  int ret = proc_name(pid, processName, sizeof(processName));

  // If it's an input up event, sent the event.
  // If it's anything else, record the time.
  // For reference, see CGEventTypes.h L102.
  if (type == kCGEventLeftMouseUp || type == kCGEventRightMouseUp ||
      type == kCGEventKeyUp || type == kCGEventOtherMouseUp) {
    action_event_delegate(&(struct ActionEvent){
        .timeDown = diffTimespec(&passedInfo->pressed[keyCode]),
        .type = type,
        .keyCode = keyCode,
        .normalisedClickPoint = normalisedClickPoint,
        .isBuiltinDisplay = display->isBuiltin,
        .isMainDisplay = display->isMain,
        .functionStart = start,
        .processName = *processName,
    });
  } else {
    /* If you hold a key down, every time t (depending on your keyboard key
     * repetition configuation) macOS (quartz?) will insert a new key down
     * event, which we don't want to track. Luckily, there's a handy flag to
     * detect this!
     */
    if (!CGEventGetDoubleValueField(event, kCGKeyboardEventAutorepeat)) {
      clock_gettime(CLOCK_REALTIME, &passedInfo->pressed[keyCode]);
    }
  }

  return event;
}

// Faster than calling CGGetDisplaysWithPoint
struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct DisplayInfo *displayInfo, CGPoint point) {
  struct Display *d = displayInfo->displays;
  for (int i = 0; i < displayInfo->displayCount; i++) {
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
  struct DisplayInfo *displayInfo = (struct DisplayInfo *)userInfo;

  CGDirectDisplayID displayIds[DISPLAY_ARR_SIZE];
  uint32_t displayCount;
  CGError result =
      CGGetActiveDisplayList(DISPLAY_ARR_SIZE, displayIds, &displayCount);
  displayInfo->displayCount = displayCount;

  // Free old memory if it was previously allocated
  if (displayInfo->displays != NULL) {
    printf("Freeing old displayInfo->displays memory\n");
    free(displayInfo->displays);
  }
  // For each display, get the bounds and the name.
  displayInfo->displays = malloc(displayCount * sizeof(struct Display));
  if (displayInfo->displays == NULL) {
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
    printf("Failed to malloc displayInfo->displays");
    return;
  }
  for (int i = 0; i < displayCount; i++) {
    displayInfo->displays[i] = (struct Display){
        .bounds = CGDisplayBounds(displayIds[i]),
        .isMain = CGDisplayIsMain(displayIds[i]),
        .isBuiltin = CGDisplayIsBuiltin(displayIds[i]),
    };
  }
}

void registerTap(void) {
  struct timespec pressed[PRESSED_ARR_SIZE];
  struct DisplayInfo displayInfo = {
      .displays = NULL,
      .displayCount = 0,

  };
  struct UserInfo userInfo = {
      .pressed = pressed,
      .displayInfo = &displayInfo,
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
  displayReconfigurationCallback(0, 0, &displayInfo);
  CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback,
                                           &displayInfo);
  displayReconfigurationCallback(0, 0, &displayInfo);

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
                                         &displayInfo);
}
