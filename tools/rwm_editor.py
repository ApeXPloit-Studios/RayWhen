#!/usr/bin/env python3
"""
RayWhen Map Editor (RWM)
A comprehensive Python map editor for RayWhen RWM files using tkinter.
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, colorchooser
import os
import struct
from pathlib import Path
import json

class RWMEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("RayWhen Map Editor - Advanced")
        self.root.geometry("1200x800")
        
        # Map data storage
        self.map_data = [[{'wall_type': 0, 'texture_id': 0, 'floor_texture_id': 0} for _ in range(16)] for _ in range(16)]
        self.current_file = None
        self.undo_stack = []
        self.redo_stack = []
        
        # Editor state
        self.current_tool = "paint"  # paint, player_spawn, enemy_spawn, fill
        self.current_wall_type = 1
        self.current_texture_id = 0
        self.current_floor_texture_id = 0
        self.is_dragging = False
        
        # Metadata
        self.map_name = "Untitled Map"
        self.map_description = "A RayWhen map"
        self.map_author = "Unknown"
        
        self.setup_ui()
        self.load_default_map()
        
    def setup_ui(self):
        """Set up the user interface"""
        # Main container
        main_container = ttk.Frame(self.root)
        main_container.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Configure grid weights
        main_container.columnconfigure(1, weight=1)
        main_container.rowconfigure(1, weight=1)
        
        # Top toolbar
        self.setup_toolbar(main_container)
        
        # Left panel - tools and properties
        self.setup_left_panel(main_container)
        
        # Center - map editor
        self.setup_map_editor(main_container)
        
        # Right panel - metadata and info
        self.setup_right_panel(main_container)
        
        # Bottom status bar
        self.setup_status_bar(main_container)
        
    def setup_toolbar(self, parent):
        """Set up the top toolbar"""
        toolbar = ttk.Frame(parent)
        toolbar.grid(row=0, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 5))
        
        # File operations
        ttk.Button(toolbar, text="New", command=self.new_map).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="Open", command=self.open_map).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="Save", command=self.save_map).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="Save As", command=self.save_map_as).pack(side=tk.LEFT, padx=(0, 5))
        
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # Edit operations
        ttk.Button(toolbar, text="Undo", command=self.undo).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="Redo", command=self.redo).pack(side=tk.LEFT, padx=(0, 5))
        
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # Tools
        self.tool_var = tk.StringVar(value="paint")
        ttk.Radiobutton(toolbar, text="Paint", variable=self.tool_var, value="paint").pack(side=tk.LEFT, padx=(0, 5))
        ttk.Radiobutton(toolbar, text="Player", variable=self.tool_var, value="player_spawn").pack(side=tk.LEFT, padx=(0, 5))
        ttk.Radiobutton(toolbar, text="Enemy", variable=self.tool_var, value="enemy_spawn").pack(side=tk.LEFT, padx=(0, 5))
        ttk.Radiobutton(toolbar, text="Fill", variable=self.tool_var, value="fill").pack(side=tk.LEFT, padx=(0, 5))
        
    def setup_left_panel(self, parent):
        """Set up the left panel with tools and properties"""
        left_panel = ttk.LabelFrame(parent, text="Tools & Properties", padding="5")
        left_panel.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(0, 5))
        
        # Wall type selection
        ttk.Label(left_panel, text="Wall Type:").pack(anchor=tk.W)
        self.wall_type_var = tk.StringVar(value="1")
        wall_frame = ttk.Frame(left_panel)
        wall_frame.pack(fill=tk.X, pady=(0, 10))
        
        wall_types = [
            ("Empty", "0", "Walkable space"),
            ("Red Wall", "1", "Red colored wall"),
            ("Green Wall", "2", "Green colored wall"),
            ("Blue Wall", "3", "Blue colored wall"),
            ("Yellow Wall", "4", "Yellow colored wall")
        ]
        
        for name, value, desc in wall_types:
            rb = ttk.Radiobutton(wall_frame, text=name, variable=self.wall_type_var, value=value,
                                command=self.update_wall_type)
            rb.pack(anchor=tk.W)
            
        # Texture selection
        ttk.Label(left_panel, text="Wall Texture:").pack(anchor=tk.W, pady=(10, 0))
        self.texture_var = tk.StringVar(value="Brick Red")
        self.texture_combo = ttk.Combobox(left_panel, textvariable=self.texture_var, state="readonly")
        self.texture_combo['values'] = [
            "Brick Red", "Stone Gray", "Metal Silver", "Wood Brown",
            "Tech Blue", "Rock Dark", "Brick Clay", "Metal Tile",
            "Gray Wall", "Dark Wood", "Hexagons", "Dirt",
            "Big Bricks", "Storage", "Pavement", "Wood Tile"
        ]
        self.texture_combo.pack(fill=tk.X, pady=(0, 10))
        self.texture_combo.bind('<<ComboboxSelected>>', self.update_texture)
        
        # Floor texture selection
        ttk.Label(left_panel, text="Floor Texture:").pack(anchor=tk.W)
        self.floor_texture_var = tk.StringVar(value="Red Bricks")
        self.floor_texture_combo = ttk.Combobox(left_panel, textvariable=self.floor_texture_var, state="readonly")
        self.floor_texture_combo['values'] = [
            "Red Bricks", "Building Bricks", "Metal Tile", "Wood A",
            "High Tech", "Gray Rocks", "Clay Bricks", "Cross Wall",
            "Gray Wall", "Dark Wood", "Hexagons", "Dirt",
            "Big Bricks", "Storage", "Pavement", "Wood Tile"
        ]
        self.floor_texture_combo.pack(fill=tk.X, pady=(0, 10))
        self.floor_texture_combo.bind('<<ComboboxSelected>>', self.update_floor_texture)
        
        # Texture preview
        preview_frame = ttk.LabelFrame(left_panel, text="Texture Preview", padding="5")
        preview_frame.pack(fill=tk.X, pady=(10, 0))
        
        self.preview_canvas = tk.Canvas(preview_frame, width=200, height=60, bg="white")
        self.preview_canvas.pack(fill=tk.X, pady=(0, 5))
        
        # Update preview when textures change
        self.texture_combo.bind('<<ComboboxSelected>>', self.update_texture_preview)
        self.floor_texture_combo.bind('<<ComboboxSelected>>', self.update_texture_preview)
        
        # Map info
        info_frame = ttk.LabelFrame(left_panel, text="Map Info", padding="5")
        info_frame.pack(fill=tk.X, pady=(10, 0))
        
        self.info_text = tk.Text(info_frame, height=6, width=25, font=("Courier", 9))
        self.info_text.pack(fill=tk.BOTH, expand=True)
        
    def setup_map_editor(self, parent):
        """Set up the map editor canvas"""
        editor_frame = ttk.LabelFrame(parent, text="Map Editor", padding="5")
        editor_frame.grid(row=1, column=1, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Canvas for map
        self.canvas = tk.Canvas(editor_frame, width=512, height=512, bg="black", 
                               relief=tk.SUNKEN, bd=2)
        self.canvas.pack(expand=True, fill=tk.BOTH)
        
        # Bind events
        self.canvas.bind("<Button-1>", self.on_canvas_click)
        self.canvas.bind("<B1-Motion>", self.on_canvas_drag)
        self.canvas.bind("<ButtonRelease-1>", self.on_canvas_release)
        
        # Draw grid
        self.draw_map()
        
    def setup_right_panel(self, parent):
        """Set up the right panel with metadata"""
        right_panel = ttk.LabelFrame(parent, text="Map Metadata", padding="5")
        right_panel.grid(row=1, column=2, sticky=(tk.W, tk.E, tk.N, tk.S), padx=(5, 0))
        
        # Map name
        ttk.Label(right_panel, text="Map Name:").pack(anchor=tk.W)
        self.name_entry = ttk.Entry(right_panel, width=25)
        self.name_entry.pack(fill=tk.X, pady=(0, 10))
        self.name_entry.insert(0, self.map_name)
        self.name_entry.bind('<KeyRelease>', self.update_metadata)
        
        # Description
        ttk.Label(right_panel, text="Description:").pack(anchor=tk.W)
        self.desc_text = tk.Text(right_panel, height=3, width=25)
        self.desc_text.pack(fill=tk.X, pady=(0, 10))
        self.desc_text.insert(1.0, self.map_description)
        self.desc_text.bind('<KeyRelease>', self.update_metadata)
        
        # Author
        ttk.Label(right_panel, text="Author:").pack(anchor=tk.W)
        self.author_entry = ttk.Entry(right_panel, width=25)
        self.author_entry.pack(fill=tk.X, pady=(0, 10))
        self.author_entry.insert(0, self.map_author)
        self.author_entry.bind('<KeyRelease>', self.update_metadata)
        
        # Map statistics
        stats_frame = ttk.LabelFrame(right_panel, text="Map Statistics", padding="5")
        stats_frame.pack(fill=tk.X, pady=(10, 0))
        
        self.stats_text = tk.Text(stats_frame, height=8, width=25, font=("Courier", 9))
        self.stats_text.pack(fill=tk.BOTH, expand=True)
        
        self.update_statistics()
        
    def setup_status_bar(self, parent):
        """Set up the status bar"""
        status_frame = ttk.Frame(parent)
        status_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(5, 0))
        
        self.status_var = tk.StringVar(value="Ready")
        status_label = ttk.Label(status_frame, textvariable=self.status_var, relief=tk.SUNKEN)
        status_label.pack(fill=tk.X)
        
    def draw_map(self):
        """Draw the map on the canvas"""
        self.canvas.delete("all")
        
        # Calculate cell size
        canvas_width = self.canvas.winfo_width()
        canvas_height = self.canvas.winfo_height()
        cell_size = min(canvas_width // 16, canvas_height // 16)
        
        if cell_size < 1:
            cell_size = 32  # Default size
            
        # Draw grid
        for y in range(17):  # 17 lines for 16 cells
            y_pos = y * cell_size
            self.canvas.create_line(0, y_pos, 16 * cell_size, y_pos, fill="gray50")
            
        for x in range(17):
            x_pos = x * cell_size
            self.canvas.create_line(x_pos, 0, x_pos, 16 * cell_size, fill="gray50")
            
        # Draw cells
        for y in range(16):
            for x in range(16):
                cell = self.map_data[y][x]
                x_pos = x * cell_size
                y_pos = y * cell_size
                
                # Get color based on wall type
                color = self.get_cell_color(cell)
                
                # Draw cell
                rect_id = self.canvas.create_rectangle(
                    x_pos + 1, y_pos + 1, x_pos + cell_size - 1, y_pos + cell_size - 1,
                    fill=color, outline="black", width=1
                )
                
                # Add special markers
                if cell['wall_type'] == 5:  # Player spawn
                    self.canvas.create_text(x_pos + cell_size//2, y_pos + cell_size//2, 
                                         text="P", fill="yellow", font=("Arial", 12, "bold"))
                elif cell['wall_type'] == 6:  # Enemy spawn
                    self.canvas.create_text(x_pos + cell_size//2, y_pos + cell_size//2, 
                                         text="E", fill="red", font=("Arial", 12, "bold"))
                
                # Store cell reference
                self.canvas.tag_bind(rect_id, "<Button-1>", lambda e, x=x, y=y: self.on_cell_click(x, y))
                
    def get_texture_colors(self):
        """Get unique colors for each texture with wall/floor variants"""
        # Base colors for each texture (16 unique colors)
        base_colors = [
            "#B4503C",  # 0 - Brick Red
            "#787878",  # 1 - Stone Gray  
            "#A9A9A9",  # 2 - Metal Silver
            "#8B4513",  # 3 - Wood Brown
            "#6496C8",  # 4 - Tech Blue
            "#5A5A5A",  # 5 - Rock Dark
            "#8C3C32",  # 6 - Brick Clay
            "#5050A0",  # 7 - Metal Tile
            "#808080",  # 8 - Gray Wall
            "#644623",  # 9 - Dark Wood
            "#5078A0",  # 10 - Hexagons
            "#785050",  # 11 - Dirt
            "#AA5A46",  # 12 - Big Bricks
            "#6E6E78",  # 13 - Storage
            "#646464",  # 14 - Pavement
            "#967850"   # 15 - Wood Tile
        ]
        
        # Wall variants (slightly darker/more saturated)
        wall_colors = [
            "#C4503C",  # 0 - Brick Red (brighter)
            "#888888",  # 1 - Stone Gray (brighter)
            "#B9B9B9",  # 2 - Metal Silver (brighter)
            "#9B4513",  # 3 - Wood Brown (brighter)
            "#7496C8",  # 4 - Tech Blue (brighter)
            "#6A6A6A",  # 5 - Rock Dark (brighter)
            "#9C3C32",  # 6 - Brick Clay (brighter)
            "#6050A0",  # 7 - Metal Tile (brighter)
            "#909090",  # 8 - Gray Wall (brighter)
            "#744623",  # 9 - Dark Wood (brighter)
            "#6078A0",  # 10 - Hexagons (brighter)
            "#885050",  # 11 - Dirt (brighter)
            "#BA5A46",  # 12 - Big Bricks (brighter)
            "#7E6E78",  # 13 - Storage (brighter)
            "#746464",  # 14 - Pavement (brighter)
            "#A67850"   # 15 - Wood Tile (brighter)
        ]
        
        # Floor variants (slightly lighter/less saturated)
        floor_colors = [
            "#A4503C",  # 0 - Brick Red (darker)
            "#686868",  # 1 - Stone Gray (darker)
            "#999999",  # 2 - Metal Silver (darker)
            "#7B4513",  # 3 - Wood Brown (darker)
            "#5496C8",  # 4 - Tech Blue (darker)
            "#4A4A4A",  # 5 - Rock Dark (darker)
            "#7C3C32",  # 6 - Brick Clay (darker)
            "#4050A0",  # 7 - Metal Tile (darker)
            "#707070",  # 8 - Gray Wall (darker)
            "#544623",  # 9 - Dark Wood (darker)
            "#4078A0",  # 10 - Hexagons (darker)
            "#685050",  # 11 - Dirt (darker)
            "#9A5A46",  # 12 - Big Bricks (darker)
            "#5E6E78",  # 13 - Storage (darker)
            "#546464",  # 14 - Pavement (darker)
            "#867850"   # 15 - Wood Tile (darker)
        ]
        
        return base_colors, wall_colors, floor_colors
        
    def get_cell_color(self, cell):
        """Get the color for a cell based on its properties"""
        wall_type = cell['wall_type']
        texture_id = cell['texture_id']
        floor_texture_id = cell['floor_texture_id']
        
        base_colors, wall_colors, floor_colors = self.get_texture_colors()
        
        if wall_type == 0:  # Empty - show floor texture color
            return floor_colors[floor_texture_id % 16]
        elif wall_type == 1:  # Red wall - use wall texture color
            return wall_colors[texture_id % 16]
        elif wall_type == 2:  # Green wall - use wall texture color
            return wall_colors[texture_id % 16]
        elif wall_type == 3:  # Blue wall - use wall texture color
            return wall_colors[texture_id % 16]
        elif wall_type == 4:  # Yellow wall - use wall texture color
            return wall_colors[texture_id % 16]
        elif wall_type == 5:  # Player spawn
            return "#FFFF00"
        elif wall_type == 6:  # Enemy spawn
            return "#FF0000"
        else:
            return "#404040"  # Default
            
    def on_canvas_click(self, event):
        """Handle canvas click"""
        self.is_dragging = True
        self.on_canvas_drag(event)
        
    def on_canvas_drag(self, event):
        """Handle canvas drag"""
        if not self.is_dragging:
            return
            
        # Calculate which cell was clicked
        canvas_width = self.canvas.winfo_width()
        canvas_height = self.canvas.winfo_height()
        cell_size = min(canvas_width // 16, canvas_height // 16)
        
        if cell_size < 1:
            cell_size = 32
            
        x = event.x // cell_size
        y = event.y // cell_size
        
        if 0 <= x < 16 and 0 <= y < 16:
            self.modify_cell(x, y)
            
    def on_canvas_release(self, event):
        """Handle canvas release"""
        self.is_dragging = False
        
    def on_cell_click(self, x, y):
        """Handle cell click"""
        self.modify_cell(x, y)
        
    def modify_cell(self, x, y):
        """Modify a cell based on current tool"""
        # Save state for undo
        self.save_undo_state()
        
        tool = self.tool_var.get()
        
        if tool == "paint":
            self.map_data[y][x]['wall_type'] = int(self.wall_type_var.get())
            self.map_data[y][x]['texture_id'] = self.get_texture_id(self.texture_var.get())
            self.map_data[y][x]['floor_texture_id'] = self.get_texture_id(self.floor_texture_var.get())
        elif tool == "player_spawn":
            # Remove existing player spawns
            for py in range(16):
                for px in range(16):
                    if self.map_data[py][px]['wall_type'] == 5:
                        self.map_data[py][px]['wall_type'] = 0
            # Add new player spawn
            self.map_data[y][x]['wall_type'] = 5
        elif tool == "enemy_spawn":
            self.map_data[y][x]['wall_type'] = 6
        elif tool == "fill":
            self.fill_area(x, y)
            
        self.draw_map()
        self.update_statistics()
        
    def fill_area(self, start_x, start_y):
        """Fill connected area with current settings"""
        target_wall_type = self.map_data[start_y][start_x]['wall_type']
        new_wall_type = int(self.wall_type_var.get())
        new_texture_id = self.get_texture_id(self.texture_var.get())
        new_floor_texture_id = self.get_texture_id(self.floor_texture_var.get())
        
        if target_wall_type == new_wall_type:
            return  # Already filled
            
        # Flood fill algorithm
        stack = [(start_x, start_y)]
        visited = set()
        
        while stack:
            x, y = stack.pop()
            
            if (x, y) in visited or x < 0 or x >= 16 or y < 0 or y >= 16:
                continue
                
            if self.map_data[y][x]['wall_type'] != target_wall_type:
                continue
                
            visited.add((x, y))
            self.map_data[y][x]['wall_type'] = new_wall_type
            self.map_data[y][x]['texture_id'] = new_texture_id
            self.map_data[y][x]['floor_texture_id'] = new_floor_texture_id
            
            # Add neighbors
            stack.extend([(x+1, y), (x-1, y), (x, y+1), (x, y-1)])
            
    def update_wall_type(self):
        """Update current wall type"""
        self.current_wall_type = int(self.wall_type_var.get())
        
    def get_texture_id(self, texture_name):
        """Convert texture name to ID"""
        texture_names = [
            "Brick Red", "Stone Gray", "Metal Silver", "Wood Brown",
            "Tech Blue", "Rock Dark", "Brick Clay", "Metal Tile",
            "Gray Wall", "Dark Wood", "Hexagons", "Dirt",
            "Big Bricks", "Storage", "Pavement", "Wood Tile"
        ]
        try:
            return texture_names.index(texture_name)
        except ValueError:
            return 0
            
    def update_texture(self, event=None):
        """Update current texture"""
        self.current_texture_id = self.get_texture_id(self.texture_var.get())
        
    def update_floor_texture(self, event=None):
        """Update current floor texture"""
        self.current_floor_texture_id = self.get_texture_id(self.floor_texture_var.get())
        self.update_texture_preview()
        
    def update_texture_preview(self, event=None):
        """Update the texture preview canvas"""
        self.preview_canvas.delete("all")
        
        # Get current texture colors
        base_colors, wall_colors, floor_colors = self.get_texture_colors()
        
        wall_texture_id = self.get_texture_id(self.texture_var.get())
        floor_texture_id = self.get_texture_id(self.floor_texture_var.get())
        
        # Draw wall texture preview
        wall_color = wall_colors[wall_texture_id % 16]
        self.preview_canvas.create_rectangle(10, 10, 90, 25, fill=wall_color, outline="black")
        self.preview_canvas.create_text(50, 17, text="Wall", fill="white" if self.is_dark_color(wall_color) else "black", font=("Arial", 8))
        
        # Draw floor texture preview
        floor_color = floor_colors[floor_texture_id % 16]
        self.preview_canvas.create_rectangle(10, 30, 90, 45, fill=floor_color, outline="black")
        self.preview_canvas.create_text(50, 37, text="Floor", fill="white" if self.is_dark_color(floor_color) else "black", font=("Arial", 8))
        
        # Draw texture names
        texture_names = [
            "Brick Red", "Stone Gray", "Metal Silver", "Wood Brown",
            "Tech Blue", "Rock Dark", "Brick Clay", "Metal Tile",
            "Gray Wall", "Dark Wood", "Hexagons", "Dirt",
            "Big Bricks", "Storage", "Pavement", "Wood Tile"
        ]
        
        wall_name = texture_names[wall_texture_id % 16]
        floor_name = texture_names[floor_texture_id % 16]
        
        self.preview_canvas.create_text(100, 17, text=wall_name, anchor=tk.W, font=("Arial", 8))
        self.preview_canvas.create_text(100, 37, text=floor_name, anchor=tk.W, font=("Arial", 8))
        
    def is_dark_color(self, hex_color):
        """Check if a color is dark (for text contrast)"""
        # Remove # and convert to RGB
        hex_color = hex_color.lstrip('#')
        r, g, b = tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
        # Calculate luminance
        luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255
        return luminance < 0.5
        
    def update_metadata(self, event=None):
        """Update metadata from UI"""
        self.map_name = self.name_entry.get()
        self.map_description = self.desc_text.get(1.0, tk.END).strip()
        self.map_author = self.author_entry.get()
        
    def update_statistics(self):
        """Update map statistics display"""
        # Count wall types
        wall_counts = {}
        texture_counts = {}
        floor_texture_counts = {}
        player_spawns = []
        enemy_spawns = []
        
        for y in range(16):
            for x in range(16):
                cell = self.map_data[y][x]
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
                    
        # Update statistics text
        stats_text = f"""Wall Types:
