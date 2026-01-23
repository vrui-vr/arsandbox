########################################################################
# Makefile for the Augmented Reality Sandbox.
# Copyright (c) 2012-2026 Oliver Kreylos
#
# This file is part of the WhyTools Build Environment.
# 
# The WhyTools Build Environment is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
# 
# The WhyTools Build Environment is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the WhyTools Build Environment; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA
########################################################################

# Directory containing the Vrui build system. The directory below
# matches the default Vrui installation; if Vrui's installation
# directory was changed during Vrui's installation, the directory below
# must be adapted.
VRUI_MAKEDIR = /usr/local/share/Vrui-14.0/make

# Base installation directory for the Augmented Reality Sandbox. If this
# is set to the default of $(PROJECT_ROOT), the Augmented Reality
# Sandbox does not have to be installed to be run. Created executables
# and resources will be installed in the bin, etc, and share directories
# under the given base directory, respectively.
# Important note: Do not use ~ as an abbreviation for the user's home
# directory here; use $(HOME) instead.
INSTALLDIR = $(PROJECT_ROOT)

########################################################################
# Everything below here should not have to be changed
########################################################################

# Name of the package
PROJECT_NAME = SARndbox
PROJECT_DISPLAYNAME = Augmented Reality Sandbox

# Version number for installation subdirectories. This is used to keep
# subsequent release versions of the Augmented Reality Sandbox from
# clobbering each other. The value should be identical to the
# major.minor version number found in VERSION in the root package
# directory.
PROJECT_MAJOR = 5
PROJECT_MINOR = 1

# Include definitions for the system environment and system-provided
# packages
include $(VRUI_MAKEDIR)/SystemDefinitions
include $(VRUI_MAKEDIR)/Packages.System
include $(VRUI_MAKEDIR)/Configuration.Vrui
include $(VRUI_MAKEDIR)/Packages.Vrui
include $(VRUI_MAKEDIR)/Configuration.Kinect
include $(VRUI_MAKEDIR)/Packages.Kinect

# Set up package directories
ETCDIR = etc
SHAREDIR = share
SHADERDIR = $(SHAREDIR)/Shaders
SHADERINSTALLDIR = $(SHAREINSTALLDIR)/Shaders

########################################################################
# Specify additional compiler and linker flags
########################################################################

CFLAGS += -Wall -pedantic

########################################################################
# List common packages used by all components of this project
# (Supported packages can be found in $(VRUI_MAKEDIR)/Packages.*)
########################################################################

PACKAGES = MYVRUI MYGLGEOMETRY MYGEOMETRY MYMATH MYCOMM MYTHREADS MYMISC GL

########################################################################
# Specify all final targets
########################################################################

CONFIGS = 
EXECUTABLES = 

CONFIGS += Config.h

EXECUTABLES += $(EXEDIR)/CalibrateProjector \
               $(EXEDIR)/SARndbox \
               $(EXEDIR)/SARndboxClient

ALL = $(EXECUTABLES)

.PHONY: all
all: $(CONFIGS) $(ALL)

########################################################################
# Pseudo-target to print configuration options and configure the package
########################################################################

.PHONY: config config-invalidate
config: config-invalidate $(DEPDIR)/config

config-invalidate:
	@mkdir -p $(DEPDIR)
	@touch $(DEPDIR)/Configure-Begin

$(DEPDIR)/Configure-Begin:
	@mkdir -p $(DEPDIR)
	@echo "---- $(PROJECT_FULLDISPLAYNAME) configuration options: ----"
	@touch $(DEPDIR)/Configure-Begin

$(DEPDIR)/Configure-SARndbox: $(DEPDIR)/Configure-Begin
	@cp Config.h.template Config.h.temp
	@$(call CONFIG_SETSTRINGVAR,Config.h.temp,CONFIG_CONFIGDIR,$(ETCINSTALLDIR))
	@$(call CONFIG_SETSTRINGVAR,Config.h.temp,CONFIG_SHADERDIR,$(SHADERINSTALLDIR))
	@if ! diff -qN Config.h.temp Config.h > /dev/null ; then cp Config.h.temp Config.h ; fi
	@rm Config.h.temp
	@touch $(DEPDIR)/Configure-SARndbox

$(DEPDIR)/Configure-Install: $(DEPDIR)/Configure-SARndbox
	@echo "---- $(PROJECT_FULLDISPLAYNAME) installation configuration ----"
	@echo "Installation directory : $(INSTALLDIR)"
	@echo "Executable directory   : $(EXECUTABLEINSTALLDIR)"
	@echo "Configuration directory: $(ETCINSTALLDIR)"
	@echo "Resource directory     : $(SHAREINSTALLDIR)"
	@echo "Shader directory       : $(SHADERINSTALLDIR)"
	@touch $(DEPDIR)/Configure-Install

