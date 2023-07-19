#include <stdio.h>
#include <time.h>
#include <ApplicationServices/ApplicationServices.h>

#define PRESSED_ARR_SIZE 300
#define LEFTMOUSE_IDX (PRESSED_ARR_SIZE - 2)
#define RIGHTMOUSE_IDX (PRESSED_ARR_SIZE - 1)

struct ActionEvent {
	long long timestampStart;
	uint64_t timeDown;
	short keyCode;
};
extern void action_event_delegate(struct ActionEvent *c_struct);

uint64_t delta_ms(struct timespec start, struct timespec end) {
	uint64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
	return delta_us / 1000;
}

void send(struct timespec *pressed, int pressedIdx) {	
	struct timespec end;
	clock_gettime(CLOCK_REALTIME, &end);

	struct ActionEvent c_struct;
	c_struct.timestampStart = pressed[pressedIdx].tv_sec;
	c_struct.timeDown = delta_ms(pressed[pressedIdx], end);
	c_struct.keyCode = pressedIdx;
	action_event_delegate(&c_struct);
}

CGEventRef myCGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    struct timespec *pressed = (struct timespec *)refcon;

	CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

	if (type == kCGEventKeyDown) {
		clock_gettime(CLOCK_REALTIME, &pressed[keyCode]);
    } else if (type == kCGEventKeyUp) {
		send(pressed, keyCode);
    } else if (type == kCGEventLeftMouseDown) {
		clock_gettime(CLOCK_REALTIME, &pressed[LEFTMOUSE_IDX]);
    } else if (type == kCGEventLeftMouseUp) {
		send(pressed, LEFTMOUSE_IDX);
    } else if (type == kCGEventRightMouseDown) {
		clock_gettime(CLOCK_REALTIME, &pressed[RIGHTMOUSE_IDX]);
    } else if (type == kCGEventRightMouseUp) {
		send(pressed, RIGHTMOUSE_IDX);
	}

    return event;
}

int main(void) {
	struct timespec pressed[PRESSED_ARR_SIZE];

    CFMachPortRef eventTap;
    CGEventMask eventMask;
    CFRunLoopSourceRef runLoopSource;

    eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1 << kCGEventLeftMouseDown) | (1 << kCGEventRightMouseDown) | (1 << kCGEventLeftMouseUp) | (1 << kCGEventRightMouseUp));
	eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, 0, eventMask, myCGEventCallback, &pressed);

    if (!eventTap) {
        printf("Failed to create event tap\n");
        exit(1);
    }

    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    CFRunLoopRun();

    exit(0);
}

