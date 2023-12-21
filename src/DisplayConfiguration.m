#import <Cocoa/Cocoa.h>
#import "DisplayConfiguration.h"

int WrappedNSApplicationLoad(void) {
    NSLog(@"WrappedNSApplicationLoad called");
    return NSApplicationLoad();
}
