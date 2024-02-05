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
#include <stdlib.h>
#include <time.h>

void updateActuationTime(struct CustomTapPayload *passedInfo, int keyCode) {
  clock_gettime(REALTIME_CLOCK, &passedInfo->pressed[keyCode].pressed);
}

int isFirstEvent(struct CustomTapPayload *passedInfo, int keyCode) {
  return passedInfo->pressed[keyCode].pressed.tv_nsec == 0;
}

void sendMouseMoveEvents(
    struct MouseMoveEvent (*_Nonnull mouseMoveEvents)[STAGED_EVENTS_ARR_SIZE],
    int *_Nonnull mouseMoveEventCount, struct timespec start,
    char *_Nonnull lastEventProcessPath, char *_Nonnull processPath) {

  char dbg_msg[64];
  snprintf(dbg_msg, 64, "sending %d mouse moves", *mouseMoveEventCount);
  log_debug(dbg_msg);

  pass_mouse_move_events(mouseMoveEvents, *mouseMoveEventCount, start);
  *mouseMoveEventCount = 0;
  strcpy(lastEventProcessPath, processPath);
}

// These are global because the Rust delegate might want to request them
// early.
struct KeyboardEvent keyboardEvents[STAGED_EVENTS_ARR_SIZE];
struct MouseClickEvent mouseClickEvents[STAGED_EVENTS_ARR_SIZE];
struct MouseMoveEvent mouseMoveEvents[STAGED_EVENTS_ARR_SIZE];
struct ScrollEvent scrollEvents[STAGED_EVENTS_ARR_SIZE];
int keyboardEventCount = 0, mouseClickEventCount = 0, mouseMoveEventCount = 0,
    scrollEventCount = 0;

float initialMouseMoveAngleAtan2Offset;

char lastEventProcessPath[PROC_PIDPATHINFO_MAXSIZE];

CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type,
                            CGEventRef event, void *userInfo) {
  // For profiling
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);

  (void)proxy; // I'm not using this.

  // If the event tap is disabled, we should exit.
  if (type == kCGEventTapDisabledByUserInput) {
    printf("Event tap disabled by user input\n");
    exit(ERR_TAP_DISABLED_USERINPUT);
  } else if (type == kCGEventTapDisabledByTimeout) {
    printf("Event tap disabled by timeout\n");
    exit(ERR_TAP_DISABLED_TIMEOUT);
  }

  struct CustomTapPayload *_Nonnull passedInfo =
      (struct CustomTapPayload *)userInfo;

  CGPoint locationGlobal =
      CGEventGetLocation(event); // Current mouse coords (global display space)
  struct Display *display =
      manuallyGetDisplaysFromPoint(passedInfo, locationGlobal);
  if (display == NULL) {
    printf("Couldn't find display for point x: %f, y: %f\n", locationGlobal.x,
           locationGlobal.y);
    exit(ERR_DISPLAY_NOTFOUND_FROM_POINT);
    return event;
  }

  char layout[KEYBOARD_LAYOUTNAME_MAXSIZE];
  memset(layout, '\0', sizeof(layout));
  TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
  // get input source id - kTISPropertyInputSourceID
  // get layout name - kTISPropertyLocalizedName
  CFStringRef layoutID =
      TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
  CFStringGetCString(layoutID, layout, sizeof(layout), kCFStringEncodingUTF8);

  // Get the process' name & executable path.
  int pid = CGEventGetIntegerValueField(event, kCGEventTargetUnixProcessID);
  char processName[255];
  char processPath[PROC_PIDPATHINFO_MAXSIZE];
  proc_name(pid, processName, sizeof(processName));
  proc_pidpath(pid, processPath, sizeof(processPath));
  if (lastEventProcessPath[0] == '\0')
    strcpy(lastEventProcessPath, processPath);

  struct BaseEvent baseEvent = {
      .type = type,
      .isBuiltinDisplay = display->isBuiltin,
      .isMainDisplay = display->isMain,
      .processName = processName,
      .processPath = processPath,
  };

  if (eventTypeIsKeyboard(type)) {
    if (mouseMoveEventCount > 1) {
      sendMouseMoveEvents(&mouseMoveEvents, &mouseMoveEventCount, start,
                          lastEventProcessPath, processPath);
    }

    // CGKeyCode keyCode =
    //     (CGKeyCode)CGEventGetIntegerValueField(event,
    //     kCGKeyboardEventKeycode);

    // keyboardEvents[keyboardEventCount++] =
    //     (struct KeyboardEvent){.baseEvent = baseEvent,
    //                            .keyCode = keyCode,
    //                            .keyChar = keyCodeToChar(keyCode),
    //                            .keyboardLayout = layout,
    //                            .repeated = CGEventGetDoubleValueField(
    //                                event, kCGKeyboardEventAutorepeat)};

    // if (keyboardEventCount > STAGED_EVENTS_ARR_SIZE) {
    //   log_debug("SENDING KEYBOARD EVENTS TO RUST DELEGATE");
    // }

    return event;
  } else if (eventTypeIsMouseMove(type)) {
    int mouseDeltaX = CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
    int mouseDeltaY = CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);

    if (isFirstEvent(passedInfo, KEYCODE_MOUSE_MOVE)) {
      log_debug("First event; skipping");
      updateActuationTime(passedInfo, KEYCODE_MOUSE_MOVE);
      strcpy(lastEventProcessPath, processPath);
      return event;
    }

    double mouseDistancePoints =
        round(sqrt(pow(mouseDeltaX, 2) + pow(mouseDeltaY, 2)));
    float mouseDistanceMm = pointsToMm(mouseDistancePoints, *display);

    long timeSinceLastActuation =
        diffTimespec(&passedInfo->pressed[KEYCODE_MOUSE_MOVE].pressed);
    updateActuationTime(passedInfo, KEYCODE_MOUSE_MOVE);
    if (timeSinceLastActuation <= 0) {
      strcpy(lastEventProcessPath, processPath);
      return event;
    }

    if (timeSinceLastActuation > 500 && mouseMoveEventCount > 1) {
      sendMouseMoveEvents(&mouseMoveEvents, &mouseMoveEventCount, start,
                          lastEventProcessPath, processPath);
      return event;
    }

    float angle_diff = 0;
    if (mouseDistancePoints >= 5.0) {
      float velocityKph = mouseDistanceMm / timeSinceLastActuation * 3.6;

      /* The atan2 function returns a value in the range of -π to π. (0 at (1,
       * 0), boundary at (-1, 0)). Since I want an angular difference, without
       * this jump, I'm rotating the delta coords by the angle of the first
       * event, so the atan2 calculation always starts from (1, 0). This way,
       * the angle will be continuous. The original values will be stored in the
       * struct. I know it's usually atan2(y, x). Always using (x, y) here
       * simplified the rotation logic. I just have to rotate (in reality
       * offset and mod) to rectify it at the end. */

      // angle_f = angle_f > 0 ? angle_f : 2 * M_PI + angle_f;
      // int angle = fmod(angle_f * 180 / M_PI + 90, 360);
      // initialMouseMoveAngleAtan2Offset

      float raw_angle = atan2(mouseDeltaX, mouseDeltaY);
      if (mouseMoveEventCount == 0) {
        initialMouseMoveAngleAtan2Offset = raw_angle;
      } else {
        int rotatedMouseDeltaX =
            mouseDeltaX * cos(initialMouseMoveAngleAtan2Offset) -
            mouseDeltaY * sin(initialMouseMoveAngleAtan2Offset);
        int rotatedMouseDeltaY =
            mouseDeltaX * sin(initialMouseMoveAngleAtan2Offset) +
            mouseDeltaY * cos(initialMouseMoveAngleAtan2Offset);
        angle_diff = fabs(atan2(rotatedMouseDeltaX, rotatedMouseDeltaY));
      }
      char dbg_msg[64];
      snprintf(dbg_msg, 64, "mousemov angles  raw: %f diff: %f init: %f",
               raw_angle, angle_diff, initialMouseMoveAngleAtan2Offset);
      log_trace(dbg_msg);

      // printf("\t\t%d\t%d %d\t %ld\t%lf\t%lf\t%lf\n",
      // (int)mouseDistancePoints,
      //        mouseDeltaX, mouseDeltaY, timeSinceLastActuation,
      //        mouseMoveEvents[0].angle, angle, angle_diff);

      // printf("arst %d %d   %d %d\t%d %d %f\n", mouseDeltaX, mouseDeltaY,
      //        rotatedMouseDeltaX, rotatedMouseDeltaY, angle,
      //        mouseMoveEvents[0].angle, angle_diff);

      mouseMoveEvents[mouseMoveEventCount++] = (struct MouseMoveEvent){
          .baseEvent = baseEvent,
          .distancePx = pointsToPx(mouseDistancePoints, *display),
          .distanceMm = mouseDistanceMm,
          .angle = 0,
          .velocityKph = velocityKph};
    }

    // if (mouseMoveEventCount > STAGED_EVENTS_ARR_SIZE ||
    //     ((angle_diff >= M_PI / 2 ||
    //       !strcmp(processPath, lastEventProcessPath)) &&
    //      mouseMoveEventCount > 1)) {
    if ((mouseMoveEventCount > STAGED_EVENTS_ARR_SIZE ||
         angle_diff >= M_PI / 2) &&
        mouseMoveEventCount > 1) {
      sendMouseMoveEvents(&mouseMoveEvents, &mouseMoveEventCount, start,
                          lastEventProcessPath, processPath);
    }
  }
  return event;

  // int keyCode;
  // if (eventTypeIsLeftMouse(type)) {
  //   keyCode = KEYCODE_MOUSE_LEFT;
  // } else if (eventTypeIsRightMouse(type)) {
  //   keyCode = KEYCODE_MOUSE_RIGHT;
  // } else if (eventTypeIsOtherMouse(type)) {
  //   keyCode = KEYCODE_MOUSE_OTHER;
  // } else if (eventTypeIsScrollWheel(type)) {
  //   keyCode = KEYCODE_SCROLL_WHEEL;
  // } else if (eventTypeIsMouseMove(type)) {
  //   keyCode = KEYCODE_MOUSE_MOVE;
  // }

  // // Click coords (local display space)
  // CGPoint locationLocal = {0};
  // locationLocal.x = locationGlobal.x - display->bounds_points.origin.x;
  // locationLocal.y = locationGlobal.y - display->bounds_points.origin.y;

  // // TODO: Handle key down events here so we can exit early.

  // // Click coords, normalised (local display space)
  // CGPoint normalisedClickPoint = {0};
  // normalisedClickPoint.x = locationLocal.x /
  // display->bounds_points.size.width; normalisedClickPoint.y = locationLocal.y
  // / display->bounds_points.size.height;

  // // If it's an input up event, sent the event.
  // // If it's anything else, record the time.
  // // For reference, see CGEventTypes.h L102.
  // if (type == kCGEventLeftMouseUp || type == kCGEventRightMouseUp ||
  //     type == kCGEventKeyUp || type == kCGEventOtherMouseUp ||
  //     type == kCGEventScrollWheel || type == kCGEventMouseMoved) {
  //   char *keyChar = keyCodeToChar(keyCode);

  //   int scrollDeltaX = 0, scrollDeltaY = 0;
  //   double scrollAngle = 0, scrollSpeed = 0, dragDistancePx = 0,
  //          dragDistanceMm = 0, dragAngle = 0, dragSpeed = 0,
  //          mouseDistancePx = 0, mouseDistanceMm = 0, mouseAngle = 0,
  //          mouseSpeed = 0;

  //   if (eventTypeIsScrollWheel(type)) {
  //     //
  //     https://developer.apple.com/documentation/coregraphics/cgeventfield/kcgscrollwheeleventpointdeltaaxis1
  //     // https://gist.github.com/svoisen/5215826
  //     scrollDeltaY = CGEventGetIntegerValueField(
  //         event, kCGScrollWheelEventPointDeltaAxis1);
  //     scrollDeltaX = CGEventGetIntegerValueField(
  //         event, kCGScrollWheelEventPointDeltaAxis2);

  //     double scrollDistancePoints =
  //         round(sqrt(pow(scrollDeltaX, 2) + pow(scrollDeltaY, 2)));
  //     double scrollDistanceMm = pointsToMm(scrollDistancePoints, *display);

  //     scrollAngle = atan2(scrollDeltaY, scrollDeltaX);

  //     if (passedInfo->pressed[keyCode].pressed.tv_sec != 0) {
  //       // mm / ms
  //       scrollSpeed = scrollDistanceMm /
  //                     diffTimespec(&passedInfo->pressed[keyCode].pressed);

  //       // Convrt to km/h. This is so cursed.
  //       scrollSpeed *= 3.6;
  //     }
  //     updateActuationTime(passedInfo, keyCode);
  //   } else if (eventTypeIsMouseClick(type)) {
  //     // Calculate the distance between the click point and the release
  //     point,
  //     // and round to nearest int
  //     double dragDistanceX =
  //         locationLocal.x - passedInfo->pressed[keyCode].clickPoint.x;
  //     double dragDistanceY =
  //         locationLocal.y - passedInfo->pressed[keyCode].clickPoint.y;

  //     dragAngle = atan2(dragDistanceY, dragDistanceX);

  //     double dragDistancePoints =
  //         round(sqrt(pow(dragDistanceX, 2) + pow(dragDistanceY, 2)));

  //     dragDistancePx = pointsToPx(dragDistancePoints, *display);
  //     dragDistanceMm = pointsToMm(dragDistancePoints, *display);

  //     if (passedInfo->pressed[keyCode].pressed.tv_sec != 0) {
  //       // km/h
  //       dragSpeed = dragDistanceMm /
  //                   diffTimespec(&passedInfo->pressed[keyCode].pressed)
  //                   * 3.6;
  //     }

  //     updateActuationTime(passedInfo, keyCode);
  //   } else if (eventTypeIsMouseMove(type)) {
  //     double mouseDeltaX =
  //         CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
  //     double mouseDeltaY =
  //         CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);

  //     mouseAngle = atan2(mouseDeltaY, mouseDeltaX);

  //     double mouseDistancePoints =
  //         round(sqrt(pow(mouseDeltaX, 2) + pow(mouseDeltaY, 2)));

  //     mouseDistancePx = pointsToPx(mouseDistancePoints, *display);
  //     mouseDistanceMm = pointsToMm(mouseDistancePoints, *display);

  //     if (passedInfo->pressed[keyCode].pressed.tv_sec != 0) {
  //       // km/h
  //       mouseSpeed = mouseDistanceMm /
  //                    diffTimespec(&passedInfo->pressed[keyCode].pressed)
  //                    * 3.6;
  //     }
  //     updateActuationTime(passedInfo, keyCode);
  //   }

  // action_event_delegate(&(struct ActionEvent){
  //     .timeDown = diffTimespec(&passedInfo->pressed[keyCode].pressed),
  //     .type = type,
  //     .keyCode = keyCode,
  //     .keyChar = keyChar,
  //     .normalisedClickPoint = normalisedClickPoint,
  //     .mouseDistancePx = mouseDistancePx,
  //     .mouseDistanceMm = mouseDistanceMm,
  //     .mouseAngle = mouseAngle,
  //     .mouseSpeedKph = mouseSpeed,
  //     .scrollDeltaX = scrollDeltaX,
  //     .scrollDeltaY = scrollDeltaY,
  //     .scrollAngle = scrollAngle,
  //     .scrollSpeedKph = scrollSpeed,
  //     .dragDistancePx = dragDistancePx,
  //     .dragDistanceMm = dragDistanceMm,
  //     .dragAngle = dragAngle,
  //     .dragSpeedKph = dragSpeed,
  //     .isBuiltinDisplay = display->isBuiltin,
  //     .isMainDisplay = display->isMain,
  //     .functionStart = start,
  //     .keyboardLayout = layout,
  //     .processName = processName,
  // });

  // free(keyChar);
  // CFRelease(source);
  // }
  // else {
  //   if (eventTypeIsKeyboard(type) || eventTypeIsMouseClick(type)) {
  //     updateActuationTime(passedInfo, keyCode);
  //   }

  //   // If it's a mouse down event, record the local click point.
  //   if (eventTypeIsMouseClick(type)) {
  //     passedInfo->pressed[keyCode].clickPoint = locationLocal;
  //   }
  // }

  // return event;
}

