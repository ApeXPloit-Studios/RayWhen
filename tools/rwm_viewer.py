#!/usr/bin/env python3
"""
RayWhen Map Viewer (RWM)
A Python tool to view RWM binary map files in text format using tkinter.
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import struct
from pathlib import Path

class RWMViewer:
    def __init__(self, root):
        self.root = root
        self.root.title("RayWhen Map Viewer (RWM)")
        self.root.geometry("800x600")
        
        # Map data storage
        self.current_map_data = None
        self.current_metadata = None
        
        self.setup_ui()
        
    def setup_ui(self):
        """Set up the user interface"""
        # Main frame
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)
        
        # File selection frame
        file_frame = ttk.LabelFrame(main_frame, text="Map File", padding="5")
        file_frame.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        file_frame.columnconfigure(1, weight=1)
        
        ttk.Button(file_frame, text="Browse Maps", command=self.browse_maps).grid(row=0, column=0, padx=(0, 10))
        
        self.file_var = tk.StringVar()
        self.file_entry = ttk.Entry(file_frame, textvariable=self.file_var, state="readonly")
        self.file_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), padx=(0, 10))
        
        ttk.Button(file_frame, text="Load Map", command=self.load_map).grid(row=0, column=2)
        
        # Metadata frame
        meta_frame = ttk.LabelFrame(main_frame, text="Map Metadata", padding="5")
        meta_frame.grid(row=1, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(0, 10))
        meta_frame.columnconfigure(1, weight=1)
        
        # Metadata labels
        ttk.Label(meta_frame, text="Name:").grid(row=0, column=0, sticky=tk.W, padx=(0, 10))
        self.name_var = tk.StringVar()
        ttk.Label(meta_frame, textvariable=self.name_var, foreground="blue").grid(row=0, column=1, sticky=tk.W)
        
        ttk.Label(meta_frame, text="Description:").grid(row=1, column=0, sticky=tk.W, padx=(0, 10))
        self.desc_var = tk.StringVar()
        ttk.Label(meta_frame, textvariable=self.desc_var, foreground="green").grid(row=1, column=1, sticky=tk.W)
        
        ttk.Label(meta_frame, text="Author:").grid(row=2, column=0, sticky=tk.W, padx=(0, 10))
        self.author_var = tk.StringVar()
        ttk.Label(meta_frame, textvariable=self.author_var, foreground="purple").grid(row=2, column=1, sticky=tk.W)
        
        # Map data display frame
        data_frame = ttk.LabelFrame(main_frame, text="Map Data", padding="5")
        data_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        data_frame.columnconfigure(0, weight=1)
        data_frame.rowconfigure(0, weight=1)
        
        # Text widget with scrollbar
        self.text_display = scrolledtext.ScrolledText(data_frame, width=80, height=20, 
                                                     font=("Courier", 10), wrap=tk.NONE)
        self.text_display.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready - Select a .rwm file to view")
        status_bar = ttk.Label(main_frame, textvariable=self.status_var, relief=tk.SUNKEN)
        status_bar.grid(row=3, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=(10, 0))
        
    def browse_maps(self):
        """Browse for RWM map files"""
        # Look for maps directory first
        maps_dir = "maps"
        if not os.path.exists(maps_dir):
            maps_dir = "."
            
        file_path = filedialog.askopenfilename(
            title="Select RWM Map File",
            initialdir=maps_dir,
            filetypes=[
                ("RayWhen Maps", "*.rwm"),
                ("All Files", "*.*")
            ]
        )
        
        if file_path:
            self.file_var.set(file_path)
            self.status_var.set(f"Selected: {os.path.basename(file_path)}")
            
    def load_map(self):
        """Load and parse the selected RWM map file"""
        file_path = self.file_var.get()
        
        if not file_path:
            messagebox.showerror("Error", "Please select a map file first")
            return
            
        if not os.path.exists(file_path):
            messagebox.showerror("Error", f"File not found: {file_path}")
            return
            
        try:
            self.parse_rwm_file(file_path)
            self.display_map_data()
            self.status_var.set(f"Loaded: {os.path.basename(file_path)}")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load map: {str(e)}")
            self.status_var.set("Error loading map")
            
    def parse_rwm_file(self, file_path):
        """Parse RWM binary file format"""
        with open(file_path, 'rb') as f:
            # Read header
            magic = f.read(3)
            if magic != b'RWM':
                raise ValueError("Invalid RWM file - missing magic number")
                
            version = struct.unpack('<B', f.read(1))[0]
            if version != 1:
                raise ValueError(f"Unsupported RWM version: {version}")
                
            width = struct.unpack('<H', f.read(2))[0]
            height = struct.unpack('<H', f.read(2))[0]
            
            if width != 16 or height != 16:
                raise ValueError(f"Unsupported map size: {width}x{height} (expected 16x16)")
                
            # Read metadata
            name_len = struct.unpack('<B', f.read(1))[0]
            map_name = f.read(name_len).decode('utf-8', errors='ignore') if name_len > 0 else "Untitled"
            
            desc_len = struct.unpack('<B', f.read(1))[0]
            description = f.read(desc_len).decode('utf-8', errors='ignore') if desc_len > 0 else "No description"
            
            author_len = struct.unpack('<B', f.read(1))[0]
            author = f.read(author_len).decode('utf-8', errors='ignore') if author_len > 0 else "Unknown"
            
            # Store metadata
            self.current_metadata = {
                'name': map_name,
                'description': description,
                'author': author,
                'width': width,
                'height': height,
                'version': version
            }
            
            # Read map data
            map_data = []
            for y in range(height):
                row = []
                for x in range(width):
                    wall_type = struct.unpack('<B', f.read(1))[0]
                    texture_id = struct.unpack('<B', f.read(1))[0]
                    floor_texture_id = struct.unpack('<B', f.read(1))[0]
                    
                    row.append({
                        'wall_type': wall_type,
                        'texture_id': texture_id,
                        'floor_texture_id': floor_texture_id
                    })
                map_data.append(row)
                
            self.current_map_data = map_data
            
    def display_map_data(self):
        """Display the loaded map data in text format"""
        if not self.current_map_data or not self.current_metadata:
            return
            
        # Update metadata display
        self.name_var.set(self.current_metadata['name'])
        self.desc_var.set(self.current_metadata['description'])
        self.author_var.set(self.current_metadata['author'])
        
        # Clear text display
        self.text_display.delete(1.0, tk.END)
        
        # Create text representation
        text_lines = []
        
        # Header information
        text_lines.append("=" * 60)
        text_lines.append(f"RAYWHEN MAP: {self.current_metadata['name']}")
        text_lines.append("=" * 60)
        text_lines.append(f"Description: {self.current_metadata['description']}")
        text_lines.append(f"Author: {self.current_metadata['author']}")
        text_lines.append(f"Size: {self.current_metadata['width']}x{self.current_metadata['height']}")
        text_lines.append(f"Version: {self.current_metadata['version']}")
        text_lines.append("")
        
        # Wall type legend
        text_lines.append("WALL TYPES:")
        text_lines.append("  0 = Empty (walkable)")
        text_lines.append("  1 = Red Wall")
        text_lines.append("  2 = Green Wall")
        text_lines.append("  3 = Blue Wall")
        text_lines.append("  4 = Yellow Wall")
        text_lines.append("  5 = Player Spawn")
        text_lines.append("  6 = Enemy Spawn")
        text_lines.append("")
        
        # Texture legend
        texture_names = [
            "Brick Red", "Stone Gray", "Metal Silver", "Wood Brown",
            "Tech Blue", "Rock Dark", "Brick Clay", "Metal Tile"
        ]
        
        text_lines.append("TEXTURE IDS:")
        for i, name in enumerate(texture_names):
            text_lines.append(f"  {i} = {name}")
        text_lines.append("")
        
        # Map data in old format style
        text_lines.append("MAP DATA (wallType:textureId:floorTextureId):")
        text_lines.append("=" * 60)
        
        for y, row in enumerate(self.current_map_data):
            line_parts = []
            for x, cell in enumerate(row):
                wall_type = cell['wall_type']
                texture_id = cell['texture_id']
                floor_texture_id = cell['floor_texture_id']
                line_parts.append(f"{wall_type}:{texture_id}:{floor_texture_id}")
            text_lines.append(" ".join(line_parts))
            
        text_lines.append("")
        text_lines.append("=" * 60)
        
        # Add some analysis
        text_lines.append("MAP ANALYSIS:")
        wall_counts = {}
        texture_counts = {}
        floor_texture_counts = {}
        player_spawns = []
        enemy_spawns = []
        
        for y, row in enumerate(self.current_map_data):
            for x, cell in enumerate(row):
                wall_type = cell['wall_type']
                texture_id = cell['texture_id']
                floor_texture_id = cell['floor_texture_id']
                
                wall_counts[wall_type] = wall_counts.get(wall_type, 0) + 1
                texture_counts[texture_id] = texture_counts.get(texture_id, 0) + 1
                floor_texture_counts[floor_texture_id] = floor_texture_counts.get(floor_texture_id, 0) + 1
                
                if wall_type == 5:
                    player_spawns.append(f"({x},{y})")
                elif wall_type == 6:
                    enemy_spawns.append(f"({x},{y})")
                    
        text_lines.append(f"Wall Types: {dict(wall_counts)}")
        text_lines.append(f"Wall Textures Used: {dict(texture_counts)}")
        text_lines.append(f"Floor Textures Used: {dict(floor_texture_counts)}")
        
        if player_spawns:
            text_lines.append(f"Player Spawns: {', '.join(player_spawns)}")
        else:
            text_lines.append("Player Spawns: None")
            
        if enemy_spawns:
            text_lines.append(f"Enemy Spawns: {', '.join(enemy_spawns)}")
        else:
            text_lines.append("Enemy Spawns: None")
            
        # Insert all text
        full_text = "\n".join(text_lines)
        self.text_display.insert(1.0, full_text)
        
        # Make text read-only
        self.text_display.config(state=tk.DISABLED)

def main():
    """Main function to run the RWM viewer"""
    root = tk.Tk()
    app = RWMViewer(root)
    
    # Try to auto-load maps directory if it exists
    maps_dir = "maps"
    if os.path.exists(maps_dir):
        rwm_files = list(Path(maps_dir).glob("*.rwm"))
        if rwm_files:
            # Auto-select the first RWM file found
            first_file = str(rwm_files[0])
            app.file_var.set(first_file)
            app.status_var.set(f"Found maps directory - {len(rwm_files)} RWM files available")
    
    root.mainloop()

if __name__ == "__main__":
    main()
