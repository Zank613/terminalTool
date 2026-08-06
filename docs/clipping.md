# Clipping and safe coordinates

Every draw operation is restricted to the framebuffer and the current nested
clip. `ScopedClip` is the recommended interface:

```cpp
{
    tt::Console::ScopedClip clip({ 2, 2, 40, 12 });
    drawContents();
}
```

Rectangle intersection and containment use widened arithmetic, preventing
signed overflow for extreme integer coordinates. Horizontal and vertical line
operations intersect with the visible frame before looping, so a huge
completely off-screen line does not perform billions of iterations.

Control scalars are never emitted from framebuffer cells. They are replaced by
U+FFFD before storage.

`popClip()` asserts on an empty stack in debug builds. Release builds retain the
safe no-op behavior.
