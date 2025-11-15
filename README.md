# UDShaper
UDShaper is a [wave shaper](https://en.wikipedia.org/wiki/Waveshaper) audio effect that features two shaping functions which can be applied to the input audio using different mappings. The shaping functions can be designed using a visual graph editor and all their parameters can be controlled by the built-in LFO tool.

![Current GUI](images/UDShaper_GUI.png)
*The current state of the UDShaper GUI. The two graph editors on the left define the shaping functions, the Envelope tool one on the right is used to modulate their parameters over time. Check the manual for a detailed guide.*

## Building the plugin
UDShaper uses the iPlug2 framework and can be built in-source.\
First, follow the [instructions to set up iPlug2](https://github.com/iPlug2/iPlug2/wiki/02_Getting_started_windows) and make sure to install the prebuilt libraries to be able to use the SKIA grahics backend.
Create a projects folder in your iPlug2 folder and clone this repository there. You will need Microsoft Visual Studio (I use VS 2022 version 17.14.20) with the C++ dev kit to build the project.
You should be able to run the UDShaper app directly from Visual Studio and compile the UDShaper-clap solution. The .clap file will appear in the folder UDShaper/build.\
If you prefer to copy the file directly to 'Common Files/CLAP' go to UDShaper-clap > properties > Build Events > Post-Build Event. From there replace "$(SolutionDir)build" with "CLAP_PATH" in the Command Line box. You might have to convince your OS that Visual Studio has admin rights.

## Compatibility
I originally designed UDShaper as a CLAP plugin, but thanks to iPlug2 it should be easy to compile it as VST as well but I did not take care of that. I hope that more DAWs will support CLAP in the future. A list of hosts supporting CLAP can be found [here](https://clapdb.tech/category/hostsdaws).\
Currently, the plugin is under development and not yet tested for any hosts or systems apart from FL Studio on windows.