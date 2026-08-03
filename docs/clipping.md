# Clipping regions

Clipping limits every Console drawing operation to the intersection of the
framebuffer and the current clip.

## RAII clipping

```cpp
{
    tt::Console::ScopedClip clip({ 4, 3, 40, 12 });
    drawWorld();
}
```

The previous clip is restored automatically. Nested `ScopedClip` objects
intersect with their parents.

## Manual clipping

```cpp
tt::Console::pushClip(panelArea);
drawPanelContents();
tt::Console::popClip();
```

`popClip()` is safe on an empty stack. `clearClips()` removes all user regions,
and `currentClip()` returns the effective rectangle.

Framebuffer initialization and resizing clear the clip stack so stale regions
cannot survive a terminal dimension change.
