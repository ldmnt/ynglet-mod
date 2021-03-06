This is a mod for [Ynglet](https://www.ynglet.com/) that changes
the behavior of collectable objects.

Some of the triangles in the game only spawn when a group of triangle shards
has been collected without touching a bubble. In the normal course of the game,
such a triangle and the associated shards will only respawn if the level is reloaded. 
When the mod is active, collecting these triangles makes the shards reappear, and
collecting them will spawn the triangle again.
This allows for much faster iterations when practicing these sequences.

Running the mod will detect a running instance of the game and directly interact
with the game’s memory. It will not modify the game files in any way.

This works only on 64-bit Windows and must run as administrator (in order to get
access to the game’s memory).

## Acknowledgment

The symbols for the game executable were generated with
[IL2CppInspector](https://github.com/djkaty/Il2CppInspector)

## Installation

### Binary release

Go to the Release page and download `ynglet-mod.zip` from the latest version. 
Verify that the zip file was not tampered with, for instance in a powershell:
```bash
Get-FileHash path/to/ynglet-mod.zip -Algorithm MD5
```
Make sure that the output matches the hash in the release’s description,
then extract the contents somewhere.
The mod is made of an executable YngletMod.exe and a DLL file IL2CppDLL.dll.
They can be placed anywhere as long as they are both in the same directory.

### From source

The project is compiled with msbuild. For instance, open the .sln file with
[Visual Studio](https://visualstudio.microsoft.com/),
then select the Release - x64 target and click Build -> Build Solution.
Open the build folder (x64/Release) and copy the two files Ynglet.exe and
IL2CppDLL.dll anywhere, as long as they are in the same folder.

## Usage

Run YngletMod.exe as administrator, this starts a console and waits for the game to
be launched. Once the game is started (or directly if the game was already running),
The console closes and another one opens and displays some information, including whether the mod 
is currently active or not, which means that the mod was attached successfully.

Note that even when the mod is disabled, it still slightly affects the game
behavior. I had to remove some effects that are not very visible in order
to make the hack work properly. You need to do a full restart of the game
to get it to run without any modification again.