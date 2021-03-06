from ge1_modelingToolkit import *
import copy
import os
import glob
# OS independence
from shutil import copyfile

# In this study, we vaccinate 30% and 70% of the population by making them recovered. 
# We then calculate SIR curves for each age group. 

# Time stamp to identify the simulation, and the directories where the data is stored
# Called in constructor of Partioning graph (used to be called in simulate() method
timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

# os has operating system aware functions (provided by Derek)
# https://urldefense.com/v3/__https://www.geeksforgeeks.org/python-os-mkdir-method/__;!!PhOWcWs!nUjA_KItpfwobIwE_tQ_ogPwde2wU4O0EeqeEL0s7bv6kOvIMGkiWbnCzzMVIh3blQ$ 


#################################################################################
#####   Begin setting up model variables  #######################################
#################################################################################

# Run with 10% of the data: slim=True
# Run with all the data: slim=False
slim = False
slim = True
print("slim= ", slim)

# Whether or not to save output files  <<<<<<<<<<<<<< Set to save directory
save_output = True
save_output = False
print("save_output= ", save_output)

#These values scale the weight that goes onto edges by the environment type involved
# Parameters less than 1 reduce the infectivity
default_env_scalars   = {"school": 1.0, "workplace": 1.0, "household": 1.0}

#As None, the degrees of the environment are implicit in contact matrices
env_degrees           = {'workplace': None, 'school': None}

# Binary variables. Either there are preventions or not

# Preventions refer to the fraction of the population wearing masks, or the fraction of 
# the population practicing social distancing. 

# these numbers refer to the fraction of people with masks and distancing
workplace_preventions = {'masking': 0.0, 'distancing': 0}

#the prevention measures in the schools
school_preventions    = {'masking':0,  'distancing': 0}

#the prevention measures in the schools
household_preventions = {'masking':0,  'distancing': 0}

# Dictionary of dictionaries
#combine all preventions into one var to easily pass during reweight and build
preventions = {'workplace': workplace_preventions, 
               'school': school_preventions, 
               'household': household_preventions}

# Parameters found by Dustin
# these values specify how much of a reduction in weight occurs when people are masked, or distancing
# These parameters are global and not per environment because people carry their own mask
prevention_reductions = {'masking': 0.1722, 'distancing': 0.2071}

#this object holds rules and variables for choosing weights
trans_weighter = TransmissionWeighter(default_env_scalars, prevention_reductions)

# https://epidemicsonnetworks.readthedocs.io/en/latest/functions/EoN.fast_SIR.html
# Argument to EoN.fast_SIR(G, tau, gamma, initial_infecteds=None,
gamma  = 0.2  # Recovery rate per edge (EoN.fast_SIR)
tau    = 0.2  # Transmission rate per node (EoN.fast_SIR) (also called beta in the literature)

#################################################################################
#####   End setting up model variables  #######################################
#################################################################################

if save_output:
    dstdirname = os.path.join(".","ge_simResults", timestamp, "src")
    os.makedirs(dstdirname)
    # Independent of OS
    os.system("cp *.py %s" % dstdirname)  # not OS independent
    #copyfile ('ge1_modelToolkit.py',os.path.join(dstdirname,'ge1_modelToolkit.py'))

#the partioner is needed to put the members of each environment into a partition,
#currently, it is setup to match the partition that is implicit to the loaded contact matrices
enumerator = {i:i//5 for i in range(75)}
enumerator.update({i:15 for i in range(75,100)})
names = ["{}:{}".format(5 * i, 5 * (i + 1)-1) for i in range(15)]
names.append("75-100")
partition = Partitioner('age', enumerator, names)
print("Age brackets: ", names)

# Create Graph

#init, build simulate
model = PopulaceGraph(partition, timestamp, slim=slim, save_output=save_output)
model.build(trans_weighter, preventions, prevention_reductions, env_degrees)
vacc_perc = 0.0
model.vaccinatePopulace(vacc_perc)
model.infectPopulace(0.001)
model.simulate(gamma, tau, title = 'base-test')
#----------------------------

# Create a range of simulation
# 'masking': 0.1, 0.2, 0.3, 0.4
# 'distancing': 0.0, 0.1, 0.2, 0.3, 0.4

#-------------------------------------------------------------------
def reduction_study(s_mask, s_dist, w_mask, w_dist):
    # Probably had no effect since masking and distancing initially set to zero
    # if s_mask = 0, prevention_reduction in school masks won't have an effect, but will in the workforce
    prevent = copy.deepcopy(preventions)
    prevent['school']['masking'] = s_mask
    prevent['school']['distancing'] = s_dist
    prevent['workplace']['masking'] = w_mask
    prevent['workplace']['distancing'] = w_dist
    # value of zero indicate that masking and social distancing have no effect.
    reduce_masking    = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0];  # np.linspace(0., 1.0, 6)
    reduce_distancing = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0];
    # If s_mask == 0, 
    for m in reduce_masking:
        for d in reduce_distancing:
            #print("m,d= ", m,d)
            prevention_reductions = {'masking': m, 'distancing': d} 
            #print("script: preventions: ", prevent)
            #print("script: prevention_reductions: ", prevention_reductions)
            trans_weighter.setPreventions(prevent)   #### where does Bryan apply self.preventions = preventions? *******
            trans_weighter.setPreventionReductions(prevention_reductions)
            model.reweight(trans_weighter, prevent, prevention_reductions)  # 2nd arg not required because of setPreventions
            title = "red_mask=%4.2f,red_dist=%4.2f,sm=%3.2f,sd=%3.2f,wm=%3.2f,wd=%3.2f" % (m, d, s_mask, s_dist, w_mask, w_dist)
            model.simulate(gamma, tau, title=title, preventions=preventions, prevention_reductions=prevention_reductions)

