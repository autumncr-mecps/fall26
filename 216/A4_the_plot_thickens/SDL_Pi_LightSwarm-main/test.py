import matplotlib.pyplot as plt
import matplotlib.ticker as tkr
import numpy as np

# -------------- Create and show plot --------------
x = np.linspace(0, 6, 30)
y = np.random.random(30) * 4096
z = [0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]

masters = ["...69", "...66", "...67"]
bar_data = (6, 10, 2)
bar_totals = (40, 20, 6)

colors = ["red", "white", "green"]
muted_colors = ["#880000", "#888888", "#005500"]

def setupChart(fig, ax_light, ax_master, ax_totals):
    fig.patch.set_facecolor("#222222")

    ax_light.set_title("Light Sensor Readings")
    ax_light.set_xlabel("Seconds")
    ax_light.set_ylabel("Photoresistor Reading")
    ax_light.set_ylim(0, 4096)
    ax_light.set_facecolor("#222222")
    ax_light.xaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda x, _: f'{round(x)}') # rounds them nicely back to 1, 2, 3
    )
    ax_light.title.set_color("#BBBBBB")
    ax_light.yaxis.label.set_color("#BBBBBB")
    ax_light.xaxis.label.set_color("#BBBBBB")
    ax_light.tick_params(axis="x", colors="#BBBBBB")
    ax_light.tick_params(axis="y", colors="#BBBBBB")
    ax_light.spines["top"].set_color("#BBBBBB")
    ax_light.spines["right"].set_color("#BBBBBB")
    ax_light.spines["bottom"].set_color("#BBBBBB")
    ax_light.spines["left"].set_color("#BBBBBB")

    ax_master.set_title("Masters Last 30 Sec")
    ax_master.set_xlabel("Last section of IP")
    ax_master.set_ylabel("Seconds as master")
    ax_master.set_facecolor("#222222")
    ax_master.yaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda y, _: f'{round(y)}') # rounds them nicely back to 1, 2, 3
    )
    ax_master.title.set_color("#BBBBBB")
    ax_master.yaxis.label.set_color("#BBBBBB")
    ax_master.xaxis.label.set_color("#BBBBBB")
    ax_master.tick_params(axis="x", colors="#BBBBBB")
    ax_master.tick_params(axis="y", colors="#BBBBBB")
    ax_master.spines["top"].set_color("#BBBBBB")
    ax_master.spines["right"].set_color("#BBBBBB")
    ax_master.spines["bottom"].set_color("#BBBBBB")
    ax_master.spines["left"].set_color("#BBBBBB")

    ax_totals.set_title("Masters All Time")
    ax_totals.set_xlabel("Last section of IP")
    ax_totals.set_ylabel("Percent time master")
    ax_totals.set_facecolor("#222222")
    ax_totals.yaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda y, _: f'{round(y)}') # rounds them nicely back to 1, 2, 3
    )
    ax_totals.title.set_color("#BBBBBB")
    ax_totals.yaxis.label.set_color("#BBBBBB")
    ax_totals.xaxis.label.set_color("#BBBBBB")
    ax_totals.tick_params(axis="x", colors="#BBBBBB")
    ax_totals.tick_params(axis="y", colors="#BBBBBB")
    ax_totals.spines["top"].set_color("#BBBBBB")
    ax_totals.spines["right"].set_color("#BBBBBB")
    ax_totals.spines["bottom"].set_color("#BBBBBB")
    ax_totals.spines["left"].set_color("#BBBBBB")

fig, (ax_light, ax_master, ax_totals) = plt.subplots(1, 3, figsize=(12,5), width_ratios=[6, 2, 1])
setupChart(fig, ax_light, ax_master, ax_totals)
for i in np.arange(len(x)-1):
    ax_light.plot([x[i],x[i+1]], [y[i],y[i+1]], c=colors[z[i]])

ax_master.bar(masters, bar_data, color=colors)
sum = sum(bar_totals)
for i in range(3):
    ax_totals.bar(masters[i], bar_totals[i]/sum * 100, color=muted_colors[i])

plt.show()
