# Structure of this plugin
In the following a brief overview of the UDShaper source files and their role is given.

## UDShaper.h
The UDShaper class declared here is the main plugin class. It is mainly responsible for storing references to the clap extensions and to subelements that enable interaction with the user.

## UDShaperElements.h
The UDShaper elements that build the funcitonality of the plugin are all derived from the class InteractiveGUIElement (see assets.h). They are assigned a fixed area on the plugin window on which they can draw their own interface. They further have methods to process user inputs.\
Their definition is platform independent, user input is forwarded as uint32_t coordinates on the window at the time of a left click/ right click/ mouse release etc.

## GUILayout.h
Every UDShaper element has a correspndong layout struct. It stores box coordinates (XYXY notation) that define how the element renders its content. They can indicate the possition of knobs, buttons, graph editors etc. It also defines a method to update the boxes based on the available space.

## GUI.h
GUI.h declares the GUI struct, as well as functions connecting the plugin and Windows. They handle creation and deletion of the plugin window and set the window procedure. User inputs are handled inside the procedure and forwarded to the UDShaper instance through the methods `UDShaper::processLeftClick(x, y)`, `UDShaper::processRightClick(x, y)`, etc.

## contextMenus.h
Context menus are handled in this file. The appearance and contents of context menus is defined in contextMenus.cpp. The declared MenuRequest class communicates between UDShaperElement.cpp and gui_w32.cpp:
- The static method `MenuRequest::sendRequest(*owner, type)` can be called from anywhere in the code and registers what type of menu has to be opened.
- `MenuRequest::showMenu(window, x, y)` has to be called from inside the window procedure where the window object is available. It will display the recently requested menu.
- `MenuRequest::processSelection(menuItem)` forwards the selected option to the owner of the request.

## assets.h
assets.h contains a collection of functions that draw simple elements to a uint32_t array, as for example rectangles, frames, points, circles, etc. It also declares the abstract class InteracteGUIElements all UDshaper elements derive from.

## logging.h
Declares the Logger class, which can be used to log debug messages to the host. If the host does not support the CLAP logging feature, it can alternatively log a message to a text file. Update the file path in logger.cpp to make it work on your machine.
