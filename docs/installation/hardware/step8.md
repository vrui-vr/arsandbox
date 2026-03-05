# Step 8: Run the Augmented Reality Sandbox

At this point, calibration is complete. It is now possible to run the main Augmented Reality Sandbox application from inside the source code directory (or the -- optionally chosen -- installation directory):

```sh
./bin/SARndbox -uhm -fpv
```

The `-fpv` ("fix projector view") option tells the AR Sandbox to use the projector calibration matrix created in [step 7](./step7.md). The `-uhm` ("use height map") option tells the AR Sandbox to color-map the 3D surface by elevation, using the default height color map.

It is very important to run the application in full-screen mode on the projector, or at least with the exact same window position and size as `CalibrateProjector` in [step 7](./step7.md). If this is not done correctly, the calibration will not work as desired. To manually switch `SARndbox` into full-screen mode after start-up, press the ++f11++ function key.

To check the calibration, observe how the projected colors and topographic contour lines exactly match the physical features of the sand surface. If there are discrepancies between the two, repeat calibration [step 7](./step7.md) until satisfied. On a typical 40in $\times$ 30in sandbox, where the Kinect is mounted approximately 38in above the sandbox's center point, and using a perpendicularly projecting 1024$\times$768 projector, alignment between the real sand surface and the projected features should be on the order of 1 mm.

`SARndbox` provides a plethora of configuration files and command line options to fine-tune the operation of the Augmented Reality Sandbox as desired. Run `SARndbox -h` to see the full list of options and their default values, or refer to external documentation on the project's web site.
