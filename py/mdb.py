import sys
import os
import sqlite3
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import numpy as np
import tkinter as tk
from tkinter import ttk

class ForceDataPlotter:
    def __init__(self, db_path, round_id, parent):
        self.db_path = db_path
        self.round_id = round_id
        self.conn = None
        self.cursor = None
        self.parent = parent
        
        self.connect_to_database()
        
        self.time, self.force1, self.force2 = [], [], []
        self.torque, self.position = [], []
        
        self.fig, (self.ax1, self.ax2, self.ax3) = plt.subplots(3, 1, figsize=(8, 6))
        self.line1, = self.ax1.plot([], [], color='#D95319', linewidth=2)
        self.line2, = self.ax1.plot([], [], color='#0072BD', linewidth=2)
        self.line3, = self.ax2.plot([], [], color='#D95319', linewidth=2)
        self.line4, = self.ax3.plot([], [], color='#D95319', linewidth=2)
        
        self.setup_plot()
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.parent)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        self.update_counter = 0
        self.axis_update_interval = 10
        self.last_data_length = 0  # 初始化 last_data_length

        self.initialize_data()  # 移动到最后，确保所有属性都已初始化

    def connect_to_database(self):
        try:
            self.conn = sqlite3.connect(self.db_path)
            self.cursor = self.conn.cursor()
            print(f"Successfully connected to database: {self.db_path}")
        except sqlite3.Error as e:
            print(f"Error connecting to database: {e}")
            raise

    def setup_plot(self):
        for ax in (self.ax1, self.ax2, self.ax3):
            ax.grid(True)
            ax.set_xlabel('Time (s)', fontsize=8, fontweight='bold', fontname='Times New Roman')
            ax.tick_params(direction='out', width=1.5, labelsize=12)
            for spine in ax.spines.values():
                spine.set_linewidth(1.5)

        self.ax1.set_ylabel('Tension (N)', fontsize=8, fontweight='bold', fontname='Times New Roman')
        self.ax2.set_ylabel('Torque (Nm)', fontsize=8, fontweight='bold', fontname='Times New Roman')
        self.ax3.set_ylabel('Position (mm)', fontsize=8, fontweight='bold', fontname='Times New Roman')
        
        self.ax1.legend(['Upward', 'Downward'], loc='best', fontsize=12, edgecolor='none')

    def initialize_data(self):
        self.query_data(incremental=False)
        self.set_initial_limits()

    def query_data(self, incremental=True):
        if incremental:
            start_index = self.last_data_length
        else:
            start_index = 0
            self.time, self.force1, self.force2, self.torque, self.position = [], [], [], [], []

        self.cursor.execute(f"SELECT ForceData FROM Forcedata WHERE RoundID = {self.round_id} AND ChID = 1")
        new_force1 = [float(row[0]) for row in self.cursor.fetchall()[start_index:]]
        
        self.cursor.execute(f"SELECT ForceData FROM Forcedata WHERE RoundID = {self.round_id} AND ChID = 2")
        new_force2 = [-float(row[0]) for row in self.cursor.fetchall()[start_index:]]
        
        self.cursor.execute(f"SELECT TorData FROM Torquedata WHERE RoundID = {self.round_id}")
        new_torque = [float(row[0]) for row in self.cursor.fetchall()[start_index:]]
        
        self.cursor.execute(f"SELECT PosData FROM Positiondata WHERE RoundID = {self.round_id}")
        new_position = [float(row[0]) - 58.23 for row in self.cursor.fetchall()[start_index:]]
        
        self.force1.extend(new_force1)
        self.force2.extend(new_force2)
        self.torque.extend(new_torque)
        self.position.extend(new_position)

        new_data_length = max(len(new_force1), len(new_force2), len(new_torque), len(new_position))
        new_time = np.arange(self.last_data_length, self.last_data_length + new_data_length) * 0.1
        self.time = np.concatenate((self.time, new_time))

        self.last_data_length += new_data_length

    def set_initial_limits(self):
        x_max = max(self.time) if len(self.time) > 0 else 1
        for ax in (self.ax1, self.ax2, self.ax3):
            ax.set_xlim(0, x_max)

        y1_min = min(min(self.force1), min(self.force2)) if len(self.force1) > 0 and len(self.force2) > 0 else -1
        y1_max = max(max(self.force1), max(self.force2)) if len(self.force1) > 0 and len(self.force2) > 0 else 1
        self.ax1.set_ylim(y1_min - 0.1 * abs(y1_min), y1_max + 0.1 * abs(y1_max))

        y2_min, y2_max = (min(self.torque), max(self.torque)) if len(self.torque) > 0 else (-1, 1)
        self.ax2.set_ylim(y2_min - 0.1 * abs(y2_min), y2_max + 0.1 * abs(y2_max))

        y3_min, y3_max = (min(self.position), max(self.position)) if len(self.position) > 0 else (-1, 1)
        self.ax3.set_ylim(y3_min - 0.1 * abs(y3_min), y3_max + 0.1 * abs(y3_max))

    def update(self, frame):
        self.query_data(incremental=True)
        
        self.line1.set_data(self.time[:len(self.force1)], self.force1)
        self.line2.set_data(self.time[:len(self.force2)], self.force2)
        self.line3.set_data(self.time[:len(self.torque)], self.torque)
        self.line4.set_data(self.time[:len(self.position)], self.position)
        
        self.update_counter += 1
        if self.update_counter >= self.axis_update_interval:
            self.update_counter = 0
            self.update_axis_limits()
        
        return self.line1, self.line2, self.line3, self.line4

    def update_axis_limits(self):
        x_max = max(self.time) if len(self.time) > 0 else 1
        for ax in (self.ax1, self.ax2, self.ax3):
            ax.set_xlim(0, x_max)

        y1_min = min(min(self.force1), min(self.force2)) if len(self.force1) > 0 and len(self.force2) > 0 else -1
        y1_max = max(max(self.force1), max(self.force2)) if len(self.force1) > 0 and len(self.force2) > 0 else 1
        self.ax1.set_ylim(y1_min - 0.1 * abs(y1_min), y1_max + 0.1 * abs(y1_max))

        y2_min, y2_max = (min(self.torque), max(self.torque)) if len(self.torque) > 0 else (-1, 1)
        self.ax2.set_ylim(y2_min - 0.1 * abs(y2_min), y2_max + 0.1 * abs(y2_max))

        y3_min, y3_max = (min(self.position), max(self.position)) if len(self.position) > 0 else (-1, 1)
        self.ax3.set_ylim(y3_min - 0.1 * abs(y3_min), y3_max + 0.1 * abs(y3_max))

        self.canvas.draw()

    def animate(self):
        self.ani = FuncAnimation(self.fig, self.update, frames=None, interval=200, blit=True)
        self.canvas.draw()

    def close(self):
        if self.conn:
            self.conn.close()
            print("Database connection closed.")

        plt.close(self.fig)