Empty: {wall_counts.get(0, 0)}
Red: {wall_counts.get(1, 0)}
Green: {wall_counts.get(2, 0)}
Blue: {wall_counts.get(3, 0)}
Yellow: {wall_counts.get(4, 0)}

Spawns:
Player: {len(player_spawns)}
Enemy: {len(enemy_spawns)}

Textures Used:
Wall: {len([t for t in texture_counts.values() if t > 0])}
Floor: {len([t for t in floor_texture_counts.values() if t > 0])}"""
        
        self.stats_text.delete(1.0, tk.END)
        self.stats_text.insert(1.0, stats_text)
        
    def save_undo_state(self):
        """Save current state for undo"""
        # Deep copy the map data
        state = [[cell.copy() for cell in row] for row in self.map_data]
        self.undo_stack.append(state)
        
        # Limit undo stack size
        if len(self.undo_stack) > 50:
            self.undo_stack.pop(0)
            
        # Clear redo stack
        self.redo_stack.clear()
        
    def undo(self):
        """Undo last action"""
        if not self.undo_stack:
            return
            
        # Save current state to redo stack
        current_state = [[cell.copy() for cell in row] for row in self.map_data]
        self.redo_stack.append(current_state)
        
        # Restore previous state
        self.map_data = self.undo_stack.pop()
        self.draw_map()
        self.update_statistics()
        
    def redo(self):
        """Redo last undone action"""
        if not self.redo_stack:
            return
            
        # Save current state to undo stack
        current_state = [[cell.copy() for cell in row] for row in self.map_data]
        self.undo_stack.append(current_state)
        
        # Restore redo state
        self.map_data = self.redo_stack.pop()
        self.draw_map()
        self.update_statistics()
        
    def new_map(self):
        """Create a new map"""
        if messagebox.askyesno("New Map", "Create a new map? Unsaved changes will be lost."):
            self.map_data = [[{'wall_type': 0, 'texture_id': 0, 'floor_texture_id': 0} for _ in range(16)] for _ in range(16)]
            self.map_name = "Untitled Map"
            self.map_description = "A RayWhen map"
            self.map_author = "Unknown"
            self.current_file = None
            
            self.name_entry.delete(0, tk.END)
            self.name_entry.insert(0, self.map_name)
            self.desc_text.delete(1.0, tk.END)
            self.desc_text.insert(1.0, self.map_description)
            self.author_entry.delete(0, tk.END)
            self.author_entry.insert(0, self.map_author)
            
            self.draw_map()
            self.update_statistics()
            self.status_var.set("New map created")
            
    def open_map(self):
        """Open an existing map"""
        file_path = filedialog.askopenfilename(
            title="Open RWM Map",
            filetypes=[("RayWhen Maps", "*.rwm"), ("All Files", "*.*")]
        )
        
        if file_path:
            try:
                self.load_rwm_file(file_path)
                self.current_file = file_path
                self.status_var.set(f"Opened: {os.path.basename(file_path)}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to open map: {str(e)}")
                
    def save_map(self):
        """Save the current map"""
        if self.current_file:
            try:
                self.save_rwm_file(self.current_file)
                self.status_var.set(f"Saved: {os.path.basename(self.current_file)}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save map: {str(e)}")
        else:
            self.save_map_as()
            
    def save_map_as(self):
        """Save the map with a new name"""
        file_path = filedialog.asksaveasfilename(
            title="Save RWM Map",
            defaultextension=".rwm",
            filetypes=[("RayWhen Maps", "*.rwm"), ("All Files", "*.*")]
        )
        
        if file_path:
            try:
                self.save_rwm_file(file_path)
                self.current_file = file_path
                self.status_var.set(f"Saved: {os.path.basename(file_path)}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save map: {str(e)}")
                
    def load_rwm_file(self, file_path):
        """Load an RWM file"""
        with open(file_path, 'rb') as f:
            # Read header
            magic = f.read(3)
            if magic != b'RWM':
                raise ValueError("Invalid RWM file")
                
            version = struct.unpack('<B', f.read(1))[0]
            if version != 1:
                raise ValueError(f"Unsupported RWM version: {version}")
                
            width = struct.unpack('<H', f.read(2))[0]
            height = struct.unpack('<H', f.read(2))[0]
            
            if width != 16 or height != 16:
                raise ValueError(f"Unsupported map size: {width}x{height}")
                
            # Read metadata
            name_len = struct.unpack('<B', f.read(1))[0]
            self.map_name = f.read(name_len).decode('utf-8', errors='ignore') if name_len > 0 else "Untitled Map"
            
            desc_len = struct.unpack('<B', f.read(1))[0]
            self.map_description = f.read(desc_len).decode('utf-8', errors='ignore') if desc_len > 0 else "A RayWhen map"
            
            author_len = struct.unpack('<B', f.read(1))[0]
            self.map_author = f.read(author_len).decode('utf-8', errors='ignore') if author_len > 0 else "Unknown"
            
            # Update UI
            self.name_entry.delete(0, tk.END)
            self.name_entry.insert(0, self.map_name)
            self.desc_text.delete(1.0, tk.END)
            self.desc_text.insert(1.0, self.map_description)
            self.author_entry.delete(0, tk.END)
            self.author_entry.insert(0, self.map_author)
            
            # Read map data
            for y in range(16):
                for x in range(16):
                    wall_type = struct.unpack('<B', f.read(1))[0]
                    texture_id = struct.unpack('<B', f.read(1))[0]
                    floor_texture_id = struct.unpack('<B', f.read(1))[0]
                    
                    self.map_data[y][x] = {
                        'wall_type': wall_type,
                        'texture_id': texture_id,
                        'floor_texture_id': floor_texture_id
                    }
                    
        self.draw_map()
        self.update_statistics()
        
    def save_rwm_file(self, file_path):
        """Save an RWM file"""
        with open(file_path, 'wb') as f:
            # Write header
            f.write(b'RWM')  # Magic
            f.write(struct.pack('<B', 1))  # Version
            f.write(struct.pack('<H', 16))  # Width
            f.write(struct.pack('<H', 16))  # Height
            
            # Write metadata
            name_bytes = self.map_name.encode('utf-8')
            f.write(struct.pack('<B', len(name_bytes)))
            f.write(name_bytes)
            
            desc_bytes = self.map_description.encode('utf-8')
            f.write(struct.pack('<B', len(desc_bytes)))
            f.write(desc_bytes)
            
            author_bytes = self.map_author.encode('utf-8')
            f.write(struct.pack('<B', len(author_bytes)))
            f.write(author_bytes)
            
            # Write map data
            for y in range(16):
                for x in range(16):
                    cell = self.map_data[y][x]
                    f.write(struct.pack('<B', cell['wall_type']))
                    f.write(struct.pack('<B', cell['texture_id']))
                    f.write(struct.pack('<B', cell['floor_texture_id']))
                    
    def load_default_map(self):
        """Load a default empty map"""
        self.draw_map()
        self.update_statistics()
        self.update_texture_preview()

def main():
    """Main function"""
    root = tk.Tk()
    app = RWMEditor(root)
    root.mainloop()

if __name__ == "__main__":
    main()
