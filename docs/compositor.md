## Operations

Recompute visible regions of windows in rectangle
    Iterate every intersecting window/view
        Set its drawable region to its frame
        For every intersecting window above it
            Occlude the drawable region by the above window's frame
        If the view isn't fully occluded
            Queue a composite of the window

Window composite


Render frame
    # Step 1: Prep
    Windows that have requested a redraw have their remote memory rendered

    # Step 2: Composite to double-buffer
    For each "composite everything" rect
        Iterate every intersecting and non-transparent view
            For each intersecting visible region of the view
                If the visible region contains the "composite everything" rect
                    Add an "extra draw" of the "composite everything" rect
                    Break
                Else, find the overlapping region of the visible region and "composite everything" rects
                    Add an "extra draw" of the overlapping region
                    Reduce the "composite everything" rect to exclude the overlapping region
                If the "composite everything" rect has been fully occluded
                    Break
        If the "composite everything" rect hasn't been fully occluded
            Draw the desktop background on its remainder
    
    For each "ready to composite" view
        For each visible region of the view
            Draw the visible region
    For each "extra draw" on desktop views
        Draw the "extra draw" rect
    
    Draw the mouse cursor

    # Step 3: Copy to physical memory

    For each "composite everything" rect
        Copy this rect from the double-buffer to physical memory
    For each "extra draw" in every view
        Copy this rect from the double-buffer to physical memory
    For each view to composite
        For each visible region of the view
            Copy this rect from the double-buffer to physical memory
    Copy the mouse cursor rect from the double-buffer to physical memory



## Triggers

Trigger recompute visible regions of windows in rectangle
    Window is added to view hierarchy
    Cmd+Tab to move a window to the top
    Dirty rect during animation frame

Trigger window composite
    Window is added to view hierarchy
    Window requests a redraw
    Recompute visible regions of windows in rectangle
    Desktop shortcut is re-rendered

Trigger "composite everything" rect
    Dirty rect during animation frame
    Animation frame
        Union[PreviousRect, CurrentRect]
    Window dragged
        Union[PreviousRect, CurrentRect]
    Window resized
        Union[PreviousRect, CurrentRect]
    Shortcut dragged
        Union[PreviousRect, CurrentRect]
    Mouse moved
        PreviousMouseFrame
    Desktop image updated
        EntireScreen

Render frame
    Called once per event loop

- state machine for mouse instead of bools that need to be updated
- only draw difference in dragged window frame for much less blitting
- unit tests for compositor
- compositor was acting slow in axle-env awm. went down rabbit holes of captures + replays + flamegraph, but it was fast
    in hosted. Turns out I was skipping drawing a frame if we consumed a msg from the mouse/kb driver, so redraws were only happening
    when the logs viewer drew any log lines
- TODO(PT): Only enable logging if the frame took >= 25ms to render
- in case a window requests remote framebuf render multiple times in one event loop pass, only add it to fetch queue once
