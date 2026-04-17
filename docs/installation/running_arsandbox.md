# Running the Augmented Reality Sandbox

<!-- define abbreviations -->
*[ARSandbox]: Augmented Reality Sandbox

## Table of Contents

- [Running the Augmented Reality Sandbox](#Running the Augmented Reality Sandbox)
    - [Table of Contents](#table-of-contents)
    - [Step 1: Run the AR Sandbox](#step-1-run-the-arsandbox)

At this point, the Augmented Reality Sandbox is configured, calibrated, and ready to run.

## Step 1: Run the ARSandbox

To run the main AR Sandbox application, run in a terminal window:
```sh
cd ~/src/SARndbox-2.8
./bin/SARndbox -uhm -fpv
```

The `-uhm` argument tells the SARndbox application to apply an elevation color map to the sand surface, and the `-fpv` argument tells it to use the projector/camera calibration collected in [Step 6 in "System Integration, Configuration, and Calibration"](system_integration.md##step-6-projector-or-camera-calibration).

!!! warning
    Set SARndbox's window to full-screen mode by pressing F11, or the projector/camera calibration will not line up.