// Faster than calling CGGetDisplaysWithPoint
struct Display *_Nullable manuallyGetDisplaysFromPoint(
    struct CustomTapPayload *_Nonnull customTapPayload, CGPoint point) {
  struct Display *d = customTapPayload->displays;
  for (int i = 0; i < customTapPayload->displayCount; i++) {
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

// void displayReconfigurationCallback(
//     CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags,
//     struct CustomTapPayload *_Nonnull userInfo) {
void displayReconfigurationCallback(unsigned int display, unsigned int flags,
                                    void *_Nonnull userInfoRaw) {
  (void)display, (void)flags;
  struct CustomTapPayload *userInfo = (struct CustomTapPayload *)userInfoRaw;

  CGDirectDisplayID displayIds[DISPLAY_ARR_SIZE];
  uint32_t displayCount;
  if (CGGetActiveDisplayList(DISPLAY_ARR_SIZE, displayIds, &displayCount) !=
      kCGErrorSuccess) {
    printf("Failed to get active display list\n");
    exit(4229);
    return;
  }
  userInfo->displayCount = displayCount;

  // Free old memory if it was previously allocated
  if (userInfo->displays != NULL) {
    free(userInfo->displays);
  }
  // For each display, get the bounds and the name.
  userInfo->displays = malloc(displayCount * sizeof(struct Display));
  if (userInfo->displays == NULL) {
    // You had one job.
    printf("Failed to malloc displaysInfo->displays");
    exit(4230);
    return;
  }
  for (unsigned int i = 0; i < displayCount; i++) {
    /* Get the actual resolution of the display in physical pixels. This is
     * useful because many display functions in CoreGraphics return values in
     * points, which are not necessarily equal to pixels, for example Quartz's
     * CGDisplayPixelsWide is somewhat of a misnomer, as it returns points,
     * not pixels. Sure you could claim they are virtual pixels, but it's
     * confusing regardless (& undocumented!). Native pixels are physical; for
     * example my 13.6-inch M2 Air has a 2560-by-1664 native resolution, but
     * in points is 1470-by-960. */
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
         * return points, as the mode with kDisplayModeNativeFlag set has a
         * 1:1 point to pixel mapping. */
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

    userInfo->displays[i] = (struct Display){
        .bounds_points = CGDisplayBounds(displayIds[i]),
        .size_mm = CGDisplayScreenSize(displayIds[i]),
        .size_physical_px = actual_px,
        .isMain = CGDisplayIsMain(displayIds[i]),
        .isBuiltin = CGDisplayIsBuiltin(displayIds[i]),
    };
  }
}

int registerTap(void) {
  char *exampleSentence = "The quick brown fox jumps over the lazy dog.";
  nl(exampleSentence);

  struct PressedInfo pressed[PRESSED_ARR_SIZE];
  for (int i = 0; i < PRESSED_ARR_SIZE; i++) {
    pressed[i] = (struct PressedInfo){
        .pressed = (struct timespec){0},
        .clickPoint = (struct CGPoint){0},
    };
  }

  struct CustomTapPayload tapPayload = {
      .pressed = pressed,
      .displays = NULL,
      .displayCount = 0,
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
                              eventMask, eventTapCallback, &tapPayload);

  // Run the callback function to initialise displays array, then register it.
  displayReconfigurationCallback(0, 0, &tapPayload);
  CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallback,
                                           &tapPayload);
  if (!eventTap) {
    log_error("Failed to create event tap");
    return 1;
  }
  runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);
  CGEventTapEnable(eventTap, true);

  CFRunLoopRun();
  CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallback,
                                         &tapPayload);

  free(tapPayload.displays);
  CFRelease(eventTap);
  CFRelease(runLoopSource);

  return 0;
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
