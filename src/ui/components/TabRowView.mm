#import "TabRowView.h"
#import "SidebarView.h"
#import "MainWindowController.h"
#import "DesignSystem.h"
#import "DSButton.h"

// Pasteboard type for tab dragging
NSPasteboardType const TabRowPasteboardType = @"com.orbfox.tabrow";

@implementation TabRowView {
    NSPoint _dragStartPoint;
    BOOL _isDragging;
    NSTrackingArea* _trackingArea;
    BOOL _isHovered;
    DSIconButton* _closeButton;
    NSImageView* _pinIconView;
    NSImageView* _muteIconView;
    NSImageView* _hibernateIconView;
    NSProgressIndicator* _loadingIndicator;
    NSImageView* _faviconView;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layer.cornerRadius = [DSLayout cornerRadiusMedium];
        _isHovered = NO;
        _isSelected = NO;
        _isLoading = NO;
        _isPinned = NO;
        _isMuted = NO;
        _isHibernated = NO;

        CGFloat iconSize = [DSLayout iconSizeSmall];
        CGFloat padding = [DSSpacing sm];

        // Favicon view
        _faviconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            padding + 2, (frame.size.height - iconSize) / 2, iconSize, iconSize)];
        _faviconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _faviconView.hidden = YES;
        [self addSubview:_faviconView];

        // Loading indicator
        _loadingIndicator = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(
            padding + 2, (frame.size.height - iconSize) / 2, iconSize, iconSize)];
        _loadingIndicator.style = NSProgressIndicatorStyleSpinning;
        _loadingIndicator.controlSize = NSControlSizeSmall;
        _loadingIndicator.displayedWhenStopped = NO;
        [self addSubview:_loadingIndicator];

        // Close button
        _closeButton = [DSIconButton buttonWithIcon:@"xmark"];
        _closeButton.frame = NSMakeRect(frame.size.width - 32, (frame.size.height - 20) / 2, 20, 20);

        // Pin icon (same position as close button - they swap based on hover)
        _pinIconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            frame.size.width - 29, (frame.size.height - 14) / 2, 14, 14)];
        _pinIconView.image = [NSImage imageWithSystemSymbolName:@"pin.fill" accessibilityDescription:@"Pinned"];
        _pinIconView.contentTintColor = [DSColors textSecondary];
        _pinIconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _pinIconView.autoresizingMask = NSViewMinXMargin;
        _pinIconView.hidden = YES;
        [self addSubview:_pinIconView];

        // Mute icon (to the left of pin icon area)
        _muteIconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            frame.size.width - 44, (frame.size.height - 14) / 2, 14, 14)];
        _muteIconView.image = [NSImage imageWithSystemSymbolName:@"speaker.slash.fill" accessibilityDescription:@"Muted"];
        _muteIconView.contentTintColor = [DSColors textSecondary];
        _muteIconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _muteIconView.autoresizingMask = NSViewMinXMargin;
        _muteIconView.hidden = YES;
        [self addSubview:_muteIconView];

        // Hibernation icon (moon symbol)
        _hibernateIconView = [[NSImageView alloc] initWithFrame:NSMakeRect(
            frame.size.width - 58, (frame.size.height - 14) / 2, 14, 14)];
        _hibernateIconView.image = [NSImage imageWithSystemSymbolName:@"moon.zzz.fill" accessibilityDescription:@"Hibernated"];
        _hibernateIconView.contentTintColor = [DSColors textSecondary];
        _hibernateIconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _hibernateIconView.autoresizingMask = NSViewMinXMargin;
        _hibernateIconView.hidden = YES;
        [self addSubview:_hibernateIconView];

        _closeButton.autoresizingMask = NSViewMinXMargin;
        _closeButton.hidden = YES;
        _closeButton.target = self;
        _closeButton.action = @selector(closeTab:);
        [self addSubview:_closeButton];
    }
    return self;
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow)
               owner:self
            userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    (void)event;
    _isHovered = YES;
    _closeButton.hidden = NO;
    _pinIconView.hidden = YES;  // Hide pin icon, show close button on hover
    [self setNeedsDisplay:YES];
}

- (void)mouseExited:(NSEvent*)event {
    (void)event;
    _isHovered = NO;
    _closeButton.hidden = YES;
    _pinIconView.hidden = !_isPinned;  // Show pin icon when not hovering (if pinned)
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
    _dragStartPoint = [self convertPoint:event.locationInWindow fromView:nil];
    _isDragging = NO;
}

- (void)mouseUp:(NSEvent*)event {
    (void)event;
    if (!_isDragging) {
        // Only activate tab if we didn't drag
        [_sidebarView.windowController activateTab:_tabId];
    }
    _isDragging = NO;
}

