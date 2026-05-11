import os
import glob
import re

def main():
    src_dir = r"c:\Users\kaito\source\repos\KTEngine\src"
    files = glob.glob(os.path.join(src_dir, "**", "*.cpp"), recursive=True) + \
            glob.glob(os.path.join(src_dir, "**", "*.h"), recursive=True)
    
    for filepath in files:
        if "RendererDX11." in filepath or "RendererDX12." in filepath or "Renderer." in filepath:
            continue
        
        try:
            with open(filepath, 'r', encoding='cp932') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            continue
            
        original_content = content
        
        # Replace includes
        content = re.sub(r'#include\s+"RendererDX11\.h"', '#include "Renderer.h"', content)
        
        # Replace context/device calls
        content = content.replace("RendererDX11::GetContext()->", "Renderer::")
        content = content.replace("RendererDX11::GetDevice()->", "Renderer::")
        
        # Replace any remaining RendererDX11:: with Renderer::
        content = content.replace("RendererDX11::", "Renderer::")
        
        if content != original_content:
            print(f"Refactoring {os.path.basename(filepath)}")
            with open(filepath, 'w', encoding='cp932') as f:
                f.write(content)

if __name__ == "__main__":
    main()
