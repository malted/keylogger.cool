#include "tap.h"
#include "DisplayConfiguration.h"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <libproc.h>
#include <mach/clock_types.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type,
                            CGEventRef event, void *userInfo) {
  // For profiling
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);

  /* If you hold a key down, every time t (depending on your keyboard key
   * repetition configuation) macOS (quartz?) will insert a new key down
   * event, which we don't want to track. Luckily, there's a handy flag to
   * detect this! */
  if (CGEventGetDoubleValueField(event, kCGKeyboardEventAutorepeat))
    return event;

  if (type == kCGEventTapDisabledByUserInput) {
    printf("Event tap disabled by user input\n");
    exit(4225);
  } else if (type == kCGEventTapDisabledByTimeout) {
    printf("Event tap disabled by timeout\n");
    exit(4226);
  }

  struct UserInfo *passedInfo = (struct UserInfo *)userInfo;

  CGKeyCode keyCode =
      (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

  char layout[128];
  memset(layout, '\0', sizeof(layout));
  TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
  // get input source id - kTISPropertyInputSourceID
  // get layout name - kTISPropertyLocalizedName
  CFStringRef layoutID =
      TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
  CFStringGetCString(layoutID, layout, sizeof(layout), kCFStringEncodingUTF8);

  if (eventTypeIsLeftMouse(type)) {
    keyCode = KEYCODE_MOUSE_LEFT;
  } else if (eventTypeIsRightMouse(type)) {
    keyCode = KEYCODE_MOUSE_RIGHT;
  } else if (eventTypeIsOtherMouse(type)) {
    keyCode = KEYCODE_MOUSE_OTHER;
  } else if (eventTypeIsScrollWheel(type)) {
    keyCode = KEYCODE_SCROLL_WHEEL;
  } else if (eventTypeIsMouseMove(type)) {
    keyCode = KEYCODE_MOUSE_MOVE;
  }

  // Get the process' name.
  int pid = CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
  char processName[255] = {0};
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
  CGPoint locationLocal = {0};
  locationLocal.x = locationGlobal.x - display->bounds_points.origin.x;
  locationLocal.y = locationGlobal.y - display->bounds_points.origin.y;

  // TODO: Handle key down events here so we can exit early.

  // Click coords, normalised (local display space)
  CGPoint normalisedClickPoint = {0};
  normalisedClickPoint.x = locationLocal.x / display->bounds_points.size.width;
  normalisedClickPoint.y = locationLocal.y / display->bounds_points.size.height;

  // If it's an input up event, sent the event.
  // If it's anything else, record the time.
  // For reference, see CGEventTypes.h L102.
  if (type == kCGEventLeftMouseUp || type == kCGEventRightMouseUp ||
      type == kCGEventKeyUp || type == kCGEventOtherMouseUp ||
      type == kCGEventScrollWheel || type == kCGEventMouseMoved) {
    char *keyChar = keyCodeToChar(keyCode);

    int scrollDeltaX = 0, scrollDeltaY = 0;
    double scrollAngle = 0, scrollSpeed = 0, dragDistancePx = 0,
           dragDistanceMm = 0, dragAngle = 0, dragSpeed = 0,
           mouseDistancePx = 0, mouseDistanceMm = 0, mouseAngle = 0,
           mouseSpeed = 0;

    if (eventTypeIsScrollWheel(type)) {
      // https://developer.apple.com/documentation/coregraphics/cgeventfield/kcgscrollwheeleventpointdeltaaxis1
      // https://gist.github.com/svoisen/5215826
      scrollDeltaY = CGEventGetIntegerValueField(
          event, kCGScrollWheelEventPointDeltaAxis1);
      scrollDeltaX = CGEventGetIntegerValueField(
          event, kCGScrollWheelEventPointDeltaAxis2);

      double scrollDistancePoints =
          round(sqrt(pow(scrollDeltaX, 2) + pow(scrollDeltaY, 2)));
      double scrollDistanceMm = pointsToMm(scrollDistancePoints, *display);

      scrollAngle = atan2(scrollDeltaY, scrollDeltaX);

      if (passedInfo->pressed[keyCode].pressed->tv_sec != 0) {
        // mm / ms
        scrollSpeed = scrollDistanceMm /
                      diffTimespec(passedInfo->pressed[keyCode].pressed);

        // Convrt to km/h. This is so cursed.
        scrollSpeed *= 3.6;
      }
      clock_gettime(REALTIME_CLOCK, passedInfo->pressed[keyCode].pressed);
    } else if (eventTypeIsMouseClick(type)) {
      // Calculate the distance between the click point and the release point,
      // and round to nearest int
      double dragDistanceX =
          locationLocal.x - passedInfo->pressed[keyCode].clickPoint.x;
      double dragDistanceY =
          locationLocal.y - passedInfo->pressed[keyCode].clickPoint.y;

      dragAngle = atan2(dragDistanceY, dragDistanceX);

      double dragDistancePoints =
          round(sqrt(pow(dragDistanceX, 2) + pow(dragDistanceY, 2)));

      dragDistancePx = pointsToPx(dragDistancePoints, *display);
      dragDistanceMm = pointsToMm(dragDistancePoints, *display);

      if (passedInfo->pressed[keyCode].pressed->tv_sec != 0) {
        // km/h
        dragSpeed = dragDistanceMm /
                    diffTimespec(passedInfo->pressed[keyCode].pressed) * 3.6;
      }

      clock_gettime(REALTIME_CLOCK, passedInfo->pressed[keyCode].pressed);
    } else if (eventTypeIsMouseMove(type)) {
      double mouseDeltaX =
          CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
      double mouseDeltaY =
          CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);

      mouseAngle = atan2(mouseDeltaX, mouseDeltaY);

      double mouseDistancePoints =
          round(sqrt(pow(mouseDeltaX, 2) + pow(mouseDeltaY, 2)));

      mouseDistancePx = pointsToPx(mouseDistancePoints, *display);
      mouseDistanceMm = pointsToMm(mouseDistancePoints, *display);

      if (passedInfo->pressed[keyCode].pressed->tv_sec != 0) {
        // km/h
        long mouseSpeed = mouseDistanceMm /
                          diffTimespec(passedInfo->pressed[keyCode].pressed) *
                          3.6;

        printf("mouseSpeed: %ld\n", mouseSpeed);
      }
      clock_gettime(REALTIME_CLOCK, passedInfo->pressed[keyCode].pressed);
    }

    action_event_delegate(&(struct ActionEvent){
        .timeDown = diffTimespec(passedInfo->pressed[keyCode].pressed),
        .type = type,
        .keyCode = keyCode,
        .keyChar = keyChar,
        .normalisedClickPoint = normalisedClickPoint,
        .mouseDistancePx = mouseDistancePx,
        .mouseDistanceMm = mouseDistanceMm,
        .mouseAngle = mouseAngle,
        .mouseSpeedKph = mouseSpeed,
        .scrollDeltaX = scrollDeltaX,
        .scrollDeltaY = scrollDeltaY,
        .scrollAngle = scrollAngle,
        .scrollSpeedKph = scrollSpeed,
        .dragDistancePx = dragDistancePx,
        .dragDistanceMm = dragDistanceMm,
        .dragAngle = dragAngle,
        .dragSpeedKph = dragSpeed,
        .isBuiltinDisplay = display->isBuiltin,
        .isMainDisplay = display->isMain,
        .functionStart = start,
        .keyboardLayout = layout,
        .processName = processName,
    });

    free(keyChar);
    CFRelease(source);
  } else {
    if (eventTypeIsKeyboard(type) || eventTypeIsMouseClick(type)) {
      clock_gettime(CLOCK_REALTIME, passedInfo->pressed[keyCode].pressed);
    }

    // If it's a mouse down event, record the local click point.
    if (eventTypeIsMouseClick(type)) {
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
    if (point.x >= d[i].bounds_points.origin.x &&
        point.x <=
            d[i].bounds_points.origin.x + d[i].bounds_points.size.width &&
        point.y >= d[i].bounds_points.origin.y &&
        point.y <=
            d[i].bounds_points.origin.y + d[i].bounds_points.size.height) {
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
    // You had one job.
    printf("Failed to malloc displaysInfo->displays");
    return;
  }
  for (int i = 0; i < displayCount; i++) {
    /* Get the actual resolution of the display in physical pixels. This is
     * useful because many display functions in CoreGraphics return values in
     * points, which are not necessarily equal to pixels, for example Quartz's
     * CGDisplayPixelsWide is somewhat of a misnomer, as it returns points, not
     * pixels. Sure you could claim they are virtual pixels, but it's confusing
     * regardless (& undocumented!). Native pixels are physical; for example
     * my 13.6-inch M2 Air has a 2560-by-1664 native resolution, but in points
     * is 1470-by-960. */
    CGSize actual_px = {0};
    CFArrayRef allModes = CGDisplayCopyAllDisplayModes(displayIds[i], NULL);
    for (CFIndex i = 0; i < CFArrayGetCount(allModes); i++) {
      // Retrieve mode
      CGDisplayModeRef mode =
          (CGDisplayModeRef)CFArrayGetValueAtIndex(allModes, i);

      // Get the I/O Kit flags for this display mode.
      uint32_t ioFlags = CGDisplayModeGetIOFlags(mode);

      // https://developer.apple.com/documentation/iokit/1505967-anonymous/kdisplaymodenativeflag
      if (ioFlags & kDisplayModeNativeFlag) {
        /* In this case CGDisplayModeGetWidth and CGDisplayModeGetHeight would
         * still yield the correct result even though they are specified to
         * return points, as the mode with kDisplayModeNativeFlag set has a 1:1
         * point to pixel mapping. */
        actual_px.width = CGDisplayModeGetPixelWidth(mode);
        actual_px.height = CGDisplayModeGetPixelHeight(mode);
        break;
      }
    }
    CFRelease(allModes);

    // Check if actual_px was set, if not, fall back to points.
    if (actual_px.width == 0 || actual_px.height == 0) {
      actual_px.width = CGDisplayPixelsWide(displayIds[i]);
      actual_px.height = CGDisplayPixelsHigh(displayIds[i]);
    }

    displaysInfo->displays[i] = (struct Display){
        .bounds_points = CGDisplayBounds(displayIds[i]),
        .size_mm = CGDisplayScreenSize(displayIds[i]),
        .size_physical_px = actual_px,
        .isMain = CGDisplayIsMain(displayIds[i]),
        .isBuiltin = CGDisplayIsBuiltin(displayIds[i]),
    };
  }
}

void registerTap(void) {
  char *exampleSentence = "The quick brown fox jumps over the lazy dog.";
  nl(exampleSentence);

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

  // TODO: Include kCGEventMouseMoved
  eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) |
               (1 << kCGEventLeftMouseDown) | (1 << kCGEventRightMouseDown) |
               (1 << kCGEventLeftMouseUp) | (1 << kCGEventRightMouseUp) |
               (1 << kCGEventScrollWheel) | (1 << kCGEventMouseMoved));

  eventTap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, 0,
                              eventMask, eventTapCallback, &userInfo);

  // Run the callback function to initialise displays array, then register it.
  displayReconfigurationCallback(0, 0, &displaysInfo);
  CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback,
                                           &displaysInfo);
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