- (void)mouseDragged:(NSEvent*)event {
    NSPoint currentPoint = [self convertPoint:event.locationInWindow fromView:nil];
    CGFloat dx = currentPoint.x - _dragStartPoint.x;
    CGFloat dy = currentPoint.y - _dragStartPoint.y;
    CGFloat distance = sqrt(dx * dx + dy * dy);

    // Start drag if moved more than 5 pixels
    if (!_isDragging && distance > 5) {
        _isDragging = YES;
        [self startDragWithEvent:event];
    }
}

- (void)startDragWithEvent:(NSEvent*)event {
    // Create dragging item with tab ID
    NSPasteboardItem* pbItem = [[NSPasteboardItem alloc] init];
    [pbItem setString:[NSString stringWithFormat:@"%d", _tabId] forType:TabRowPasteboardType];

    // Create a snapshot of this view for the drag image
    NSImage* dragImage = [[NSImage alloc] initWithSize:self.bounds.size];
    [dragImage lockFocus];
    [self drawRect:self.bounds];
    [dragImage unlockFocus];

    NSDraggingItem* dragItem = [[NSDraggingItem alloc] initWithPasteboardWriter:pbItem];
    [dragItem setDraggingFrame:self.bounds contents:dragImage];

    [self beginDraggingSessionWithItems:@[dragItem] event:event source:self];
}

#pragma mark - NSDraggingSource

- (NSDragOperation)draggingSession:(NSDraggingSession*)session sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
    (void)session;
    (void)context;
    return NSDragOperationMove;
}

- (void)rightMouseDown:(NSEvent*)event {
    [self showContextMenu:event];
}

- (void)otherMouseDown:(NSEvent*)event {
    // Middle-click (button 2) duplicates tab in background
    if (event.buttonNumber == 2) {
        [self duplicateTabInBackground:nil];
    }
}

- (void)duplicateTabInBackground:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab) {
        [_sidebarView.windowController openUrlInBackgroundTab:[NSString stringWithUTF8String:tab->url.c_str()]];
    }
}

- (void)showContextMenu:(NSEvent*)event {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Tab"];

    // Pin/Unpin option
    NSString* pinTitle = _isPinned ? @"Unpin Tab" : @"Pin Tab";
    NSMenuItem* pinItem = [[NSMenuItem alloc] initWithTitle:pinTitle
                                                     action:@selector(togglePinTab:)
                                              keyEquivalent:@""];
    pinItem.target = self;
    [menu addItem:pinItem];

    // Mute/Unmute option
    NSString* muteTitle = _isMuted ? @"Unmute Tab" : @"Mute Tab";
    NSMenuItem* muteItem = [[NSMenuItem alloc] initWithTitle:muteTitle
                                                      action:@selector(toggleMuteTab:)
                                               keyEquivalent:@""];
    muteItem.target = self;
    [menu addItem:muteItem];

    // Rename Tab
    NSMenuItem* renameItem = [[NSMenuItem alloc] initWithTitle:@"Rename Tab"
                                                        action:@selector(renameTab:)
                                                 keyEquivalent:@""];
    renameItem.target = self;
    [menu addItem:renameItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeItem = [[NSMenuItem alloc] initWithTitle:@"Close Tab"
                                                       action:@selector(closeTab:)
                                                keyEquivalent:@""];
    closeItem.target = self;
    [menu addItem:closeItem];

    NSMenuItem* duplicateItem = [[NSMenuItem alloc] initWithTitle:@"Duplicate Tab"
                                                           action:@selector(duplicateTab:)
                                                    keyEquivalent:@""];
    duplicateItem.target = self;
    [menu addItem:duplicateItem];

    // Move to submenu (moves tab to another workspace)
    NSMenuItem* moveToItem = [[NSMenuItem alloc] initWithTitle:@"Move to"
                                                        action:nil
                                                 keyEquivalent:@""];
    NSMenu* moveSubmenu = [[NSMenu alloc] initWithTitle:@"Move to"];

    TabManager* tabManager = _sidebarView.windowController.tabManager;
    Workspace* currentWorkspace = tabManager ? tabManager->GetActiveWorkspace() : nullptr;

    if (tabManager) {
        // Add existing workspaces (except current)
        for (const auto& workspace : tabManager->GetWorkspaces()) {
            if (currentWorkspace && workspace->id == currentWorkspace->id) {
                continue;
            }
            NSMenuItem* wsItem = [[NSMenuItem alloc] initWithTitle:
                [NSString stringWithUTF8String:workspace->name.c_str()]
                                                            action:@selector(moveTabToWorkspace:)
                                                     keyEquivalent:@""];
            wsItem.target = self;
            wsItem.tag = workspace->id;
            [moveSubmenu addItem:wsItem];
        }

        // Add separator and "New Space" option
        if (tabManager->GetWorkspaces().size() > 1) {
            [moveSubmenu addItem:[NSMenuItem separatorItem]];
        }
        NSMenuItem* newSpaceItem = [[NSMenuItem alloc] initWithTitle:@"New Space"
                                                              action:@selector(moveTabToNewWorkspace:)
                                                       keyEquivalent:@""];
        newSpaceItem.target = self;
        [moveSubmenu addItem:newSpaceItem];
    }

    moveToItem.submenu = moveSubmenu;
    [menu addItem:moveToItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* reloadItem = [[NSMenuItem alloc] initWithTitle:@"Reload Tab"
                                                        action:@selector(reloadTab:)
                                                 keyEquivalent:@""];
    reloadItem.target = self;
    [menu addItem:reloadItem];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeOthersItem = [[NSMenuItem alloc] initWithTitle:@"Close Other Tabs"
                                                             action:@selector(closeOtherTabs:)
                                                      keyEquivalent:@""];
    closeOthersItem.target = self;
    [menu addItem:closeOthersItem];

    [NSMenu popUpContextMenu:menu withEvent:event forView:self];
}

- (void)togglePinTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab) {
        tab->is_pinned = !tab->is_pinned;
        _isPinned = tab->is_pinned;
        _pinIconView.hidden = !_isPinned || _isHovered;  // Show pin icon only when pinned and not hovering
        [self setNeedsDisplay:YES];
    }
}

