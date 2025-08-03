import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import subprocess
import time
import os
import re

# open a folder
def browse_folder():
    folder = filedialog.askdirectory()
    if folder:
        folder_entry.delete(0, tk.END)
        folder_entry.insert(0, folder)

def count_supported_files(folder):
    supported_exts = ('.txt', '.pdf', '.docx')
    count = 0
    for entry in os.listdir(folder):
        if entry.lower().endswith(supported_exts):
            count += 1
    return count

def extract_performance_data(output):
    """Extract detailed performance data from the C program output"""
    data = {
        'matches': [],
        'total_files': 0,
        'match_count': 0,
        'times': {},
        'preprocessing_times': {},
        'search_times': {},
        'found_counts': {},
        'speedups': {},
        'efficiency': {},
        'insights': []
    }
    
    lines = output.splitlines()
    
    # Extract matches from SERIAL output
    seen_filenames = set()
    for line in lines:
        if line.startswith("[SERIAL]") and "Found in" in line:
            full_path = line.split("Found in")[-1].strip()
            filename = os.path.basename(full_path)
            if filename not in seen_filenames:
                data['matches'].append(filename)
                seen_filenames.add(filename)
    
    data['match_count'] = len(data['matches'])
    
    # Extract timing data using regex patterns
    time_patterns = {
        'serial': r'\[SERIAL\] Total: ([\d.]+) seconds, Found: (\d+)',
        'openmp': r'\[OPENMP\] Total: ([\d.]+) seconds, Found: (\d+)',
        'mpi': r'\[MPI\] Total: ([\d.]+) seconds, Found: (\d+)',
        'hybrid': r'\[MPI\+OPENMP\] Total: ([\d.]+) seconds, Found: (\d+)'
    }
    
    preprocessing_patterns = {
        'serial': r'\[SERIAL\] Preprocessing: ([\d.]+) seconds',
        'openmp': r'\[OPENMP\] Preprocessing: ([\d.]+) seconds',
        'mpi': r'\[MPI\] Preprocessing: ([\d.]+) seconds',
        'hybrid': r'\[MPI\+OPENMP\] Preprocessing: ([\d.]+) seconds'
    }
    
    search_patterns = {
        'serial': r'\[SERIAL\] Search: ([\d.]+) seconds',
        'openmp': r'\[OPENMP\] Search: ([\d.]+) seconds',
        'mpi': r'\[MPI\] Search: ([\d.]+) seconds',
        'hybrid': r'\[MPI\+OPENMP\] Search: ([\d.]+) seconds'
    }
    
    # Extract total times and found counts
    for method, pattern in time_patterns.items():
        match = re.search(pattern, output)
        if match:
            data['times'][method] = float(match.group(1))
            data['found_counts'][method] = int(match.group(2))
    
    # Extract preprocessing times
    for method, pattern in preprocessing_patterns.items():
        match = re.search(pattern, output)
        if match:
            data['preprocessing_times'][method] = float(match.group(1))
    
    # Extract search times
    for method, pattern in search_patterns.items():
        match = re.search(pattern, output)
        if match:
            data['search_times'][method] = float(match.group(1))
    
    # Extract speedup data from the speedup table
    speedup_section = False
    for line in lines:
        if "=== SPEEDUP ANALYSIS ===" in line:
            speedup_section = True
            continue
        elif speedup_section and line.startswith("Total"):
            # Parse speedup line: "Total           | 5.23x  | 3.45x  | 6.78x"
            parts = line.split('|')
            if len(parts) >= 4:
                try:
                    data['speedups']['openmp'] = float(parts[1].strip().rstrip('x'))
                    data['speedups']['mpi'] = float(parts[2].strip().rstrip('x'))
                    data['speedups']['hybrid'] = float(parts[3].strip().rstrip('x'))
                except:
                    pass
            break
    
    # Extract efficiency data
    for line in lines:
        if "OpenMP:" in line and "% (16 threads)" in line:
            match = re.search(r'([\d.]+)%', line)
            if match:
                data['efficiency']['openmp'] = float(match.group(1))
        elif "MPI:" in line and "processes)" in line:
            match = re.search(r'([\d.]+)%', line)
            if match:
                data['efficiency']['mpi'] = float(match.group(1))
        elif "Hybrid:" in line and "processes ×" in line:
            match = re.search(r'([\d.]+)%', line)
            if match:
                data['efficiency']['hybrid'] = float(match.group(1))
    
    # Extract performance insights
    insights_section = False
    for line in lines:
        if "=== PERFORMANCE INSIGHTS ===" in line:
            insights_section = True
            continue
        elif insights_section and line.startswith("✓"):
            data['insights'].append(line[2:])  # Remove the checkmark
    
    return data

def format_time(seconds):
    """Format time with appropriate precision"""
    if seconds < 0.001:
        return f"{seconds*1000:.3f}ms"
    elif seconds < 1:
        return f"{seconds*1000:.1f}ms"
    else:
        return f"{seconds:.4f}s"

