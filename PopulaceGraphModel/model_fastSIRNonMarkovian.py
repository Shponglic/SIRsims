import networkx as nx
import EoN as eon
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import random
import distributions as dist
import plot_utils

# Experiment with setting up an array of infectivities and an array of 
# recoveries. The infectivities beta[] will be a Poisson-Gamma mixture
# choose beta from a Gamma, and use that Beta for the individual



def processTransmissionTimes(trans):
    # trans is a list of tuples (t,u,v): u infected v at time t
    # Times are ordered chronologically

    # I would like to identify the time u got infected. 
    # Sort the list by t

    # Given tu = (t,u,v), find the infection time of u
    # For that, must find (t1, u1, u). Note that t1 < t
    # Need dictionary: who[u] to return u1

    t0,u0,v0 = trans[0]
    # u can infect multiple v
    # I should be able to construct the forward generation (fg) histogram across 
    # the population and binned per day. 
    # Also create dictionary of when each node is infected

    fwd = defaultdict(list)
    infect = defaultdict(float)

    for t,u,v in trans:
        fwd[u].append((t,v))
        infect[v] = t

    # construct time intervals
    gen_times = []
    for t,u,v in trans:
        #if infect[u] > 0.1: # (early times)
        if 1:
            gen_times.append(infect[v] - infect[u])

    # construct time bins

    # superimpose a range of exponentials (theoretically the case)
    def expf(t, lmbda):
        return lmbda * np.exp(-lmbda*t)

    mean = np.mean(gen_times)
    var = np.var(gen_times)
    std = np.sqrt(np.var(gen_times))
    print("mean/var(gen_times)= ", mean, var)
    print("std= ", std)

    t = np.linspace(0., 0.6, 20)
    plt.hist(gen_times, density=True, bins=50)
    #for v in [10,12,14,16,18,20]:
    plt.plot(t, expf(t, 1./mean), label=mean)
    plt.legend(fontsize=10)
    plt.show()
    quit()

    #print("fwd= ", fwd)
    #for k,v in fwd.items():
        #print(k,v)

    # Create a histogram of how many people a person infected
    nb_people = defaultdict(int)
    for k,v in fwd.items():
        nb = len(v)
        nb_people[nb] += 1

    # Sort by keys
    lst = sorted(zip(nb_people.keys(), nb_people.values()))
    x, y = zip(*lst)
    print(lst)
    #plt.bar(x, y)
    #plt.show()


#---------------------------------

# Example SIR model using fast_SIR, based on an N to N graph (homogeneous connections. The graph is undirected. Transmission and recovery parameters are constant. 

# last_time approx 50
# very small mean(Tg) (=0.002)
R0 = 2.5
k = 0.7
beta = .2 * R0   # Transmission (S ->beta -> I) [1/days] (per edge)
gamma = .2  # Recovery (I -> R)  [1/days]  (per node)

print("1\/beta = %3.1f days" % (1./beta))
print("1\/gamma = %3.1f days" % (1./gamma))

# A graph with N nodes has N*(N-1)/2 edges. 

N = 100000  # number of nodes
#G = nx.complete_graph(N)
#G = nx.erdos_renyi_graph(N, p=0.01, directed=False)
G = nx.barabasi_albert_graph(N,10)
#G = nx.erdos_renyi_graph(N, 0.03)
nb_nodes = G.number_of_nodes()

# Other useful graphs you could create
"""
G = nx.erdos_renyi_graph(n, p[, seed, directed])
G = nx.complete_bipartite_graph(3, 5)
G = nx.watts_strogatz_graph(n, k, p[, seed])
G = nx.barabasi_albert_graph(n, m[, seed])
"""

print("num nodes: ", G.number_of_nodes())
print("num edges: ", G.number_of_edges())

initial_recovered = []

# Single initial infected. Does not matter which one since the graph 
# is fully-connected
initial_infected = list(range(1,30))
print(initial_infected)

tmin, tmax = 0., 100000.
# HOW DO I CHOOSE THE PARAMETERS OF THE GAMMA?
#gammas = dist.gamma(R0, k, nb_nodes)
gammas = dist.gamma(beta, k, nb_nodes)

# Mean of gamma should
# Compose this gamma with Poisson: 
# int_0^inf p(beta|lambda) * p(lambda; alpha,beta) dlambda
# = int_0^inf Pois(beta


