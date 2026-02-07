import os
import shutil
import re

# Paths
PROJECT_ROOT = os.getcwd()
DX12_DOCS_SOURCE = os.path.join(PROJECT_ROOT, "VibeDX12Renderer", "Docs")
ASSETS_SOURCE = os.path.join(PROJECT_ROOT, "Assets")
SITE_SOURCE = os.path.join(PROJECT_ROOT, "site_source")

def main():
    print(f"Preparing documentation site in: {SITE_SOURCE}")

    # 1. Clean and Create Target Directory
    if os.path.exists(SITE_SOURCE):
        shutil.rmtree(SITE_SOURCE)
    os.makedirs(SITE_SOURCE)

    # 2. Setup Substructure
    dx12_target = os.path.join(SITE_SOURCE, "VibeDX12Renderer")
    os.makedirs(dx12_target)
    
    # Placeholder for Vulkan
    vulkan_target = os.path.join(SITE_SOURCE, "VibeVulkanRenderer")
    os.makedirs(vulkan_target)
    with open(os.path.join(vulkan_target, "index.md"), "w", encoding="utf-8") as f:
        f.write("# Vibe Vulkan Renderer\n\nComing Soon...")

    # 3. Copy DX12 Docs
    print("Copying DX12 documentation files...")
    for item in os.listdir(DX12_DOCS_SOURCE):
        s = os.path.join(DX12_DOCS_SOURCE, item)
        d = os.path.join(dx12_target, item)
        if os.path.isfile(s):
            shutil.copy2(s, d)
        elif os.path.isdir(s):
            shutil.copytree(s, d)

    # 4. Copy Assets (Rename to lowercase 'assets' to match MkDocs theme structure)
    print("Copying assets...")
    # MkDocs Material theme uses 'assets' (lowercase) for its CSS/JS.
    # On Windows (case-insensitive), 'Assets' and 'assets' merge, causing no local issues.
    # On GitHub Pages (Linux, case-sensitive), if we ship 'Assets', links to 'assets/css/...' FAIL (404).
    # Solution: Normalize our user content to 'assets' (lowercase) in the build output.
    assets_target = os.path.join(SITE_SOURCE, "assets")
    shutil.copytree(ASSETS_SOURCE, assets_target)

    # 5. Copy Main README as Index
    print("Copying main README...")
    shutil.copy2(os.path.join(PROJECT_ROOT, "README.md"), os.path.join(SITE_SOURCE, "index.md"))

    # Add .nojekyll to prevent GitHub Pages from ignoring files starting with _
    # This is a common cause of broken styling on GitHub Pages
    print("Creating .nojekyll file...")
    with open(os.path.join(SITE_SOURCE, ".nojekyll"), "w") as f:
        f.write("")

    # 6. Process Markdown Files (Fix Links)
    print("Processing links in markdown files...")
    
    for root, dirs, files in os.walk(SITE_SOURCE):
        for file in files:
            if file.endswith(".md"):
                file_path = os.path.join(root, file)
                
                with open(file_path, "r", encoding="utf-8") as f:
                    content = f.read()

                new_content = content
                
                # Logic for files inside VibeDX12Renderer subdirectory
                if "VibeDX12Renderer" in root:
                    # Rename README.md to index.md in subfolder for proper navigation behavior
                    if file == "README.md":
                        # This becomes VibeDX12Renderer/index.md
                        # Links inside it point to 01_Hello.md which are in same folder, so OK.
                        pass # renaming happens below
                    
                    # Fix Asset Links: ../../Assets -> ../assets (LOWERCASE)
                    new_content = new_content.replace("../../Assets/", "../assets/")
                    # Also catch any that might have been single parent
                    new_content = new_content.replace("../Assets/", "../assets/")
                
                elif file == "index.md" and root == SITE_SOURCE:
                    # Root index.md (Main README) -> Fix Assets/ to assets/
                    new_content = new_content.replace("Assets/", "assets/")

                # Rename logic for READMEs
                final_path = file_path
                if file == "README.md":
                    final_path = os.path.join(root, "index.md")
                    if os.path.exists(file_path): # Ensure we don't duplicate if already exists (shouldn't happen with clean copy)
                        os.remove(file_path)
                
                with open(final_path, "w", encoding="utf-8") as f:
                    f.write(new_content)
                    print(f"Processed {final_path}")

    print("Site source preparation complete.")

if __name__ == "__main__":
    main()
