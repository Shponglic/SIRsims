
from ModelToolkit import *
import plot_graph as pg

#################################################################################
#####   Begin setting up model variables  #######################################
#################################################################################

default_env_scalars   = {"school": 0.3, "workplace": 0.3, "household": 1}
env_degrees           = {'workplace': None, 'school': None}
default_env_masking   = {'workplace': 0, 'school':0, 'household': 0}

workplace_preventions = {'masking': 0, 'distancing': 0}
school_preventions    = {'masking':0, 'distancing': 0}
household_preventions = {'masking':0, 'distancing':0}
preventions           = {'workplace': workplace_preventions, 'school': school_preventions, 'household': household_preventions}
prevention_reductions = {'masking': 0.1722, 'distancing': 0.2071}# dustins vals
trans_weighter        = TransmissionWeighter(default_env_scalars, prevention_reductions)
gamma                 = 0.1
tau                   = 0.08

enumerator            = {i:i//5 for i in range(75)}
enumerator.update({i:15 for i in range(75,100)})
names                 = ["{}:{}".format(5 * i, 5 * (i + 1)) for i in range(15)]
partition             = Partitioner('age', enumerator, names)

# Choose one or the other model
which_model           = 'random_GE'    # GE contact graph algorithm
#which_model          = 'strogatz_AB'  # AB contact graph algorithm (makeGraph)

#################################################################################
#####   End setting up model variables  #######################################
#################################################################################

model = PopulaceGraph(partition, slim = True)

# Select graph generation model
if which_model == 'strogatz_AB':
    model_func = model.clusterPartitionedStrogatz
    name = "Strogatz (Bryan)"
elif which_model == 'random_GE':
    model_func = model.clusterPartitionedRandom
    name = "random edge selection (Gordon)"

# Create the network graph according to model_func
model.build(trans_weighter, preventions, env_degrees, alg = model_func)

env_schools_func    = lambda environment: model.environments[environment].quality == 'school'
env_workplaces_func = lambda environment: model.environments[environment].quality == 'workplace'
env_pop_func        = lambda environment: model.environments[environment].population

schools    = sorted(list(filter(env_schools_func,    model.environments)), key = env_pop)
workplaces = sorted(list(filter(env_workplaces_func, model.environments)), key = env_pop)

for index in reversed(schools):
    print("pop= ", model.environments[index].population)

# Plot two graphs of #nodes vs degree: schools and workplaces
pg.plotGraph(model, schools, workplaces)
quit()