long diffTimespec(struct timespec *start) {
  struct timespec end;
  clock_gettime(CLOCK_REALTIME, &end);

  return (end.tv_sec - start->tv_sec) * 1000 +
         (end.tv_nsec - start->tv_nsec) / 1000000;
}

double pointsToPx(double points, struct Display display) {
  return points * display.size_physical_px.width /
         display.bounds_points.size.width;
}

double pointsToMm(double points, struct Display display) {
  return points * display.size_mm.width / display.bounds_points.size.width;
}

double hypot(double x, double y) { return sqrtf(powf(x, 2) + powf(y, 2)); }

bool eventTypeIsMouseClick(CGEventType type) {
  return type == kCGEventLeftMouseDown || type == kCGEventLeftMouseUp ||
         type == kCGEventRightMouseDown || type == kCGEventRightMouseUp ||
         type == kCGEventOtherMouseDown || type == kCGEventOtherMouseUp;
}
bool eventTypeIsMouseMove(CGEventType type) {
  return type == kCGEventMouseMoved;
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
bool eventTypeIsScrollWheel(CGEventType type) {
  return type == kCGEventScrollWheel;
}

bool eventTypeIsKeyboard(CGEventType type) {
  return type == kCGEventKeyDown || type == kCGEventKeyUp;
}

char *keyCodeToChar(CGKeyCode keyCode) {
  TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardInputSource();
  CFDataRef layoutData = TISGetInputSourceProperty(
      currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
  const UCKeyboardLayout *keyboardLayout =
      (const UCKeyboardLayout *)CFDataGetBytePtr(layoutData);

  UInt32 deadKeyState = 0;
  UniChar unicodeString[4] = {0};
  UniCharCount actualStringLength = 0;

  UCKeyTranslate(keyboardLayout, keyCode, kUCKeyActionDisplay, 0,
                 LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit, &deadKeyState,
                 sizeof(unicodeString) / sizeof(UniChar), &actualStringLength,
                 unicodeString);

  CFRelease(currentKeyboard);

  char *ret = malloc(sizeof(char) * 4);
  for (int i = 0; i < 4; i++) {
    ret[i] = unicodeString[i];
  }
  return ret;
}

// #endregion