def run_plotter(db_path, round_id):
    print("Starting run_plotter function")
    
    if not os.path.exists(db_path):
        print(f"Error: Database file not found at {db_path}")
        return
    
    root = tk.Tk()
    root.title("Force Data Plotter")
    root.geometry("850x700")

    plotter_frame = ttk.Frame(root)
    plotter_frame.pack(fill=tk.BOTH, expand=True)

    plotter = ForceDataPlotter(db_path, round_id, plotter_frame)

    control_frame = ttk.Frame(root)
    control_frame.pack(side=tk.BOTTOM, fill=tk.X)

    round_id_var = tk.IntVar(value=round_id)
    round_id_label = ttk.Label(control_frame, text="Select Round ID:")
    round_id_label.pack(side=tk.LEFT, padx=10, pady=5)
    
    round_id_menu = ttk.Combobox(control_frame, textvariable=round_id_var, values=list(range(0, 21)), state='readonly')
    round_id_menu.pack(side=tk.LEFT, padx=10, pady=5)
    
    def on_plot():
        plotter.round_id = round_id_var.get()
        plotter.initialize_data()
        plotter.animate()
    
    plot_button = ttk.Button(control_frame, text="Plot Data", command=on_plot)
    plot_button.pack(side=tk.LEFT, padx=10, pady=5)

    def on_closing():
        plotter.close()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_closing)

    root.mainloop()

if __name__ == "__main__":
    db_path = os.path.expanduser('/home/hui/workdir/VK701_Demo/db/mdbsqlite.db')
    round_id = 0
    run_plotter(db_path, round_id)