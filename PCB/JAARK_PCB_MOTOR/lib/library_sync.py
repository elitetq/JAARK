import os
import glob
import re

# ================= CONFIGURATION =================
# 1. Your footprint folder name (The folder ending in .pretty)
library_folder = "SamacSys_Parts.pretty"  # Updated based on your image path

# 2. The folder where you want the 3D models to live (inside your project)
relative_3d_folder = "lib/SamacSys_Parts.3dshapes"

# =================================================

def fix_footprints():
    # Get all .kicad_mod files
    footprints = glob.glob(os.path.join(library_folder, "*.kicad_mod"))
    
    print(f"Scanning {len(footprints)} footprints in '{library_folder}'...")
    
    fixed_count = 0
    
    # Regex to find: (model "ANYTHING/filename.step"
    # It handles double backslashes (\\) or forward slashes (/)
    # It captures the filename in Group 1
    pattern = r'\(model\s+"(?:.*[\\/])?([^\\/"]+\.(?:step|stp|wrl))"'
    
    # Replacement: (model "${KIPRJMOD}/YourFolder/filename.step"
    replacement = f'(model "${{KIPRJMOD}}/{relative_3d_folder}/\\1"'

    for file_path in footprints:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # We perform the substitution on the content
        new_content = re.sub(pattern, replacement, content, flags=re.IGNORECASE)
        
        # If the content changed (meaning a match was found and replaced)
        if new_content != content:
            with open(file_path, 'w') as f:
                f.write(new_content)
            
            # Print specifically what file was fixed for confirmation
            filename = os.path.basename(file_path)
            print(f"Fixed: {filename}")
            fixed_count += 1

    print(f"\nSummary: Updated {fixed_count} footprints.")
    if fixed_count == 0:
        print("Warning: No files changed. Check if 'library_folder' name matches your folder exactly.")

if __name__ == "__main__":
    fix_footprints()