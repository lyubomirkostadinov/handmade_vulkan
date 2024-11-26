#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>

#import <mach-o/dyld.h>
#import <stdlib.h>
#import <string.h>

#import "platform.h"

@interface ApplicationDelegate : NSObject <NSApplicationDelegate>

@end

@implementation ApplicationDelegate

-(void)applicationDidFinishLaunching:(NSNotification*)notification
{
    @autoreleasepool
    {
        NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                         location:NSMakePoint(0,0)
                         modifierFlags:0
                         timestamp:0
                         windowNumber:0
                         context:nil
                         subtype:0
                         data1:0
                         data1:0];

        [NSApp postEvent:event atStart:YES];
    }

    [NSApp stop:nil];

    NSLog(@"Application launched \n");
}

@end

@interface WindowDelegate : NSObject <NSWindowDelegate>
{
    window* Window;
}

-(instancetype)initWithState:(window*)init_state;

@end

@interface ContentView : NSView <NSTextInputClient>
{
    window* window;
    NSTrackingArea* tracking_area;
    NSMutableAttributedString* marked_text;
}

-(instancetype)initWithState:(window *)init_state;

@end

@implementation ContentView

-(instancetype)initWithState:(window *)init_state
{
    self = [super init];

    if(self != nil)
    {
        window = init_state;
    }

    return self;
}

-(nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    return nil;
}

-(NSUInteger)characterIndexForPoint:(NSPoint)point
{
    return 0;
}

-(void)doCommandBySelector:(nonnull SEL)selector
{

}

-(NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange
{
    NSRect rect = {};
    return rect;
}

-(BOOL)hasMarkedText
{
    return false;
}

-(void)insertText:(nonnull id)string replacementRange:(NSRange)replacacementRange
{

}

-(NSRange)markedRange
{
    NSRange range = {};
    return range;
}

-(NSRange)selectedRange
{
    NSRange range = {};
    return range;
}

-(void)setMarkedText:(nonnull id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{

}

-(void)unmarkText
{

}

-(nonnull NSArray<NSAttributedStringKey> *)validAttributesForMarkedText
{
    return [NSArray array];
}

@end

typedef struct platform_state
{
    ApplicationDelegate* AppDelegate;
    window** Windows;
    bool32 ShouldClose;
}platform_state;

typedef struct internal_platform_window_state
{
    WindowDelegate* WindowDelegate;
    NSWindow* Window;
    ContentView* ContentView;
    CAMetalLayer* MetalLayer;
    float32 PixelRatio;
}internal_platform_window_state;

static platform_state InternalState;

@implementation WindowDelegate

-(instancetype)initWithState:(window*)init_state
{
    self = [self init];
    Window = init_state;

    return self;
}

-(BOOL)windowShouldClose:(NSWindow*)sender
{
    InternalState.ShouldClose = true;
    return YES;
}

@end

bool32 Initialize(void)
{
    [NSApplication sharedApplication];

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    InternalState.AppDelegate =  [[ApplicationDelegate alloc] init];

    if(!InternalState.AppDelegate)
    {
        NSLog(@"Failed to create application delegate\n");
        return false;
    }

    InternalState.Windows = (window**)malloc(sizeof(window) * 2);
    InternalState.ShouldClose = false;

    [NSApp setDelegate:InternalState.AppDelegate];

    //[NSApp run];
    [NSApp finishLaunching];

    return true;
}

void Shutdown(void)
{
    if(InternalState.Windows)
    {
        free(InternalState.Windows);
    }

    [InternalState.AppDelegate release];
    InternalState.AppDelegate = 0;
}

bool32 PollEvents(void)
{
    @autoreleasepool
    {
        NSEvent* event;

        for(;;)
        {
            event = [NSApp
                    nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate distantPast]
                    inMode:NSDefaultRunLoopMode
                    dequeue:YES];

            if(!event)
            {
                break;
            }

            [NSApp sendEvent:event];
        }
    }
    return true;
}

bool32 ShouldClose()
{
    return InternalState.ShouldClose;
}

bool32 CreateWindow(int16 Width, int16 Height, const char* Name, WINDOW_STYLE_FLAGS Flags)
{
    window* Window = (window *)malloc(sizeof(window));

    Window->State = (internal_platform_window_state *)malloc(sizeof(internal_platform_window_state));
    Window->State->WindowDelegate = [[WindowDelegate alloc] initWithState:Window];
    Window->State->ContentView = [[ContentView alloc] initWithState:Window];
    [Window->State->ContentView setWantsLayer:(BOOL)YES];
    Window->State->Window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, Width, Height)
                                              styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                                                        NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable
                                              backing:NSBackingStoreBuffered
                                              defer:NO];

    [Window->State->Window setDelegate:Window->State->WindowDelegate];
    [Window->State->Window setContentView:Window->State->ContentView];
    [Window->State->Window setTitle:@(Name)];

    InternalState.Windows[0] = Window;

    ShowWindow(Window);

    Window->State->MetalLayer = [CAMetalLayer layer];
    [Window->State->MetalLayer setBounds:Window->State->ContentView.bounds];
    [Window->State->MetalLayer setDrawableSize: [Window->State->ContentView convertSizeToBacking: Window->State->ContentView.bounds.size]];

    [Window->State->MetalLayer setContentsScale: Window->State->ContentView.window.backingScaleFactor];
    Window->State->PixelRatio = Window->State->MetalLayer.contentsScale;
    [Window->State->ContentView setLayer: Window->State->MetalLayer];

    NSLog(@"ContentsScale = %f \n", Window->State->PixelRatio);

    return true;
}

CAMetalLayer* GetInternalStateMetalLayer()
{
    //TODO(Lyubomir): Casey won't be proud!
    return InternalState.Windows[0]->State->MetalLayer;
}

void DestroyWindow(window* Window)
{

}

void ShowWindow(window* Window)
{
    [Window->State->Window makeKeyAndOrderFront:nil];
}

void HideWindow(window* Window)
{

}

void ProcessKey(void)
{

}

void ProcessButton(void)
{

}

void ProcessMouseMove(void)
{

}

void ProcessMouseWheel(void)
{

}

void* MyNSGLGetProcAddress (const char *Name)
{
    NSSymbol Symbol;
    char *SymbolName;
    SymbolName = (char*)malloc(strlen (Name) + 2);
    strcpy(SymbolName + 1, Name);
    SymbolName[0] = '_';
    Symbol = NULL;
    if (NSIsSymbolNameDefined (SymbolName))
        Symbol = NSLookupAndBindSymbol (SymbolName);
    free (SymbolName);
    return Symbol ? NSAddressOfSymbol (Symbol) : NULL;
}
