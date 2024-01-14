#import "DisplayConfiguration.h"
#import <Foundation/Foundation.h>
#import <NaturalLanguage/NaturalLanguage.h>

int nl(char *str) {
	NLTokenizer *tokenizer = [[NLTokenizer alloc] initWithUnit:NLTokenUnitWord];
	NSString *testString = [NSString stringWithUTF8String:str];
	tokenizer.string = testString;
	__block NSInteger count = 0;
	[tokenizer enumerateTokensInRange:NSMakeRange(0, testString.length) usingBlock:^(NSRange tokenRange, NLTokenizerAttributes attributes, BOOL *stop) {
        (void)attributes, (void)stop;
		//NSLog(@"%@ %@", [testString substringWithRange:tokenRange], NSStringFromRange(tokenRange));
		count++;
	}];
	return count;
}
