import tkinter as tk
from tkinter import filedialog, messagebox
import subprocess
import time
import os

# open a folder
def browse_folder():
    folder = filedialog.askdirectory()
    if folder:
        folder_entry.delete(0, tk.END)
        folder_entry.insert(0, folder)

def run_search():
    docs_folder = folder_entry.get().strip()
    pattern = pattern_entry.get().strip()
    mode = "0" if mode_var.get() == "Exact" else "1"
    np = processes_entry.get().strip()

    if not docs_folder or not pattern or not np:
        messagebox.showwarning("Missing Input", "Please fill all fields.")
        return

    cmd = ["mpirun", "-np", np, "./docsearch", docs_folder, pattern, mode]

    try:
        start = time.time()
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        end = time.time()
    except subprocess.CalledProcessError as e:
        output_text.config(state=tk.NORMAL)
        output_text.delete(1.0, tk.END)
        output_text.insert(tk.END, f"❌ Error:\n{e.stderr}")
        output_text.config(state=tk.DISABLED)
        return

    output = result.stdout
    output_text.config(state=tk.NORMAL)
    output_text.delete(1.0, tk.END)

    # === MATCHES (from SERIAL, unique filenames only) ===
    output_text.insert(tk.END, "🔍 Matches\n", "heading")
    output_text.insert(tk.END, "──────────────────────────────\n")

    seen_filenames = set()
    match_count = 0
    for line in output.splitlines():
        if line.startswith("[SERIAL]") and "Found in" in line:
            full_path = line.split("Found in")[-1].strip()
            filename = os.path.basename(full_path)
            if filename not in seen_filenames:
                output_text.insert(tk.END, f"{filename}\n")
                seen_filenames.add(filename)
                match_count += 1

    if match_count == 0:
        output_text.insert(tk.END, "No match found.\n")

    # === TIMINGS ===
    output_text.insert(tk.END, "\n⏱️ Execution Times\n", "heading")
    output_text.insert(tk.END, "──────────────────────────────\n")

    serial_time = extract_time(output, "[SERIAL]")
    openmp_time = extract_time(output, "[OPENMP]")
    mpi_time = extract_time(output, "[MPI]")
    mpi_omp_time = extract_time(output, "[MPI+OPENMP]")

    output_text.insert(tk.END, f"🧵 Serial:        {serial_time}\n")
    output_text.insert(tk.END, f"🧵 OpenMP:        {openmp_time}\n")
    output_text.insert(tk.END, f"💻 MPI:           {mpi_time}\n")
    output_text.insert(tk.END, f"⚡ MPI + OpenMP:  {mpi_omp_time}\n")
    output_text.insert(tk.END, f"\n📌 Total Wall Time: {end - start:.4f} seconds")

    output_text.config(state=tk.DISABLED)



def extract_time(output, label):
    for line in output.splitlines():
        if line.startswith(label) and "Time:" in line:
            parts = line.split("Time:")
            return parts[1].strip() if len(parts) > 1 else "N/A"
    return "N/A"

# GUI Setup
root = tk.Tk()
root.title("DocSearch Parallel Search GUI")
root.geometry("800x600")  # initial size

# Make the grid expand with window resize
root.grid_rowconfigure(5, weight=1)
root.grid_columnconfigure(1, weight=1)

tk.Label(root, text="📂 Documents Folder:").grid(row=0, column=0, sticky="w", padx=10, pady=5)
folder_entry = tk.Entry(root)
folder_entry.grid(row=0, column=1, sticky="ew", padx=5)
tk.Button(root, text="Browse", command=browse_folder).grid(row=0, column=2, padx=5)

tk.Label(root, text="🔍 Search Pattern:").grid(row=1, column=0, sticky="w", padx=10, pady=5)
pattern_entry = tk.Entry(root)
pattern_entry.grid(row=1, column=1, sticky="ew", padx=5)

tk.Label(root, text="⚙️ Search Mode:").grid(row=2, column=0, sticky="w", padx=10, pady=5)
mode_var = tk.StringVar(value="Exact")
tk.OptionMenu(root, mode_var, "Exact", "Approximate").grid(row=2, column=1, sticky="w", padx=5)

tk.Label(root, text="💻 MPI Processes:").grid(row=3, column=0, sticky="w", padx=10, pady=5)
processes_entry = tk.Entry(root, width=10)
processes_entry.insert(0, "4")  # default
processes_entry.grid(row=3, column=1, sticky="w", padx=5)

tk.Button(root, text="🔎 Run Search", command=run_search, bg="#4CAF50", fg="white").grid(row=4, column=1, pady=10, sticky="e")

# Frame for scrollable Text output
output_frame = tk.Frame(root)
output_frame.grid(row=5, column=0, columnspan=3, sticky="nsew", padx=10, pady=10)

output_text = tk.Text(output_frame, wrap="word")
output_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

scrollbar = tk.Scrollbar(output_frame, command=output_text.yview)
scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

output_text.config(yscrollcommand=scrollbar.set)


root.mainloop()
