wadreader (Doom wad file reader, and walk-through)

Coyright 2002, Brad Broerman

This is a quick and dirty attempt to take what I leared when writing Ray, and put together a Doom map reader and rendering engine. 
The code is pretty rough, and not commented. It also is incomplete... It does not render sprites (objects), floors, or ceilings. 
It also has a bug: When lookign at walls off-angle, the texture slides to the left / right when you move towards or away from it. 

This version was originally written for Borland C++, and then ported over to Microsoft Visual Studio. I've compiled once in Studio 2003, but not since.

I haven't touched this file in over 10 years, but figured I'd put it up here in case someone finds it useful, or can figure out my texturing bug :)