#-------------------------------------------------------------------
def vaccination_study(s_mask, s_dist, w_mask, w_dist):
    # Probably had no effect since masking and distancing initially set to zero
    # if s_mask = 0, prevention_reduction in school masks won't have an effect, but will in the workforce
    prevent = copy.deepcopy(preventions)
    prevent['school']['masking'] = s_mask
    prevent['school']['distancing'] = s_dist
    prevent['workplace']['masking'] = w_mask
    prevent['workplace']['distancing'] = w_dist
    # value of zero indicate that masking and social distancing have no effect.
    reduce_masking    = [0.5]
    reduce_distancing = [0.5]
    # Vaccination across the entire population
    percent_vaccinated = [0., 0.25, 0.50, 0.75,1.00]
    percent_vaccinated = [0.75, 0.99] # In the workplace
    percent_vaccinated = [0.99] # In each workplace
    # If s_mask == 0, 
    print("enter vaccination study")
    for v in percent_vaccinated:  # not used
     for m in reduce_masking:
      for d in reduce_distancing:
       #for nb_wk in [10, 25, 50,100,200,400,600,800]:
       #for nb_wk in [10, 25, 50, 100]:
       #for nb_wk in [1000]:
       #for nb_wk in [0, 10, 25, 50, 100, 1000, 5000, 10000, 15000]:
       for nb_wk in [0]:
        #for nb_sch in [0]:
        for nb_sch in [0, 5, 10, 20, 40, 60]:
         #for v_pop_perc in [0., 0.25, 0.5, 0.75, 0.99]
         for v_pop_perc in [0.0]:
            model.resetVaccinated_Infected() # reset to default state (for safety)
            prevention_reductions = {'masking': m, 'distancing': d} 
            trans_weighter.setPreventions(prevent)   #### where does Bryan apply self.preventions = preventions? *******
            trans_weighter.setPreventionReductions(prevention_reductions)
            model.infectPopulace(perc=0.001)
            print("--------  nb workplaces to vaccinate: %d" % nb_wk)
            # I should be able to set 2nd argument to 1  (nb workplaces, perc vaccinated)
            model.set_nbTopWorkplacesToVaccinate(nb_wk, 1.00)  # should set in term of number of available vaccinations
            # make sure first index < nb schools (nb schools, perc vaccinated)vaccinated)
            model.set_nbTopSchoolsToVaccinate(nb_sch, 1.00)  # should set in term of number of available vaccinations
            model.vaccinatePopulace(perc=v_pop_perc)  # random vaccination of populace
            model.reweight(trans_weighter, prevent, prevention_reductions)  # 2nd arg not required because of setPreventions
            # The file data will be stored in a pandas file
            title= "red_mask=%4.2f,red_dist=%4.2f,sm=%3.2f,sd=%3.2f,wm=%3.2f,wd=%3.2f,v=%3.2f" % (m, d, s_mask, s_dist, w_mask, w_dist, v)
            model.simulate(gamma, tau, title=title, preventions=preventions, prevention_reductions=prevention_reductions)
    return 
#-------------------------------------------------------------------

s_mask = [0.0, 0.0]  # percentage of people wearing masks in schools
s_dist = [0.3, 0.3]  # percentage of people social distancing in schools
w_mask = [0.7, 0.3]  # percentage of people wearing masks in the workplace
w_dist = [1.0, 1.0]  # percentage of people social distancing in the workplace

levels = [0.0, 0.25, 0.5, 0.75, 1.0]  # different values of s_mask
levels = [0.5]  # different reduction values of s_mask

# 5 levels of vaccination, 5 levels of mask and social distancing. 25 cases.
for level in levels:
    sm, wm, sd, wd = [level] * 4
    print("before enter vaccination study")
    vaccination_study(sm, sd, wm, wd)

quit()