#plt.plot(bins, y, linewidth=2, color='r')
#plt.show()
#quit()

# The gamma will be a distribution of rates (beta)

def transTime(nodeA, nodeB, rates):
    rate = rates[nodeA]
    #print("nodeA, nodeB, rate= ", nodeA, nodeB, rate)
    #print("nodeA= ", nodeA)
    #return random.expovariate(beta) # (works)
    if rate > 0:
        return random.expovariate(rates[nodeA])
    else:
        return float('Inf')

def recovTime(nodeA, rate):
    #rate = rates[nodeA]
    return random.expovariate(gamma) # (works)
    if rate > 0:
        return random.expovariate(rate)
    else:
        return float('Inf')



#sim_result = eon.fast_SIR(G, beta, gamma, initial_infecteds=initial_infected, 
sim_result = eon.fast_nonMarkov_SIR(G, 
        trans_time_fxn = transTime, 
        trans_time_args = (gammas,),
        rec_time_fxn = recovTime,
        rec_time_args = (gamma,),
        #initial_infecteds=initial_infected, 
        rho=0.001,
        initial_recovereds=initial_recovered, 
        #transmission_weight="tw", 
        #recovery_weight="rw",
        return_full_data = True, 
        tmin=tmin, tmax=tmax)

plot_utils.plotPopulaceSIR(sim_result)
plt.show()
quit()

print("==> after simResult")
# All the times for which there is data. Print out to see: floats. 
times = sim_result.t()
statuses = {}
last_time = times[-1]
print("last_time= ", last_time)

# trans: list of (t,u,v): u infected v at time t
trans = sim_result.transmissions()
trans_tree = sim_result.transmission_tree()
print("nodes: ", trans_tree.number_of_nodes())
print("edges: ", trans_tree.number_of_edges())
processTransmissionTimes(trans)
quit()

# Specify the times you wish to work with. Tix is the time index. 
for tix in range(0, int(last_time)+2, 1):
    # statuses[tix]: for each node of the graph, S,I,R status at time tix
    # this is computed by some internal interpolation
    statuses[tix] = sim_result.get_statuses(time=tix)
    #print("time= ", tix, "  status= ", statuses[tix])

# statuses.keys() tells you the times at which data is saved.
# statuses[2] is a dictionary of the network at t=2., for example: 
# status[2] = {0: 'I', 1: 'I', 2: 'I', 3: 'R', 4: 'R', 5: 'R', 6: 'R', 7: 'R', 8: 'I', 9: 'I', 10: 'I', 11: 'I', 12: 'I', 13: 'R', 14: 'I', 15: 'I', 16: 'R', 17: 'R', 18: 'R', 19: 'R', 20: 'R', 21: 'I', 22: 'I', 23: 'I', 24: 'R', 25: 'R', 26: 'R', 27: 'R', 28: 'R', 29: 'I'}

# Together with lat/long for each node, you can plot the map. 

# Synthetic lat/long (random chocie among [0,100])
x = np.random.choice(100, nb_nodes, replace=False)
y = np.random.choice(100, nb_nodes, replace=False)

# Replace: 'S', 'I', 'R' by 0, 1, 2 in statuse

subst_dict = {'S':0,  'I':1, 'R':2}
print("keys: ", statuses[1].keys())
print("last_time= ", last_time)

def replace(time_index):
    d = np.zeros(len(statuses[0]), dtype='int')
    for k,v in statuses[time_index].items():
        d[k] = subst_dict[v]
    return d

#for i in range(0, int(last_time), 2):
#    print(replace(i))

# Notes: https://stackoverflow.com/questions/14827650/pyplot-scatter-plot-marker-size
nrows = 6
ncols = 6
plt.subplots(ncols,nrows)

import matplotlib as mpl
# Map 0,1,2 to black, red, green
cmap = mpl.colors.ListedColormap(['black', 'red', 'green'])
c_norm = mpl.colors.BoundaryNorm(boundaries=[-0.5, 0.5, 1.5, 2.5], ncolors=4)


for i in range(nrows*ncols):
    plt.subplot(ncols,nrows,i+1)
    try:
        col = replace(2*i)
    except:
        break
    plt.scatter(x, y, c=col, s=3*3*3, cmap=cmap, norm=c_norm, alpha=0.5)
plt.colorbar()
plt.show()
