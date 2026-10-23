# Graphics modes

Open **Options → Graphics** from the main menu or pause menu. **Quality Mode**
trades frame-rate headroom for a sharper image or richer lighting. Changes apply
while in the menu and are remembered after successful activation.

| System | Quality off | Quality on | Initial setting |
| --- | --- | --- | --- |
| Nintendo 3DS / 3DS XL | Existing 400×240 profile, 30 FPS target | 800×240 wide output, 30 FPS target | On |
| Nintendo 2DS | Existing 400×240 profile, 30 FPS target | 400×240 with directional lighting, 30 FPS target | On |
| New Nintendo 3DS / 3DS XL / New 2DS XL | 400×240, 60 FPS target | 800×240 wide output, 60 FPS target | On |

The lower display remains 320×240. Wide output doubles horizontal samples while
preserving the screen's physical aspect ratio. It is not an 800p display mode.
Original 2DS hardware does not support wide output.

Frame rates are targets and can vary by scene.

Quality defaults to On for every recognized model. Existing saved selections
remain in effect. Preferences for original 3DS, original 2DS and New models are
stored separately.
An unknown model uses the standard display. If a Quality render target cannot be
allocated, the game restores Normal output without saving the failed selection.

**Stereo 3D** follows the console's 3D slider on Nintendo 3DS, 3DS XL, New Nintendo
3DS and New Nintendo 3DS XL. Raise the slider to enable 3D and adjust its depth
gradually. Lower it fully to restore flat output. No menu switch or restart is
required.

Stereo automatically uses the Normal profile at 400×240 per eye. Quality Mode is
unavailable while stereo is active. The game keeps your Quality preference and
restores it when you lower the slider fully, including during gameplay.
Nintendo 2DS and New Nintendo 2DS XL keep stereo disabled.

Stereo can reduce frame-rate headroom. Its target is 30 FPS on original 3DS and
60 FPS on New 3DS; achieved performance depends on the scene.

The living-room menu and its animated close-ups remain at screen depth.
The car selector presents the retail 3D preview model. Hold L/R to rotate it;
left/right changes vehicles. Names and controls stay at screen depth. Pause
objective cards put their frame, portrait and text together on a shallow
foreground layer. Driving, walking and interiors share a projective world with
restrained foreground depth. Walking and driving anchor the screen plane near
the player when the camera changes, rather than following every movement.
Existing camera collisions select a conservative depth range; recovery waits
for a clear camera before restoring depth. Bumper and cinematic views retain
the conservative range. Pausing also uses that range for the world backdrop,
keeping the objective card in front of every background surface. The lower
screen remains flat.

On stereoscopic 3DS models, the green 3D availability light stays on in menus and
when the slider is down. The light indicates availability, not active eye separation.
Quality Mode still restores 800×240 wide output when the slider is fully down.