def run_search():
    docs_folder = folder_entry.get().strip()
    pattern = pattern_entry.get().strip()
    mode = "0" if mode_var.get() == "Exact" else "1"
    np = processes_entry.get().strip()

    if not docs_folder or not pattern or not np:
        messagebox.showwarning("Missing Input", "Please fill all fields.")
        return

    if not os.path.exists(docs_folder):
        messagebox.showerror("Error", "Documents folder does not exist.")
        return

    # Show progress
    progress_var.set("Running search...")
    root.update()

    cmd = ["mpirun", "-np", np, "./docsearch", docs_folder, pattern, mode]

    try:
        start = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        end = time.time()
        wall_time = end - start
    except subprocess.CalledProcessError as e:
        output_text.config(state=tk.NORMAL)
        output_text.delete(1.0, tk.END)
        output_text.insert(tk.END, f"❌ Error running search:\n{e.stderr}")
        output_text.config(state=tk.DISABLED)
        progress_var.set("Ready")
        return
    except FileNotFoundError:
        messagebox.showerror("Error", "MPI or docsearch executable not found. Please ensure they are installed and in PATH.")
        progress_var.set("Ready")
        return

    # Extract performance data
    data = extract_performance_data(result.stdout)
    total_files = count_supported_files(docs_folder)

    # Clear and populate output
    output_text.config(state=tk.NORMAL)
    output_text.delete(1.0, tk.END)

    # === SEARCH RESULTS ===
    output_text.insert(tk.END, "🔍 SEARCH RESULTS\n", "heading")
    output_text.insert(tk.END, "=" * 50 + "\n")
    
    if data['matches']:
        output_text.insert(tk.END, f"📂 Files with matches ({data['match_count']} found):\n", "subheading")
        for i, filename in enumerate(data['matches'][:10], 1):  # Show first 10
            output_text.insert(tk.END, f"  {i:2d}. {filename}\n")
        if len(data['matches']) > 10:
            output_text.insert(tk.END, f"     ... and {len(data['matches']) - 10} more files\n")
    else:
        output_text.insert(tk.END, "❌ No matches found in any files\n")

    output_text.insert(tk.END, f"\n📊 Total files searched: {total_files}\n")
    output_text.insert(tk.END, f"✅ Files with matches: {data['match_count']}\n")
    output_text.insert(tk.END, f"📌 Wall clock time: {format_time(wall_time)}\n\n")

    # === PERFORMANCE BREAKDOWN ===
    output_text.insert(tk.END, "⚡ PERFORMANCE BREAKDOWN\n", "heading")
    output_text.insert(tk.END, "=" * 50 + "\n")
    
    # Create a formatted table
    methods = ['serial', 'openmp', 'mpi', 'hybrid']
    method_names = {'serial': 'Serial', 'openmp': 'OpenMP', 'mpi': 'MPI', 'hybrid': 'MPI+OpenMP'}
    
    # Timing table
    output_text.insert(tk.END, "🕐 Execution Times:\n", "subheading")
    output_text.insert(tk.END, f"{'Method':<12} {'Preprocessing':<14} {'Search':<10} {'Total':<10} {'Found':<6}\n")
    output_text.insert(tk.END, "-" * 58 + "\n")
    
    for method in methods:
        if method in data['times']:
            name = method_names[method]
            prep_time = format_time(data['preprocessing_times'].get(method, 0))
            search_time = format_time(data['search_times'].get(method, 0))
            total_time = format_time(data['times'][method])
            found = data['found_counts'].get(method, 0)
            output_text.insert(tk.END, f"{name:<12} {prep_time:<14} {search_time:<10} {total_time:<10} {found:<6}\n")
    
    # === SPEEDUP ANALYSIS ===
    if data['speedups']:
        output_text.insert(tk.END, "\n🚀 SPEEDUP ANALYSIS\n", "heading")
        output_text.insert(tk.END, "=" * 50 + "\n")
        
        if 'openmp' in data['speedups']:
            output_text.insert(tk.END, f"OpenMP:      {data['speedups']['openmp']:.2f}x faster than serial\n")
        if 'mpi' in data['speedups']:
            output_text.insert(tk.END, f"MPI:         {data['speedups']['mpi']:.2f}x faster than serial\n")
        if 'hybrid' in data['speedups']:
            output_text.insert(tk.END, f"MPI+OpenMP:  {data['speedups']['hybrid']:.2f}x faster than serial\n")

    # === EFFICIENCY ANALYSIS ===
    if data['efficiency']:
        output_text.insert(tk.END, "\n📈 EFFICIENCY ANALYSIS\n", "heading")
        output_text.insert(tk.END, "=" * 50 + "\n")
        
        if 'openmp' in data['efficiency']:
            output_text.insert(tk.END, f"OpenMP:      {data['efficiency']['openmp']:.1f}% (16 threads)\n")
        if 'mpi' in data['efficiency']:
            output_text.insert(tk.END, f"MPI:         {data['efficiency']['mpi']:.1f}% ({np} processes)\n")
        if 'hybrid' in data['efficiency']:
            output_text.insert(tk.END, f"Hybrid:      {data['efficiency']['hybrid']:.1f}% ({np} processes × threads)\n")

    # === PERFORMANCE INSIGHTS ===
    if data['insights']:
        output_text.insert(tk.END, "\n💡 PERFORMANCE INSIGHTS\n", "heading")
        output_text.insert(tk.END, "=" * 50 + "\n")
        for insight in data['insights']:
            output_text.insert(tk.END, f"✓ {insight}\n")

    # === BEST PERFORMER ===
    output_text.insert(tk.END, "\n🏆 PERFORMANCE SUMMARY\n", "heading")
    output_text.insert(tk.END, "=" * 50 + "\n")
    
    if data['times']:
        best_method = min(data['times'].items(), key=lambda x: x[1])
        best_name = method_names[best_method[0]]
        best_time = best_method[1]
        
        output_text.insert(tk.END, f"🥇 Fastest method: {best_name} ({format_time(best_time)})\n")
        
        if 'serial' in data['times']:
            speedup = data['times']['serial'] / best_time
            output_text.insert(tk.END, f"🚀 Best speedup: {speedup:.2f}x over serial\n")

    output_text.config(state=tk.DISABLED)
    progress_var.set("Search completed!")

