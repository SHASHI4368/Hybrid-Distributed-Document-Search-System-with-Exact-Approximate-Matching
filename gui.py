import tkinter as tk
from tkinter import ttk, messagebox
import subprocess

class SearchApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Hybrid Document Search")

        # --- Input Frame ---
        input_frame = ttk.Frame(root, padding="10")
        input_frame.grid(row=0, column=0, sticky="ew")

        ttk.Label(input_frame, text="Search Keyword:").grid(row=0, column=0, sticky="w")
        self.keyword_entry = ttk.Entry(input_frame, width=40)
        self.keyword_entry.grid(row=0, column=1, padx=5)

        ttk.Label(input_frame, text="Search Method:").grid(row=1, column=0, sticky="w")
        self.search_mode = tk.StringVar()
        ttk.Combobox(input_frame, textvariable=self.search_mode, values=["Exact", "Approximate"], width=37).grid(row=1, column=1, padx=5)
        self.search_mode.set("Exact")

        # --- Search Button ---
        search_btn = ttk.Button(input_frame, text="Search", command=self.run_search)
        search_btn.grid(row=2, column=0, columnspan=2, pady=10)

        # --- Output Box ---
        output_frame = ttk.Frame(root, padding="10")
        output_frame.grid(row=1, column=0, sticky="nsew")

        self.output_text = tk.Text(output_frame, height=20, wrap="word")
        self.output_text.pack(side="left", fill="both", expand=True)

        scrollbar = ttk.Scrollbar(output_frame, command=self.output_text.yview)
        scrollbar.pack(side="right", fill="y")
        self.output_text.configure(yscrollcommand=scrollbar.set)

        root.grid_rowconfigure(1, weight=1)
        root.grid_columnconfigure(0, weight=1)

    def run_search(self):
        keyword = self.keyword_entry.get().strip()
        mode_str = self.search_mode.get()
        mode = "0" if mode_str == "Exact" else "1"

        if not keyword:
            messagebox.showwarning("Input Error", "Please enter a keyword.")
            return

        # Clear output
        self.output_text.delete("1.0", tk.END)

        # Build command: mpirun -np 4 ./docsearch docs <keyword> <mode>
        cmd = ["mpirun", "-np", "4", "./docsearch", "docs", keyword, mode]

        try:
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            output = result.stdout.strip() or result.stderr.strip()
        except FileNotFoundError:
            output = "Error: Could not run docsearch. Make sure it is compiled and mpirun is installed."

        self.output_text.insert(tk.END, output)

if __name__ == "__main__":
    root = tk.Tk()
    app = SearchApp(root)
    root.mainloop()

