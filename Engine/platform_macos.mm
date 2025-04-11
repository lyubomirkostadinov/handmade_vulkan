#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>

#import <mach-o/dyld.h>
#import <stdlib.h>
#import <string.h>
#import <iostream>

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

CGFloat deltaX, deltaY;
NSPoint lastMouseLocation;
void HandleMouseMove(NSEvent *event)
{
    NSPoint currentLocation = [event locationInWindow];
    deltaX = currentLocation.x - lastMouseLocation.x;
    deltaY = lastMouseLocation.y - currentLocation.y;

    lastMouseLocation = currentLocation;
}

void ProcessMouseMove(float *X, float *Y)
{
    *X = (float)deltaX;
    *Y = (float)deltaY;
}

bool WasdKeyPressed[4];
bool LockCamera = false;
void HandleKeyDown(NSEvent* Event)
{
    const char* Characters = [[Event charactersIgnoringModifiers] UTF8String];

    if (strcmp(Characters, "p") == 0)
    {
        std::cout << "P pressed" << std::endl;
    }

    if (strcmp(Characters, "w") == 0)
    {
        WasdKeyPressed[0] = true;
        std::cout << "W pressed" << std::endl;
    }
    else if (strcmp(Characters, "a") == 0)
    {
        WasdKeyPressed[1] = true;
        std::cout << "A pressed" << std::endl;
    }
    else if (strcmp(Characters, "s") == 0)
    {
        WasdKeyPressed[2] = true;
        std::cout << "S pressed" << std::endl;
    }
    else if (strcmp(Characters, "d") == 0)
    {
        WasdKeyPressed[3] = true;
        std::cout << "D pressed" << std::endl;
    }
}

void HandleKeyUp(NSEvent* Event)
{
    const char* Characters = [[Event charactersIgnoringModifiers] UTF8String];

    if (strcmp(Characters, "p") == 0)
    {
        if(!LockCamera)
        {
            printf("LOCK The Camera");
            CGDisplayHideCursor(NULL);
            CGAssociateMouseAndMouseCursorPosition(false);

            LockCamera = true;
        }
        else
        {
            printf("UNLOCK The Camera");
            CGDisplayShowCursor(NULL);
            CGAssociateMouseAndMouseCursorPosition(true);

            LockCamera = false;
        }
        std::cout << "P released" << std::endl;
    }

    if (strcmp(Characters, "w") == 0)
    {
        WasdKeyPressed[0] = false;
        std::cout << "W released" << std::endl;
    }
    else if (strcmp(Characters, "a") == 0)
    {
        WasdKeyPressed[1] = false;
        std::cout << "A released" << std::endl;
    }
    else if (strcmp(Characters, "s") == 0)
    {
        WasdKeyPressed[2] = false;
        std::cout << "S released" << std::endl;
    }
    else if (strcmp(Characters, "d") == 0)
    {
        WasdKeyPressed[3] = false;
        std::cout << "D released" << std::endl;
    }
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

            if (!event)
            {
                break;
            }

            switch ([event type])
            {
                case NSEventTypeKeyDown:
                    HandleKeyDown(event);
                    break;
                case NSEventTypeKeyUp:
                    HandleKeyUp(event);
                    break;
                case NSEventTypeMouseMoved:
                    HandleMouseMove(event);
                    break;
                default:
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

int32 ProcessKey(int32 Key)
{

    int32 Result = -1;

    if(Key == KeyW && WasdKeyPressed[0] == true)
    {
        printf("SIGNAL MOVE WWWWWWW\n");
        Result = KeyW;
    }
    if(Key == KeyA && WasdKeyPressed[1] == true)
    {
        Result = KeyA;
    }
    if(Key == KeyS && WasdKeyPressed[2] == true)
    {
        Result = KeyS;
    }
    if(Key == KeyD && WasdKeyPressed[3] == true)
    {
        Result = KeyD;
    }

    return Result;
}

void ProcessButton(void)
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
