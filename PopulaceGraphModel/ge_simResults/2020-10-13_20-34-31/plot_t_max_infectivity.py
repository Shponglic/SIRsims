import matplotlib.pyplot as plt
import matplotlib
import numpy as np
import pandas as pd
import utils as u
from matplotlib.collections import PatchCollection
from matplotlib.patches import Rectangle
import matplotlib.patches as mpatches
import matplotlib.colors as _colors

# Return dictionary parameter/values of first row of a DataFrame
def getParams(dfs, params):
    row = dfs.head(1)[list(params)]
    dct = {}
    values = row.values[0]
    for i,p in enumerate(params):
        dct[p] = values[i]
    return dct

#-----------------------------------------------------
df = pd.read_pickle("transformed_metadata.gz")
print(df.columns)
#df.to_csv("df.csv")

# Choose all elements with
# DataFrame headers
# ,sm,sd,wm,wd,red_mask,red_dist,tau,gamma,SIR,fn,ages
# Choose frames with sm = sd = 1, wm = wd = 0

group_key = ['sm','sd','wm','wd']
dfg = df.groupby(group_key)

keys = []
dfs = {}
for key, gr in dfg:
    keys.append(key)
    dfs[key]= dfg.get_group(key)

#-----------------------------------------------------

#---------------------
# Choose rows according to some criteria from the pandas table
# For example, wm=1, wd=1, sm=0, sd=0
# and then all the reductions of masks and keeping social reductions constant. 
# or create a 2D plot of max infection as a function of age somehow
#---------------------

#---------------------
# return a matrix of the time of maximum infections and a matrix
# of maximum infections
def computeTMaxInfections(age_bracket, df0):
    # I AM CALCULATING REDUNDANT STUFF.
    k = age_bracket
    mat = np.zeros([6,6])   # <<<<
    mat_maxI = np.zeros([6,6], dtype='int')   # <<<<
    mat_d = {}
    mat_maxI_d = {}
    for r in df0.itertuples():  # loop over rows
        #print("r.argmaxI_age: ", r.argmaxI_age)
        curve = r.SIR_age[k]
        red_mask = r.red_mask
        red_dist = r.red_dist
        t_maxI = r.t_maxI_age[k]    # time of max infectivity
        maxI = r.maxI_age[k]    # time of max infectivity
        #print("t_maxI= ", t_maxI, len(curve['S']), len(curve['t']))
        #print(curve['t'])
        N_age = r.N_age[k]
        mat_d[(red_mask,red_dist)] = t_maxI
        mat_maxI_d[(red_mask,red_dist)] = maxI

    # How to convert this information into a matrix
    print("mat_d= ", mat_d)
    print("mat_maxI_d= ", mat_maxI_d)
    red = np.linspace(0.,10,6) / 10.  # to avoid 0.3000004 <<<<< ERROR?
    for i,r1 in enumerate(red):
        for j,r2 in enumerate(red):
            try:
                mat[i,j] = mat_d[r1,r2]
                mat_maxI[i,j] = mat_maxI_d[r1,r2]
            except:
                print("pass")
                pass
    return mat, mat_maxI, N_age

#------------------------
def myImgshow(ax, mat, mat_maxI, cmap):
    nx = ny = 6
    xx = np.asarray(range(6)) / 5.
    yy = np.asarray(range(6)) / 5.
    plt.rcParams['image.cmap'] = 'gray'

    print("colors= ", _colors)
    print(dir(_colors))
    norm = _colors.LogNorm(vmin=0.005, vmax=1.0)

    def mycolor(val):
        #return plt.get_cmap(cmap)(val)
        return plt.get_cmap(cmap)(norm(val))

    colors = []
    rects  = []
    print("myImgshow:mat_maxI: ", mat_maxI.shape)
    print(mat_maxI)

    for x in range(nx):
        for y in range(ny):
            rect = mpatches.Rectangle((xx[x]-.1,yy[y]-.1), .2, .2) #, color=mycolor(mat[x,y]))
            rects.append(rect)
            colors.append(mycolor(mat[x,y]/30.))  # 140: max value of t_argmax

    pc = PatchCollection(rects) #, cmap='hot', array=None)
    pc.set(array=None, facecolors=colors)
    ax.add_collection(pc)


#------------------------
def plotTMaxInfections(df0, dct, title):
    filnm = "t_maxI_infectivity"
    for k,v in dct.items():
        filnm += "_%s=%3.2f" % (k,v)

    fig, axes = plt.subplots(4,5, figsize=(10,8))
    title += "\n"
    count = 0
    for k,v in dct.items():
        if count == 0:
            comma = ""
        else:
            comma = ","
        count += 1
        title += comma + " %s=%3.2f" % (k,v) # sm=%2.1f, sd=%2.1f, wm=%2.1f, wd=%2.1f" % (sm,sd,wm,wd))

    fig.suptitle(title)
    matplotlib.rc('xtick', labelsize=6)
    matplotlib.rc('ytick', labelsize=6)
    axes = axes.reshape(-1)

    #0  2   4  6  8  10
    names = ['0', '.2', '.4', '.6', '.8', '1.']
    #ticks = [0., 0.5, 1.0]
    ticks = np.linspace(0,10,6) / 10.

    for k in range(19):
        if 1:
            mat, mat_maxI, Nage = computeTMaxInfections(k, df0)
            ax = axes[k]
            ax.xaxis.set_ticks_position('bottom')
            myImgshow(ax, mat, mat_maxI, cmap='hot_r')
            #myImgshow(ax, mat, cmap='bwr')
            ax.set_xlim(-0.1, 1.1)
            ax.set_ylim(-0.1, 1.1)
            nx,ny = len(names), len(names)  # not general

            for i in range(nx):
              for j in range(ny):
                  #txt = "%3d" % mat_maxI[i,j]
                  txt = "%3d" % mat[i,j]
                  if mat[i,j] > 15: col = 'white'
                  else: col = 'black'
                  text = ax.text(i*0.2, j*0.2, txt, ha="center", va="center", color=col, fontsize=4)

            # Nage: nb of people in that age category
            ax.set_title("k=%d, N=%d" % (k, Nage))
            ax.set_xlabel("mask red.")
            ax.set_ylabel("distance red.")
            ax.set_xticklabels(names)
            ax.set_yticklabels(names)
            ax.set_xticks(ticks)
            ax.set_yticks(ticks)
            ax.tick_params(axis='both', which='both', length=0)
    plt.tight_layout()
    plt.savefig(filnm + ".pdf")
    
dfs0 = dfs[(.3,.3,.3,.3)]
dct = getParams(dfs0, group_key)
plotTMaxInfections(dfs0, dct, title="Time of maximum infectivity by age category")

dfs0 = dfs[(.7,.7,.7,.7)]
dct = getParams(dfs0, group_key)
plotTMaxInfections(dfs0, dct, title="Time of maximum infectivity by age category")

quit()
#-----------------------------------------------------