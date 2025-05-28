import tkinter as tk
from tkinter import filedialog, messagebox
import json

TEMPLATE_HEADER = '''#include "level.h"
#include "../src/decomp/include/surface_terrains.h"

const struct SM64Surface surfaces[] = {
'''

TEMPLATE_FOOTER = '''};

const size_t surfaces_count = sizeof(surfaces) / sizeof(surfaces[0]);
'''

def generate_level_c(json_path, output_path):
    with open(json_path, 'r') as f:
        triangles = json.load(f)

    lines = [TEMPLATE_HEADER]

    for tri in triangles:
        formatted = ','.join([f'{{{int(x)},{int(y)},{int(z)}}}' for x, y, z in tri])
        line = f'{{SURFACE_DEFAULT,0,TERRAIN_SNOW,{{{formatted}}}}},\n'
        lines.append(line)

    lines.append(TEMPLATE_FOOTER)

    with open(output_path, 'w') as f:
        f.writelines(lines)

def select_file_and_generate():
    input_path = filedialog.askopenfilename(
        title="Select Triangle JSON File",
        filetypes=[("JSON Files", "*.json")]
    )
    if not input_path:
        return

    output_path = filedialog.asksaveasfilename(
        title="Save level.c File",
        defaultextension=".c",
        filetypes=[("C Source File", "*.c")]
    )
    if not output_path:
        return

    try:
        generate_level_c(input_path, output_path)
        messagebox.showinfo("Success", f"level.c generated at:\n{output_path}")
    except Exception as e:
        messagebox.showerror("Error", f"Failed to generate level.c:\n{e}")

def main():
    root = tk.Tk()
    root.title("Generate level.c from Triangle JSON")
    root.geometry("400x200")

    label = tk.Label(root, text="Select triangle JSON to convert to level.c", pady=20)
    label.pack()

    btn = tk.Button(root, text="Select File and Generate", command=select_file_and_generate, height=2, width=25)
    btn.pack(pady=10)

    root.mainloop()

if __name__ == "__main__":
    main()
