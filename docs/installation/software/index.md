# Installing the Augmented Reality Sandbox

Both the simple and advanced instructions will install the ARSandbox with the standard settings.

The Augmented Reality Sandbox package contains the sandbox application itself, `SARndbox`, and a calibration utility to interactively measure a transformation between the Kinect camera scanning the sandbox surface, and the projector projecting onto it. The setup procedure described below also uses several utilities from the Kinect 3D video capture project.

!!! warning
    The Augmented Reality Sandbox requires Vrui version 13.0 build 001 or newer, and the Kinect 3D Video Capture Project version 5.0 or newer.

## Prerequisites

The Augmented Reality Sandbox requires Vrui version 13.0 build 001 or newer, and the Kinect 3D Video Capture Project version 5.0 or newer.

Both the simple and advanced installation instructions assume that you have already installed the [Vrui VR Toolkit](https://github.com/vrui-vr/vrui) and the [Kinect 3D Video Package](https://github.com/vrui-vr/kinect) by following the simple installation instructions of those respective packages.

## Simple vs. advanced install

If you want to run the ARSandbox from multiple user accounts, or from a dedicated user account with limited access rights, you need to install it in a system location where it can be accessed by those user accounts.

For example, you could install it in the same `/usr/local` directory hierarchy where you installed the Vrui VR Toolkit following the simple instructions included with Vrui.

To do this, follow the [advanced install](./advanced_install.md) instructions.