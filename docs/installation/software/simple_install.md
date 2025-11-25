# Installing DataLab VR Software

<!-- define abbreviations -->
*[ARSandbox]: Augmented Reality Sandbox

The following instructions describe the simplified installation procedure for DataLab-developed VR software, using PullPackage, a simple custom package management tool.

## Prerequisites
DataLab's VR software runs on the Linux operating system, and requires that operating system to run on a real computer, in other words, not on a virtual machine. While DataLab's VR software runs on any Linux distribution, PullPackage, DataLab's package manager, currently only works with two large families of Linux distributions: 

* RedHat (RedHat Linux, CentOS, Fedora, ...)  
* Ubuntu (Ubuntu, Linux Mint, ...)

Additionally, running DataLab's VR software effectively requires that the computer has a discrete graphics card, and has the drivers for that graphics card installed.

In detail, the prerequisite installation steps are as follows:

1. Install a compatible (RedHat- or Ubuntu-based) Linux version on a real computer (not a virtual machine), either as sole operating system or in a dual-boot configuration.

2. Install the proper driver for the computer's discrete graphics card. For Nvidia graphics cards, this must be the vendor-supplied nvidia driver, not the open-source nouveau driver.

## Install PullPackage

To install the PullPackage package manager, copy the command from the following box into a terminal window and press the Enter key:

```sh
curl https://vroom.library.ucdavis.edu/PullPackage | bash
```


At some point during installation, the terminal window will ask you to enter your user password. This is required to create files in several system locations. Specifically, those locations are:

```sh
/opt/PullPackage
/usr/local/bin
```

Later on, PullPackage may ask for your user password again to install packages such as Vrui or SARndbox. Generally, software packages installed by PullPackage end up inside the /opt directory, and may create files in /usr/local/bin and inside the /etc directory.

If the terminal responds with an error message like "bash: curl: command not found", please copy and run the following alternative command:

```sh
wget -O - https://vroom.library.ucdavis.edu/PullPackage | bash
```

If either of the two commands above succeed, you are ready to install DataLab VR software.

## Install the ARSandbox

After PullPackage has been installed successfully, the ARSandbox can be installed by entering the following commands into a terminal window in the order below.

Vrui is a software development toolkit for interactive virtual reality applications. It is also the foundation of the ARSandbox.

```sh
PullPackage Vrui
```

The Kinect package contains drivers for several types of 3D cameras that are used by the ARSandbox, including two versions of the original Microsoft Kinect camera.

```sh
PullPackage Kinect
```

The SARndbox package contains the actual ARSandbox application.

```sh
PullPackage SARndbox
```

Alternatively, the following command will install all three packages required for the ARSandbox in one go:

```sh
PullPackage Vrui && PullPackage Kinect && PullPackage SARndbox
```

## Install Support for SteamVR Headsets (HTC Vive, HTC Vive Pro, Valve Index...)
In order to use SteamVR-compatible commodity VR headsets with Vrui VR software, install the SteamVR package before installing the Vrui package:

```sh
PullPackage SteamVR 
```	

Note: The SteamVR package is not a complete Steam and/or SteamVR installation. It only contains the low-level drivers required to connect Vrui VR software to SteamVR headsets. To avoid compatibility issues, we strongly recommend that you install this package even if you already have Steam and/or SteamVR installed on your computer.

## Install Other Vrui VR Applications
To install other Vrui VR applications such as 3D Visualizer, LiDAR Viewer, VR ProtoShop, etc., first install the Vrui package (and optionally the SteamVR package before it), then install the package(s) for the desired Vrui application(s).

### List of Currently Hosted Vrui VR Applications
3DVisualizer: The 3D Visualizer application to analyze volumetric 3D gridded data.

LidarViewer: The LiDAR Viewer application to visualize and measure very large 3D point cloud data.