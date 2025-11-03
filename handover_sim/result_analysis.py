import pandas as pd
import matplotlib.pyplot as plt
import glob, os, re

# extract final summary from CSV
def extract_summary(file):
    with open(file, encoding="utf-8") as f:
        lines = f.readlines()
    data = {"Protocol": "", "UEs": 0, "Payload": 0,
            "Delay": 0, "Throughput": 0, "Loss": 0}

    fname = os.path.basename(file).lower()

    # detect protocol
    if "udp_" in fname:
        data["Protocol"] = "UDP"
    elif "tcp_" in fname:
        data["Protocol"] = "TCP"
    elif "quic_" in fname:
        data["Protocol"] = "QUIC"

    # --- extract UE index from filename ---
    # pattern like _ue99_ → UEs = 100
    ue_match = re.search(r"_ue(\d+)_", fname)
    if ue_match:
        data["UEs"] = int(ue_match.group(1)) + 1

    # --- extract run index from filename ---
    # pattern like run19_ → payload size (example scaling: 100,200,...)
    run_match = re.search(r"run(\d+)_", fname)
    if run_match:
        data["Payload"] = (int(run_match.group(1)) + 1) * 100

    # parse metrics from Final Statistics section
    for line in lines:
        if "Average Packet Delay" in line:
            nums = re.findall(r"[-+]?\d*\.\d+|\d+", line)
            if nums:
                data["Delay"] = float(nums[0])
        if "Throughput" in line:
            nums = re.findall(r"[-+]?\d*\.\d+|\d+", line)
            if nums:
                data["Throughput"] = float(nums[0])
        if "Packet Loss Ratio" in line:
            nums = re.findall(r"[-+]?\d*\.\d+|\d+", line)
            if nums:
                data["Loss"] = float(nums[0])
    return data


# -------------------------------
# collect data from all CSVs
# -------------------------------
rows = []
for f in glob.glob("results/*/*.csv"):
    if "stats" in f.lower():
        try:
            rows.append(extract_summary(f))
        except Exception as e:
            print(f"⚠️ Error reading {f}: {e}")

if not rows:
    print("No result CSV files found. Please check your 'results' folder path.")
    exit()

df = pd.DataFrame(rows)
df = df.sort_values(by=["Protocol", "UEs", "Payload"])

print(f"Loaded {len(df)} result summaries.")
print(df.head())

# -------------------------------
# Graph 1 – Delay vs Number of UEs
# -------------------------------
plt.figure(figsize=(7,5))
for proto in ["UDP", "TCP", "QUIC"]:
    subset = df[df["Protocol"] == proto]
    plt.plot(subset["UEs"], subset["Delay"], marker="o", label=proto)
plt.title("Average Packet Delay vs Number of UEs")
plt.xlabel("Number of UEs")
plt.ylabel("Average Delay (s)")
plt.legend()
plt.grid(True)
plt.tight_layout()

# make a folder called "graphs" & save the graphs before showing
os.makedirs("graphs", exist_ok=True)
plt.savefig(os.path.join("graphs", "delay_vs_ues.png"), dpi=300)
plt.show() # finally show the graphs

# -------------------------------
# Graph 2 – Throughput vs Payload Size
# -------------------------------
plt.figure(figsize=(7,5))
for proto in ["UDP", "TCP", "QUIC"]:
    subset = df[df["Protocol"] == proto]
    plt.plot(subset["Payload"], subset["Throughput"], marker="o", label=proto)
plt.title("Throughput vs Payload Size")
plt.xlabel("Payload Size (B)")
plt.ylabel("Throughput (Mbps)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(os.path.join("graphs", "throughput_vs_payload.png"), dpi=300) # add the graph to the folder
plt.show()

# -------------------------------
# Graph 3 – Packet Loss Ratio vs Number of UEs
# -------------------------------
plt.figure(figsize=(7,5))
for proto in ["UDP", "TCP", "QUIC"]:
    subset = df[df["Protocol"] == proto]
    plt.plot(subset["UEs"], subset["Loss"], marker="o", label=proto)
plt.title("Packet Loss Ratio vs Number of UEs")
plt.xlabel("Number of UEs")
plt.ylabel("Loss Ratio (%)")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(os.path.join("graphs", "loss_vs_ues.png"), dpi=300) # add the graph to the folder
plt.show()