- (void)toggleMuteTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab && tab->browser) {
        tab->is_muted = !tab->is_muted;
        tab->browser->GetHost()->SetAudioMuted(tab->is_muted);
        _isMuted = tab->is_muted;
        _muteIconView.hidden = !_isMuted;
        [self setNeedsDisplay:YES];
    }
}

- (void)closeTab:(id)sender {
    (void)sender;
    [_sidebarView.windowController closeTab:_tabId];
}

- (void)duplicateTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab) {
        [_sidebarView.windowController openUrlInNewTab:[NSString stringWithUTF8String:tab->url.c_str()]];
    }
}

- (void)reloadTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (tab && tab->browser) {
        tab->browser->Reload();
    }
}

- (void)closeOtherTabs:(id)sender {
    (void)sender;
    Workspace* workspace = _sidebarView.windowController.tabManager->GetActiveWorkspace();
    if (!workspace) return;

    std::vector<int> tabsToClose;
    for (const auto& tab : workspace->tabs) {
        if (tab->id != _tabId) {
            tabsToClose.push_back(tab->id);
        }
    }

    for (int tabId : tabsToClose) {
        [_sidebarView.windowController closeTab:tabId];
    }
}

- (void)renameTab:(id)sender {
    (void)sender;
    Tab* tab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!tab) return;

    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Rename Tab";
    alert.informativeText = @"Enter a new title for this tab:";
    [alert addButtonWithTitle:@"Rename"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 250, 24)];
    input.stringValue = [NSString stringWithUTF8String:tab->title.c_str()];
    alert.accessoryView = input;

    __weak TabRowView* weakSelf = self;
    int tabId = _tabId;
    [alert beginSheetModalForWindow:_sidebarView.windowController.window
                  completionHandler:^(NSModalResponse response) {
        if (response == NSAlertFirstButtonReturn) {
            NSString* newTitle = input.stringValue;
            if (newTitle.length > 0) {
                TabRowView* strongSelf = weakSelf;
                if (strongSelf) {
                    Tab* t = strongSelf->_sidebarView.windowController.tabManager->GetTabById(tabId);
                    if (t) {
                        t->title = [newTitle UTF8String];
                        [strongSelf setTitle:newTitle];
                    }
                }
            }
        }
    }];
}

- (void)duplicateTabToWorkspace:(NSMenuItem*)sender {
    int targetWorkspaceId = (int)sender.tag;
    Tab* sourceTab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!sourceTab) return;

    NSString* urlStr = [NSString stringWithUTF8String:sourceTab->url.c_str()];

    // Switch to target workspace
    _sidebarView.windowController.tabManager->SetActiveWorkspace(targetWorkspaceId);

    // Create new tab with same URL
    [_sidebarView.windowController createNewTab:urlStr];

    // Reload UI
    [_sidebarView reloadWorkspaceTabs];
    [_sidebarView reloadTabs];
}

