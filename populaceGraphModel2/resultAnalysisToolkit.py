import matplotlib.pyplot as plt
import pandas as pd
from  modelingToolkit import *
import seaborn as sns
import os 

#def getDegrees(model):
#    degrees = [len(graph[person]) for person in 
def filterEnvByType(envs, env_type):
    '''return all the household, workplace, or school type envs from a list'
    :envs dict:
    all the environments
    :env_type string:
    the name of the type of environment to search for
    :return dict:
    '''
    #return list(filter(lambda env: env.env_type == env_type, envs.values()))
    return dict(filter(lambda env: env[1].env_type == env_type, envs.items()))

def plotSIR(self, memberSelection = None):
    """
    For members of the entire graph, will generate three charts in one plot, representing the frequency of S,I, and R, for all nodes in each simulation
    """

    rowTitles = ['S','I','R']
    fig, ax = plt.subplots(3,1,sharex = True, sharey = True)
    simCount = len(self.sims)
    if simCount == []:
        print("no sims to show")
        return
    else:
        for sim in self.sims:
            title = sim[0]
            sim = sim[1]
            t = sim.t()
            ax[0].plot(t, sim.S())
            ax[0].set_title('S')

            ax[1].plot(t, sim.I(), label = title)
            ax[1].set_ylabel("people")
            ax[1].set_title('I')
            ax[2].plot(t, sim.R())
            ax[2].set_title('R')
            ax[2].set_xlabel("days")
    ax[1].legend()
    plt.show()

def getPeakPrevalences(self):
    return [max(sim[0].I()) for sim in self.sims]

#If a structuredEnvironment is specified, the partition of the environment is applied, otherwise, a partition must be passed
def plotBars(model, partition, partitioner, SIRstatus = 'R', normalized = False):
    """
    Will show a bar chart that details the final status of each partition set in the environment, at the end of the simulation
    :param environment: must be a structured environment
    :param SIRstatus: should be 'S', 'I', or 'R'; is the status bars will represent
    :param normalized: whether to plot each bar as a fraction or the number of people with the given status
    #TODO finish implementing None environment as entire graph
    """

    
    simCount = len(model.sims)
    partitionCount = partitioner.num_sets
    barGroupWidth = 0.8
    barWidth = barGroupWidth/simCount
    index = np.arange(partitionCount)

    offset = 0        
    for sim in model.sims:
            title = sim[0]
            sim = sim[1]

            totals = []
            end_time = sim.t()[-1]
            for index in partition:
                set = partition[index]
                if len(set) == 0:
                    #no bar if no people
                    totals.append(0)
                    continue
                total = sum(status == SIRstatus for status in sim.get_statuses(set, end_time).values()) / len(set)
                if normalized == True:  total = total/len(set)
                totals.append(total)

            #totals = sorted(totals)
            xCoor = [offset + x for x in list(range(len(totals)))]
            plt.bar(xCoor,totals, barWidth, label = title)
            offset = offset+barWidth
    #plt.legend()
    #plt.ylabel("Fraction of people with status {}".format(SIRstatus))
    #plt.xlabel("Age groups of 5 years")
    plt.show()
    plt.savefig(model.basedir+"/evasionChart.pdf")

def getR0(self):
    sim = self.sims[-1]
    herd_immunity = list.index(max(sim.I))
    return(self.population/sim.S([herd_immunity]))

def getDegreeHistogram(model, env_indexes, normalized = True): 
    """
    :param model: PopulaceGraph
    Produce a histogram of the populace 
    :param normalized: normalize the histogram if true. 
    """
    degreeCounts = [0] * 100
    for index in env_indexes:
        env = model.environments[index]
        people = env.members
        graph = model.graph.subgraph(people)
        
        for person in people:
            try:
                degree = len(graph[person])
            except:
                degree = 0
            degreeCounts[degree] += 1
    while degreeCounts[-1] == 0:
        degreeCounts.pop()
    return degreeHistogram

def getMaxDegreeNodes(model, env_indexes):
    degrees = {}
    for index in env_indexes:
        env = model.environments[index]
        people = env.members
        graph = model.graph.subgraph(people)
        deg_seq = sorted([d for n, d in graph.degree()], reverse = True)        
        degrees[index] = max(deg_seq)
    return degrees
    
def aFormatGraph(model, folder):
    ageGroups = [[0,5], [5,18], [18,50], [50,65], [65,100]]
    enumerator = {}
    try:
        os.mkdir(folder)
    except:
        pass
    for i, group in enumerate(ageGroups):
        enumerator.update({j:i for j in range(group[0], group[1])})    
    open(folder +"/nodes.txt","a").writelines(["{} {}\n".format(item,enumerator[model.populace[item]['age']])  for item in model.graph.nodes()])
    with  open(folder +"/edges.txt","a") as file:
        adj = model.graph.adj
        for edgeA in adj:            
            for edgeB in adj[edgeA]:
                file.writelines("{} {} {}/n".format(edgeA,edgeB, adj[edgeA][edgeB]['transmission_weight']))
                file.writelines("{} {} {}/n".format(edgeB,edgeA, adj[edgeA][edgeB]['transmission_weight']))
            