$(DEPDIR)/Configure-End: $(DEPDIR)/Configure-Install
	@echo "---- End of $(PROJECT_FULLDISPLAYNAME) configuration options ----"
	@touch $(DEPDIR)/Configure-End

$(DEPDIR)/config: $(DEPDIR)/Configure-End
	@touch $(DEPDIR)/config

########################################################################
# Specify other actions to be performed on a `make clean'
########################################################################

.PHONY: extraclean
extraclean:

.PHONY: extrasqueakyclean
extrasqueakyclean:
	-rm -f $(ALL)
	-rm -r $(CONFIGS)

# Include basic makefile
include $(VRUI_MAKEDIR)/BasicMakefile

########################################################################
# Specify build rules for executables
########################################################################

#
# Calibration utility for Kinect 3D camera and projector:
#

CALIBRATEPROJECTOR_SOURCES = CalibrateProjector.cpp

$(CALIBRATEPROJECTOR_SOURCES:%.cpp=$(OBJDIR)/%.o): | $(DEPDIR)/config

$(EXEDIR)/CalibrateProjector: PACKAGES += MYKINECT MYIO
$(EXEDIR)/CalibrateProjector: $(CALIBRATEPROJECTOR_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: CalibrateProjector
CalibrateProjector: $(EXEDIR)/CalibrateProjector

#
# The Augmented Reality Sandbox:
#

SARNDBOX_SOURCES = FrameFilter.cpp \
                   TextureTracker.cpp \
                   ShaderHelper.cpp \
                   Shader.cpp \
                   DepthImageRenderer.cpp \
                   ElevationColorMap.cpp \
                   SurfaceRenderer.cpp \
                   WaterTable2.cpp \
                   PropertyGridCreator.cpp \
                   WaterRenderer.cpp \
                   HandExtractor.cpp \
                   HuffmanBuilder.cpp \
                   IntraFrameCompressor.cpp \
                   InterFrameCompressor.cpp \
                   RemoteServer.cpp \
                   GlobalWaterTool.cpp \
                   LocalWaterTool.cpp \
                   DEM.cpp \
                   DEMTool.cpp \
                   BathymetrySaverTool.cpp \
                   Sandbox.cpp

$(SARNDBOX_SOURCES:%.cpp=$(OBJDIR)/%.o): | $(DEPDIR)/config

$(EXEDIR)/SARndbox: PACKAGES += MYKINECT MYVIDEO MYGLMOTIF MYIMAGES MYGLSUPPORT MYGLWRAPPERS MYIO TIFF
$(EXEDIR)/SARndbox: $(SARNDBOX_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: SARndbox
SARndbox: $(EXEDIR)/SARndbox

#
# The Augmented Reality Sandbox remote client application:
#

SARNDBOXCLIENT_SOURCES = HuffmanBuilder.cpp \
                         IntraFrameDecompressor.cpp \
                         InterFrameDecompressor.cpp \
                         RemoteClient.cpp \
                         TextureTracker.cpp \
                         Shader.cpp \
                         ElevationColorMap.cpp \
                         SandboxClient.cpp

$(SARNDBOXCLIENT_SOURCES:%.cpp=$(OBJDIR)/%.o): | $(DEPDIR)/config

$(EXEDIR)/SARndboxClient: PACKAGES += MYGLSUPPORT MYGLWRAPPERS
$(EXEDIR)/SARndboxClient: $(SARNDBOXCLIENT_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: SARndboxClient
SARndboxClient: $(EXEDIR)/SARndboxClient

########################################################################
# Specify installation rules
########################################################################

install:
	@echo Installing $(PROJECT_FULLDISPLAYNAME) in $(INSTALLDIR)...
	@install -d $(INSTALLDIR)
	@echo Installing executables in $(EXECUTABLEINSTALLDIR)
	@install -d $(EXECUTABLEINSTALLDIR)
	@install $(EXECUTABLES) $(EXECUTABLEINSTALLDIR)
	@install -d $(ETCINSTALLDIR)
	@install -m u=rw,go=r $(ETCDIR)/* $(ETCINSTALLDIR)
	@install -d $(SHAREINSTALLDIR)
	@install -d $(SHADERINSTALLDIR)
	@install -m u=rw,go=r $(SHADERDIR)/* $(SHADERINSTALLDIR)
