import matplotlib.pyplot as plt
import matplotlib.ticker as tkr
import numpy as np

# -------------- Create and show plot --------------
x = np.linspace(0, 6, 30)
y = np.random.random(30) * 4096
z = [0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1]

colors = ["red", "white", "green"]

def setupChart(fig, ax):
    plt.title("Light Sensor Readings")
    fig.patch.set_facecolor("#222222")
    ax.set_facecolor("#222222")
    ax.xaxis.set_major_formatter(   
        tkr.FuncFormatter(lambda x, _: f'{round(x)}') # rounds them nicely back to 1, 2, 3
    )
    ax.title.set_color("#BBBBBB")
    ax.yaxis.label.set_color("#BBBBBB")
    ax.xaxis.label.set_color("#BBBBBB")
    ax.tick_params(axis="x", colors="#BBBBBB")
    ax.tick_params(axis="y", colors="#BBBBBB")
    ax.spines["top"].set_color("#BBBBBB")
    ax.spines["right"].set_color("#BBBBBB")
    ax.spines["bottom"].set_color("#BBBBBB")
    ax.spines["left"].set_color("#BBBBBB")

fig = plt.figure(1, figsize=(10,5))
ax = plt.gca()
ax.set_ylim(0, 4096)
setupChart(fig, ax)
for i in np.arange(len(x)-1):
    ax.plot([x[i],x[i+1]], [y[i],y[i+1]], c=colors[z[i]])

plt.show()
