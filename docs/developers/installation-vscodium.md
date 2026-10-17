# Developer´s Installation with PlatformIO

## Installation of VSCodium (Visual Studio Code)

I prefer the installation of VSCodium, it is an excellent, privacy-focused alternative to Microsoft Visual Studio (VS Code) as it completely eliminates telemetry, tracking, and proprietary Microsoft licenses.

However, setting up VSCodium with PlatformIO introduces a major technical hurdle:
Microsoft explicitly restricts the use of its official C/C++ extension (ms-vscode.cpptools) to official Microsoft product distributions.
Because PlatformIO relies heavily on this extension for code completion, debugging, and navigation, you cannot simply download everything directly from VSCodium's default extension marketplace.

The following guide walks you through the proven workflows to successfully install and run PlatformIO on VSCodium.

## Method 1: Manual Extension Installation (Recommended for Privacy)
This method keeps your VSCodium completely decoupled from Microsoft's marketplace servers by manually sideloading the extension files (.vsix).

### 1. Check System Prerequisites

Ensure **Python 3** and **Git** are installed on your system.
PlatformIO uses them under the hood to manage libraries and compile code.

* Linux Users: You must also install the `python3-venv` package via your package manager (e.g. `sudo apt install python3-venv`).
On **Arch Linux** the core `python` package (installed via `sudo pacman -S python`) **already includes the standard library `venv` module by default**.

### 2. Download the Required Extensions

Because of license restrictions, the required versions are not available on VSCodium's default *Open-VSX* registry.
You need to grab them from the official Visual Studio Marketplace:

1. Go to the Microsoft C/C++ Extension Page.
On the right-hand side under *Resources*, click **Download Extensions** to save the `.vsix` file.

2. Go to the PlatformIO IDE Extension Page and download its `.vsix` file as well.

### 3. Sideload into VSCodium

1. Open VSCodium.

2. Click on the **Extension** icon in the left Activity Bar (or press `Ctrl+Shift+X` / `Cmd+Shift+X`).

3. Click the `three dots (...)` at the top right of the Extensions view.

4. Select and install the C/C++ Tools `.vsix` file first.

5. Repeat the process to install the PlatformIO IDE `.vsix` file.

### 4. Initialize PlatformIO Core

1. **Restart VSCodium** completely after both installations finish.

2. Upon restarting, PlatformIO will automatically start installing the **PlatformIO Core** framework in the background.
This may take a few minutes depending on your internet connection.

3. Once completed, you will see the iconic **PlatformIO Alien/Ant icon** appear in your left sidebar.

## Method 2: Switching to the Microsoft Marketplace (Easier Updates)

If you prefer a seamless one-click installation and automatic updates directly inside VSCodium, you can modify VSCodium's configuration to pull extensions directly from Microsoft's official marketplace instead of Open-VSX.

*Note: This connects your editor to Microsoft's gallery infrastructure, which slightly reduces the strict "no-Microsoft-servers" privacy model of VSCodium.*

1. Locate and open the product.json file for your VSCodium installation.

	• Windows: %APPDATA%\VSCodium or inside the installation directory.
	• Linux: /usr/share/vscodium/resources/app/product.json (requires root privileges to edit) or inside your Flatpak/Snap path.
	• macOS: /Applications/VSCodium.app/Contents/Resources/app/product.json.

2. Look for the extensionsGallery block and replace it with the following configuration:
```json
json
"extensionsGallery": {
    "serviceUrl": "https://visualstudio.com",
    "itemUrl": "https://visualstudio.com"
}
```

3. Save the file and restart VSCodium.

4. You can now simply search for PlatformIO IDE and C/C++ in the Extensions tab and click Install.
