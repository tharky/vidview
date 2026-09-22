# VidView

<p align="center">
  <img src="docs/vidview.gif" alt="VidView demo" width="100%">
</p>

A small Windows video player I made because finding one with decent frame-by-frame controls was way way way harder than it should be. The built-in Windows players don't do frame stepping, VLC does, but I got tired of little issues like the giant filename overlay sitting on top of the video.

So, I made my own.

VidView is mostly for reviewing clips quickly: step frame by frame, scrub around, mark a loop, slow something down, and get out of the way.

## Features

- exact frame stepping
- hold to scrub forward or backward through frames
- timeline seeking
- 0.25x to 4x playback
- loop start / loop end markers
- frame export
- drag and drop
- MP4 "Open with VidView" support on Windows

Only supported on Windows.

## Controls

| Key | Action |
|---|---|
| `Space` | Play / pause |
| `K` | Previous frame |
| `L` | Next frame |
| hold `,` | Scrub backward |
| hold `.` | Scrub forward |
| `Left` / `Right` | Seek 5 seconds |
| `Shift + Left` / `Shift + Right` | Seek 1 second |
| `-` / `=` | Playback speed down / up |
| `[` | Set loop start |
| `]` | Set loop end |
| `\` | Clear loop |
| `Ctrl + O` | Open video |
| `Ctrl + S` | Save current frame |
| `F` | Fullscreen |
| `Esc` | Leave fullscreen |
| `F1` | Show shortcuts |

## Tech

- C++20
- Qt 6
- FFmpeg
- OpenGL
- CMake + Ninja
- multithreaded video decoding
- frame caching + reverse frame reconstruction
- FFmpeg audio decoding/resampling
- Windows installer + Explorer integration

## Packaging

```powershell
.\scripts\package.ps1
.\scripts\build-installer.ps1