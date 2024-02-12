# EntitySlayer
EntitySlayer is an editor for DOOM Eternal's .entities files.  

Beginning originally as a bug fix and QoL update for [EntityHero](https://github.com/nopjne/EntityHero/tree/master), it quickly became a separate application containing only a few core components of the original codebase. It aims to overhaul the editing experience provided by EntityHero while maintaining a familiar interface for frequent users of this tool.

The current list of features includes:
* Open multiple .entities files simultaneously and toggle Oodle compression on/off.
* A filter menu inspired by Elena, to help you quickly find the entities you're looking for.
* Undo/Redo support with visual feedback on what changes were made.
* Copy nodes to your clipboard as text, and paste text from clipboard directly into the node tree.
* Multi-selection of nodes to enable mass-copying, deleting, or other actions.
* Automatic idList renumbering
* Meathook integration - reload levels using the files you're editing, and quickly set the spawnPosition and spawnOrientation of entities based on your player character's position
* A custom-made, very fast .entities parser with support for single-line comments and accurate error detection
* Uses **90% less memory** than EntityHero.
* **No crashes, no data corruption, no broken features**

### Installation
1. Download the latest release from the [Releases Page](https://github.com/FlavorfulGecko5/EntitySlayer/releases)
2. Place the zip contents into your DOOM Eternal installation folder
3. Install [Meathook](https://github.com/brongo/m3337ho0o0ok/releases/tag/v7.2) to make use of every feature EntitySlayer has to offer.

### Troubleshooting and Common Questions

> How does the filter system work?

1. The drop down menu let's you check/uncheck options. Entities that meet none of the selected criteria are filtered-out.
2. The text box lets you quickly enter a filter option to toggle it on/off, instead of scrolling through the checklist to find it.
3. To add options to the Text Key filter, type your custom text key into it's text box and press `Enter`
4. Spawn Position Distance filters entities based on their spawnPosition's distance from a point. Use the `xyz` text boxes to enter a coordinate, and `r` to enter the radius. Any entities beyond the radius from this point are filtered-out. Use the checkbox to toggle spawn filtering on/off.
5. If you've added a new layer/class/inheritance value that wasn't previously in the file, use the `Refresh Filter Lists` button to make it appear in it's respective checklist

> The Text Key Filter and Search Bar don't seem to be working?

For these features, use input strings like `globalAIsettings"default"` instead of `globalAIsettings = "default";`

> Meathook is reloading the map, but none of the changes I made to the entities file are appearing?

The `Use as Reload Tab` option **must be set AFTER loading into the level you want to edit.**

> Selecting an option from the append menu is slow! Is there a way to speed things up?

Edit `EntitySlayer_Config.txt` - you can set a custom hotkey for every item in the append menu!

> Can `EntitySlayer_Config.txt` be edited in EntitySlayer?

Yes!

> Navigating the Append Parameter menu with a mouse is slow! How can I do it more efficiently?

Use `Tab` to quickly navigate between the different textboxes, checkboxes and buttons! Use `Spacebar` to check/uncheck a checkbox or activate a button!

### Contributing
EntitySlayer is written in C++17 using [wxWidgets](https://www.wxwidgets.org/) 3.1.4 as it's GUI library. You will need [Microsoft's Visual Studio](https://visualstudio.microsoft.com/) to work with the project files.

To get this project operational on Windows 10:
1. Clone this repository
2. Download pre-built binaries for wxWidgets 3.1.4: https://github.com/nopjne/EntityHero/releases/download/v0.4/wxWidget.zip
3. Extract the contents of the downloaded zip file into your cloned repository folder.
4. Open the solution file in Visual Studio and build the project.

Instead of downloading pre-built binaries for wxWidgets, you may opt to build the library yourself. Any version numbered 3.1.4 or higher should suffice. If you wish to do this, I highly recommend the following tutorial: https://www.youtube.com/watch?v=ONYW3hBbk-8

### Credits
* FlavorfulGecko5 - Author of EntitySlayer
* Velser - Extensive Alpha testing, feedback and feature suggestions
* Scorp0rX0r - original author of [EntityHero](https://github.com/nopjne/EntityHero/tree/master)
* Alveraan - Author of [Elena](https://github.com/alveraan/elena), another GUI application for viewing and editing .entities files. The filtering systems used in EntitySlayer were first included in this editor.
* Chrispy - Developer of [Meathook](https://github.com/brongo/m3337ho0o0ok/releases/tag/v7.1)
* Wyo - Developer of [wxWidgets/samples/stc](https://github.com/wxWidgets/wxWidgets/tree/master/samples/stc) - this program is used as the basis for the text editor in both EntityHero and EntitySlayer. 