- (void)moveTabToWorkspace:(NSMenuItem*)sender {
    int targetWorkspaceId = (int)sender.tag;
    TabManager* tabManager = _sidebarView.windowController.tabManager;

    if (tabManager->MoveTabToWorkspace(_tabId, targetWorkspaceId)) {
        // Reload UI for both source and target workspaces
        [_sidebarView reloadWorkspaceTabs];
        [_sidebarView reloadTabs];
    }
}

- (void)moveTabToNewWorkspace:(id)sender {
    (void)sender;
    TabManager* tabManager = _sidebarView.windowController.tabManager;

    // Create new workspace with auto-generated unique name
    Workspace* newWorkspace = tabManager->CreateWorkspace("");

    if (newWorkspace && tabManager->MoveTabToWorkspace(_tabId, newWorkspace->id)) {
        [_sidebarView reloadWorkspaceTabs];
        [_sidebarView reloadTabs];
    }
}

- (void)duplicateTabToNewWorkspace:(id)sender {
    (void)sender;
    Tab* sourceTab = _sidebarView.windowController.tabManager->GetTabById(_tabId);
    if (!sourceTab) return;

    NSString* urlStr = [NSString stringWithUTF8String:sourceTab->url.c_str()];

    // Create new workspace
    TabManager* tabManager = _sidebarView.windowController.tabManager;
    size_t count = tabManager->GetWorkspaces().size();
    NSString* spaceName = [NSString stringWithFormat:@"WS %zu", count + 1];
    Workspace* newWorkspace = tabManager->CreateWorkspace([spaceName UTF8String]);

    if (!newWorkspace) return;

    // Switch to new workspace and create tab
    tabManager->SetActiveWorkspace(newWorkspace->id);
    [_sidebarView.windowController createNewTab:urlStr];

    // Reload UI
    [_sidebarView reloadWorkspaceTabs];
    [_sidebarView reloadTabs];
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    NSColor* bgColor = nil;
    if (_isSelected) {
        bgColor = [DSColors surfaceActive];
    } else if (_isHovered) {
        bgColor = [DSColors surfaceHover];
    }

    if (bgColor) {
        [bgColor setFill];
        CGFloat inset = [DSSpacing xs];
        NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, inset, 2)
                                                             xRadius:[DSLayout cornerRadiusMedium]
                                                             yRadius:[DSLayout cornerRadiusMedium]];
        [path fill];
    }

    // Draw title
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    style.lineBreakMode = NSLineBreakByTruncatingTail;

    NSDictionary* attrs = [DSTypography attributesWithStyle:DSFontStyleBody
                                                      color:[DSColors textPrimary]];
    NSMutableDictionary* mutableAttrs = [attrs mutableCopy];
    mutableAttrs[NSParagraphStyleAttributeName] = style;

    BOOL hasIcon = _isLoading || (_favicon != nil);
    CGFloat titleX = hasIcon ? 32 : [DSSpacing md];
    NSRect titleRect = NSMakeRect(titleX, 10, self.bounds.size.width - titleX - 50, 18);
    [_title drawInRect:titleRect withAttributes:mutableAttrs];
}

- (void)setTitle:(NSString*)title {
    _title = [title copy];
    [self setNeedsDisplay:YES];
}

- (void)setIsSelected:(BOOL)isSelected {
    _isSelected = isSelected;
    [self setNeedsDisplay:YES];
}

- (void)setIsLoading:(BOOL)isLoading {
    _isLoading = isLoading;
    if (isLoading) {
        [_loadingIndicator startAnimation:nil];
        _faviconView.hidden = YES;
    } else {
        [_loadingIndicator stopAnimation:nil];
        _faviconView.hidden = (_favicon == nil);
    }
    [self setNeedsDisplay:YES];
}

- (void)setFavicon:(NSImage*)favicon {
    _favicon = favicon;
    _faviconView.image = favicon;
    _faviconView.hidden = (_isLoading || favicon == nil);
    [self setNeedsDisplay:YES];
}

- (void)setIsPinned:(BOOL)isPinned {
    _isPinned = isPinned;
    _pinIconView.hidden = !isPinned || _isHovered;  // Show pin icon only when pinned and not hovering
    [self setNeedsDisplay:YES];
}

- (void)setIsMuted:(BOOL)isMuted {
    _isMuted = isMuted;
    _muteIconView.hidden = !isMuted;
    [self setNeedsDisplay:YES];
}

- (void)setIsHibernated:(BOOL)isHibernated {
    _isHibernated = isHibernated;
    _hibernateIconView.hidden = !isHibernated;
    // Dim the tab row when hibernated
    self.alphaValue = isHibernated ? 0.6 : 1.0;
    [self setNeedsDisplay:YES];
}

@end
