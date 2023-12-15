# EntitySlayer
EntitySlayer is a WIP editor for DOOM Eternal's .entities files.  

Beginning originally as a bug fix and QoL update for [EntityHero](https://github.com/nopjne/EntityHero/tree/master), it quickly became a separate application containing only a few core components of the original codebase. It aims to overhaul the editing experience provided by EntityHero while maintaining a familiar interface for frequent users of this tool.

### Installation
EntitySlayer is written in C++17 using [wxWidgets](https://www.wxwidgets.org/) 3.1.4 as it's GUI library. You will need [Microsoft's Visual Studio](https://visualstudio.microsoft.com/) to work with the project files.

To get this project operational on Windows 10:
1. Clone this repository
2. Download pre-built binaries for wxWidgets 3.1.4: https://github.com/nopjne/EntityHero/releases/download/v0.4/wxWidget.zip
3. Extract the contents of the downloaded zip file into your cloned repository folder.
4. Open the solution file in Visual Studio and build the project.

Instead of downloading pre-built binaries for wxWidgets, you may opt to build the library yourself. Any version numbered 3.1.4 or higher should suffice. If you wish to do this, I highly recommend the following tutorial: https://www.youtube.com/watch?v=ONYW3hBbk-8

### Credits
* FlavorfulGecko5 - Author of EntitySlayer
* Scorp0rX0r - original author of [EntityHero](https://github.com/nopjne/EntityHero/tree/master)
* Alveraan - Author of [Elena](https://github.com/alveraan/elena), another GUI application for viewing and editing .entities files. The filtering systems used in EntitySlayer were first included in this editor.
* Chrispy - Developer of [Meathook](https://github.com/brongo/m3337ho0o0ok/releases/tag/v7.1)
* Wyo - Developer of [wxWidgets/samples/stc](https://github.com/wxWidgets/wxWidgets/tree/master/samples/stc) - this program is used as the basis for the text editor in both EntityHero and EntitySlayer. 
