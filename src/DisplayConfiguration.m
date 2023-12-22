#import "DisplayConfiguration.h"
#import <AppKit/AppKit.h>

int WrappedNSApplicationLoad(void) {
    NSLog(@"WrappedNSApplicationLoad called");
    return NSApplicationLoad();
}