# GUI Setup
root = tk.Tk()
root.title("DocSearch Parallel Performance Analyzer")
root.geometry("900x700")
root.minsize(800, 600)

# Configure styles
style = ttk.Style()
style.theme_use('clam')

# Make the grid expand with window resize
root.grid_rowconfigure(6, weight=1)
root.grid_columnconfigure(1, weight=1)

# Input fields
tk.Label(root, text="📂 Documents Folder:", font=('Arial', 10, 'bold')).grid(row=0, column=0, sticky="w", padx=10, pady=5)
folder_entry = tk.Entry(root, font=('Arial', 10))
folder_entry.grid(row=0, column=1, sticky="ew", padx=5)
tk.Button(root, text="Browse", command=browse_folder, bg="#2196F3", fg="white", font=('Arial', 9)).grid(row=0, column=2, padx=5)

tk.Label(root, text="🔍 Search Pattern:", font=('Arial', 10, 'bold')).grid(row=1, column=0, sticky="w", padx=10, pady=5)
pattern_entry = tk.Entry(root, font=('Arial', 10))
pattern_entry.grid(row=1, column=1, sticky="ew", padx=5)

tk.Label(root, text="⚙️ Search Mode:", font=('Arial', 10, 'bold')).grid(row=2, column=0, sticky="w", padx=10, pady=5)
mode_var = tk.StringVar(value="Exact")
mode_menu = tk.OptionMenu(root, mode_var, "Exact", "Approximate")
mode_menu.config(font=('Arial', 10))
mode_menu.grid(row=2, column=1, sticky="w", padx=5)

tk.Label(root, text="💻 MPI Processes:", font=('Arial', 10, 'bold')).grid(row=3, column=0, sticky="w", padx=10, pady=5)
processes_entry = tk.Entry(root, width=10, font=('Arial', 10))
processes_entry.insert(0, "4")
processes_entry.grid(row=3, column=1, sticky="w", padx=5)

# Progress indicator
progress_var = tk.StringVar(value="Ready")
progress_label = tk.Label(root, textvariable=progress_var, font=('Arial', 9), fg="blue")
progress_label.grid(row=4, column=0, columnspan=2, sticky="w", padx=10, pady=5)

# Search button
search_btn = tk.Button(root, text="🔎 Run Performance Analysis", command=run_search, 
                      bg="#4CAF50", fg="white", font=('Arial', 11, 'bold'), pady=5)
search_btn.grid(row=5, column=1, pady=10, sticky="e")

# Output area with improved styling
output_frame = tk.Frame(root, relief="sunken", bd=1)
output_frame.grid(row=6, column=0, columnspan=3, sticky="nsew", padx=10, pady=10)

output_text = tk.Text(output_frame, wrap="word", font=('Consolas', 10), bg="#f8f9fa", fg="#212529")
output_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

# Configure text tags for styling
output_text.tag_configure("heading", font=('Arial', 12, 'bold'), foreground="#1976D2")
output_text.tag_configure("subheading", font=('Arial', 10, 'bold'), foreground="#388E3C")

scrollbar = tk.Scrollbar(output_frame, command=output_text.yview)
scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
output_text.config(yscrollcommand=scrollbar.set)

# Initial status
progress_var.set("Ready - Enter search parameters and click 'Run Performance Analysis'")

root.mainloop()