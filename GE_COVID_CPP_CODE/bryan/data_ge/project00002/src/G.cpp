#include <iostream>
#include "stdlib.h"
#include "G.h"
// contains states R,S,L,... that are also defined in earlier
// include files. So must be called last
#include "cxxopts.hpp"
#include "states.h"
#include <vector>
using namespace std;

#define EXP 1       // Exponential distribution of infection times
#define CONST_INFECTION_TIME    // constant recovery time

#define INFECTION_TIME 8

#define VACCINATE_LATENT 
//#undef VACCINATE_LATENT 

// If defined, setup up vaccinated people before the simulation begins. 
// Set these vaccinated people to the R state
#define SETUPVACC
#undef SETUPVACC


// define VARBETA to enable time-dependent transmissibility
#define VARBETA
//#undef VARBETA

// If VARBETA is defined, compute each infectivity profile on the fly
// otherwise, use a precomputed infectivity profile.
#define INDIV_VAR


using namespace std;

G::G(Params& p, Files& f, Lists& l, Counts& c, Network& n, GSL& gsl) : par(p), files(f), lists(l), net(n), counts(c), gsl(gsl)
{
}

//----------------------------------------------------------------------
void G::initialize(int argc, char** argv, Files& files, Params& params, Lists& lists, Network& network, GSL& gsl)
{
  int seed;

  params.n_runs = 100;
  params.n_runs = 10;
  params.n_runs = 5;
  params.n_runs = 2;
  params.n_runs = 1;
  //parameters = 0;

  seed = time(NULL);
  initRandom(seed, gsl);

  // the Results folder contains parameters.txt, result files
  // the Data folder contains network data
  // executable data_folder result_folder
 
  // reading the parameter file comes before argument parsing. Therefore, 
  // I am hardcoding the name of the parameter file
  strcpy(files.parameter_file, "data_ge/");
  strcat(files.parameter_file, "parameters_0.txt");
  readParameters(files.parameter_file, params);
  parse(argc, argv, params);

#if 0
  if (argc != 3) {
	printf("To run the code, \n");
	printf("  ./executable DataFolder ResultFolder\n");
	printf("-------------------------------------\n");
	exit(1);
  }
#endif

  //strcpy(files.data_folder, argv[1]);
  strcpy(files.data_folder, "data_ge");
  strcat(files.data_folder, "/");
  //strcpy(files.result_folder, argv[2]);
  strcpy(files.result_folder, "data_ge/results/");
  strcat(files.result_folder, "/");

  //strcpy(files.parameter_file, files.result_folder);
  //strcat(files.parameter_file, "parameters_0.txt");
  //printf("parameter_file= %s\n", files.parameter_file); 

  strcpy(files.node_file, files.data_folder);
  strcat(files.node_file, "nodes.txt");
  printf("**** node_file: %s\n", files.node_file);

  strcpy(files.network_file, files.data_folder);
  strcat(files.network_file, "network.txt");
  printf("**** network_file: %s\n", files.network_file);

  strcpy(files.vaccination_file, files.data_folder);
  strcat(files.vaccination_file, "vaccines.csv");

  allocateMemory(params, lists);

  readData(params, lists, network, files);
  setBeta(params);
  printf("end initialize: params.N= %d\n", params.N);
}

//----------------------------------------------------------------------
void G::runSimulation(Params& params, Lists& lists, Counts& c, Network& n, GSL& gsl, Files& f)
{
  for (int run=0; run < params.n_runs; run++) {
     printf("run= %d\n", run);
     {
  		c.count_l_asymp   = 0;
  		c.count_l_symp    = 0;
  		c.count_l_presymp = 0;
  		c.count_i_symp    = 0;
  		c.count_recov     = 0;
  		c.count_vacc1     = 0;
  		c.count_vacc2     = 0;

        init(params, c, n, gsl, lists, f);

        while(lists.n_active > 0) {
	      spread(run, f, lists, n, params, gsl, c);
		}

  		printf("total number latent symp:  %d\n", c.count_l_symp);
  		printf("total number latent presymp:  %d\n", c.count_l_presymp);
  		printf("total number infectious symp:  %d\n", c.count_i_symp);
  		printf("total number recovered:  %d\n", c.count_recov);
  		printf("total number vacc 1:  %d\n", c.count_vacc1);
  		printf("total number vacc 2:  %d\n", c.count_vacc2);
        printResults(run, lists, f);

		printTransitionStats();
		writeStates(c);
     }
  }
}
//----------------------------------------------------------------------
//spread
void G::countStates(Params& params, Counts& c, Network& n, Files& f)
{
  c.countS  = 0;
  c.countL  = 0;
  c.countIS = 0;
  c.countR  = 0;

  for(int i=0;i < params.N; i++) {
    if (n.node[i].state == S)  c.countS++;
    if (n.node[i].state == L)  c.countL++;
    if (n.node[i].state == IS) c.countIS++;
    if (n.node[i].state == R)  c.countR++;
  }

  c.cS.push_back(c.countS);
  c.cL.push_back(c.countL);
  c.cIS.push_back(c.countIS);
  c.cR.push_back(c.countR);
  c.cvacc1.push_back(c.count_vacc1);
  c.cvacc2.push_back(c.count_vacc2);
  c.times.push_back(f.t);

  printf("time= %f=\n", f.t);
  printf("Counts: t,S,L,IS,R,V1,V2: %3.1f, %d, %d, %d, %d, %d, %d\n", f.t, c.countS, c.countL, c.countIS, c.countR, c.count_vacc1, c.count_vacc2);
}
//----------------------------------------------------------------------
void G::writeStates(Counts& c)
{
	FILE* fd = fopen("counts.csv", "w");

	int sz = c.cvacc1.size();
	fprintf(fd, "time,S,L,IS,R,vacc1,vacc2\n");

	for (int i=0; i < sz; i++) {
		fprintf(fd, "%4.2f,%d,%d,%d,%d,%d,%d\n", 
		  c.times[i], c.cS[i], c.cL[i], c.cIS[i], c.cR[i], c.cvacc1[i], c.cvacc2[i]);
	}
	fclose(fd);
}
//----------------------------------------------------------------------
void G::init(Params& p, Counts& c, Network& n, GSL& gsl, Lists& l, Files& f)
{
  resetVariables(l, f);
  resetNodes(p, n);
  
  //Start
  seedInfection(p, c, n, gsl, l, f);
}
//----------------------------------------------------------------------
void G::seedInfection(Params& par, Counts& c, Network& n, GSL& gsl, Lists& l, Files& f)
{
  int seed;
  int id;

  f.t = 0.0;
  f.it = 0;
 
  // Use a permutation to make sure that there are no duplicates when 
  // choosing more than one initial infected
  gsl_permutation* p = gsl_permutation_alloc(par.N);
  gsl_rng_env_setup();
  gsl.T = gsl_rng_default;
  gsl.r_rng = gsl_rng_alloc(gsl.T); // *****
  // Set up a random seed
  gsl_rng_set(gsl.r_rng, time(NULL));
  gsl_permutation_init(p);
  // MAKE SURE par.N is correct
  gsl_ran_shuffle(gsl.r_rng, p->data, par.N, sizeof(size_t));


// Initialize Susceptibles. List not needed if there are no vaccinations
  for (int i=0; i < par.N; i++) {
	 // susceptibles have been subject to a random permution 
	 id = p->data[i];  
	 // Nodes in randomized order to randomize batch vaccinations
	 l.permuted_nodes.push_back(p->data[i]);
     n.node[i].state = S;
     addToList(&l.susceptible, id); 
  }

  printf("inside seedInf\n\n\n");

#ifdef SETUPVACC
  int nb_vaccinated = l.people_vaccinated.size();
  int maxN = l.susceptible.n;

  //for (int i=0; i < l.susceptible.n; i++) {
  for (int i=0; i < maxN; i++) {
	id = l.susceptible.v[i];
  	int j = removeFromList(&l.susceptible, id);
  }

  // MIGHT HAVE TO FIX THIS
  for (int i=0; i < nb_vaccinated; i++) {
	    int id = l.people_vaccinated[i];
	  	//n.node[id].state = V1;
		n.node[id].t_V1 = 0.;
		// I should remove the correct person from the list of susceptibles
		// Not correct. Only works if loop above is over the same list one
		// is removing from. 
	    //int j = removeFromList(&l.susceptible, i);
        //addToList(&l.vacc1, j); 
  }

  // set Recovered to those vaccinated
  for (int i=0; i < nb_vaccinated; i++) {
	    int person_vaccinated = l.people_vaccinated[i];
	  	n.node[person_vaccinated].state = R;
  }
#endif

  float rho = 0.001; // infectivity percentage at t=0 (SHOULD BE IN PARAMS)
  int ninfected = rho * par.N;
  //ninfected = 1;
  printf("N= %d, ninfected= %d\n", par.N, ninfected);

  for (int i=0; i < ninfected; i++) { 
  	seed = p->data[i];  // randomized infections
  	n.node[seed].state = L;
    addToList(&l.latent_symptomatic, seed); // orig 
	// Not sure. 
	//removeFromList(&l.susceptibles, i);  // i is index to remove
  }
 

  // Count number in recovered state
  int count = 0;
  for (int i=0; i < par.N; i++) {
      if (n.node[i].state == R) { count++; }
  }
  printf("nb recovered: %d\n", count);

  gsl_permutation_free(p);  // free storage

  l.n_active = ninfected;
  c.count_l_symp += ninfected;
  printf("added %d latent_symptomatic individuals\n", ninfected);

  // Assumes that dt = 0.1. Print every 10 time steps.
  if (f.it % 10 == 0) {
  	countStates(par, c, n, f);
  }
}
//----------------------------------------------------------------------
//void G::spread(int){}
void G::spread(int run, Files& f, Lists& l, Network& net, Params& params, GSL& gsl, Counts& c)
{  
  resetNew(l);
  float cur_time = f.t;

  // Vaccinate people at specified daily rate
  // S to V1
  vaccinations(params, l, gsl, net, c, cur_time);

  // Transition from first to second batch
  secondVaccination(par, l, gsl, net, c, cur_time);

  // S to L
  //printf("before infection()\n"); countStates(params, c, net);
  infection(l, net, params, gsl, c, cur_time);
  // L to P
  //printf("before latency()\n"); countStates(params, c, net);
  latency(params, l, gsl, net, c, cur_time);
  // P to I
  //preToI();
  // I to R
  //printf("before IsTransition()\n"); countStates(params, c, net);
  IsTransition(params, l, net, c, gsl, cur_time);

  countStates(par, c, net, f);  // Might be too much cost

  updateLists(l, net);
  //printf("----------------------------------------\n");
  updateTime();

  //Write data
  fprintf(f.f_data,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %f\n",
	l.susceptible.n, 
	l.latent_asymptomatic.n, 
	l.latent_symptomatic.n, 
	l.infectious_asymptomatic.n, 
	l.pre_symptomatic.n, 
	l.infectious_symptomatic.n, 
	l.home.n, 
	l.hospital.n, 
	l.icu.n, 
	l.recovered.n, 
	l.new_latent_asymptomatic.n, 
	l.new_latent_symptomatic.n, 
	l.new_infectious_asymptomatic.n, 
	l.new_pre_symptomatic.n, 
	l.new_infectious_symptomatic.n, 
	l.new_home.n, 
	l.new_hospital.n, 
	l.new_icu.n, 
	l.new_recovered.n, 
	l.new_vacc1.n, 
	l.new_vacc2.n, 
	run,
    f.t
	);
}
//----------------------------------------------------------------------
void G::vaccinations(Params& par, Lists& l, GSL& gsl, Network &net, Counts& c, float cur_time)
{
  // SOME KIND OF ERROR. MUST LOOK CAREFULLY AT DEFINITIONS OF RATES
  // Poisson  Pois(lambda), mean(lambda). So lambda is in number/time=rate
	printf("par.vacc1_rate= %f\n", par.vacc1_rate);
  int n_to_vaccinate = gsl_ran_poisson(gsl.r_rng, par.vacc1_rate*par.dt);
  
	//printf("Pois, n_to_vaccinate: %d\n", n_to_vaccinate);
  printf("n_vaccinated: %u \n", n_to_vaccinate);  
  printf("par.vacc1_rate: %f", par.vacc1_rate);
  printf("par.dt: %f", par.dt);
  printf("par.dt*par.vacc1_rate: %f", par.dt*par.vacc1_rate);
  vaccinateNextBatch(net, l, c, par, gsl, n_to_vaccinate, cur_time);
}
//----------------------------------------------------------------------
// Same as vaccinateNextBatch, but vaccinate a S/L neighbor of the person chosen
void G::vaccinateAquaintance(Network& net, Lists& l, Counts& c, Params& par, GSL& gsl, int n, float cur_time) {
	if (net.start_search == par.N) return;
	if (c.count_vacc1 > par.max_nb_avail_doses) return;

	int count = 0;
	for (int i=net.start_search; i < par.N; i++) {
		int id = l.permuted_nodes[i];  // This allows vaccinations in randomized order
		// Both Latent (no visible symptoms) and Susceptibles can be infected
		int state = net.node[id].state;
#ifdef VACCINATE_LATENT
		if ((state == S || state == L) && net.node[id].is_vacc == 0) {
#else
		if (net.node[id].state == S && net.node[id].is_vacc == 0) {
#endif
			net.start_search++;
			count++;
		    c.count_vacc1++;
			net.node[id].is_vacc = 1;
			net.node[id].t_V1 = cur_time;
			// constant in time
	        net.node[id].beta_IS = par.beta[IS] * (1.-par.vacc1_eff); 

			// perhaps have a new entry in the data file for par.vacc1_recov_eff?
			// mu is a rate in [1/days]
	        net.node[id].mu   = par.mu / (1.-par.vacc1_recov_eff); 

			// Probability of vaccine effectivness
			// if effectiveness is 1., else branch is always true
	        float prob = par.dt * par.vacc1_eff;
	        if (gsl_rng_uniform(gsl.random_gsl) < prob) {
        		net.node[id].vacc_infect  = 0.;  // transmission rate to others 
        		net.node[id].vacc_suscept = 0.;  // susceptibility to infection
			} else {
        		net.node[id].vacc_infect  = 1.;  // transmission rate to others 
        		net.node[id].vacc_suscept = 1.;  // susceptibility to infection
			}

			addToList(&l.new_vacc1, id);
		    stateTransition(id, id, S, V1, 0., cur_time); 
		}
		if (count >= n) break;
	}
	if (count < n) {
		printf("Insufficient Susceptibles to Vaccinate\n");
	}
}
//----------------------------------------------------------------------
void G::vaccinateNextBatch(Network& net, Lists& l, Counts& c, Params& par, GSL& gsl, int n, float cur_time) {
// Vaccinate n susceptibles (state == S)

	if (net.start_search == par.N) return;
	printf("top of NextBatch: %d/%d\n", c.count_vacc1, par.max_nb_avail_doses);
	if (c.count_vacc1 > par.max_nb_avail_doses) {
		printf("c.count_vacc1 > par.max_nb_avail_doses\n");
		return;
	}
	printf("continue processing NextBatch\n");

	// Count the number of of Susceptibles 
#if 0
	int nb_S = 0;
	for (int i=net.start_search; i < par.N; i++) {
		if (net.node[i].state == S) nb_S++;
	}
	printf("vaccinateNextBatch, tot nb S: %d\n", nb_S);
#endif

	int count = 0;
	for (int i=net.start_search; i < par.N; i++) {
		int id = l.permuted_nodes[i];  // This allows vaccinations in randomized order
		// Both Latent (no visible symptoms) and Susceptibles can be infected
		int state = net.node[id].state;
#ifdef VACCINATE_LATENT
		if ((state == S || state == L) && net.node[id].is_vacc == 0) {
#else
		if (net.node[id].state == S && net.node[id].is_vacc == 0) {
#endif
			net.start_search++;
			count++;
		    c.count_vacc1++;
			//net.node[id].state = V1;
			net.node[id].is_vacc = 1;
			net.node[id].t_V1 = cur_time;
			// constant in time

			// Higher vaccine effectiveness decreases the transmission rate
	        net.node[id].beta_IS = par.beta[IS] * (1.-par.vacc1_eff); 

			// Higher vaccine effectiveness increases the recovery rate
	        net.node[id].mu   = par.mu / (1.-par.vacc1_recov_eff); 

			// Probability of vaccine effectivness
			// if effectiveness is 1., else branch is always true
	        float prob = par.dt * par.vacc1_eff;
	        if (gsl_rng_uniform(gsl.random_gsl) < prob) {
        		net.node[id].vacc_infect  = 0.;  // transmission rate to others 
        		net.node[id].vacc_suscept = 0.;  // susceptibility to infection
			} else {
        		net.node[id].vacc_infect  = 1.;  // transmission rate to others 
        		net.node[id].vacc_suscept = 1.;  // susceptibility to infection
			}

			//net.node[id].vacc_infect = 
        	//net.node[i].vacc_infect  = 1.0;
			//net.node[i].vacc_suscept = 1.0;
        	//vacc1_effect 0.6   # Effective on x% [0,1] of the vaccinated
			// Add to V1 list
			addToList(&l.new_vacc1, id);
		    stateTransition(id, id, S, V1, 0., cur_time); 
		}
		if (count >= n) break;
	}
	if (count < n) {
		printf("Insufficient Susceptibles to Vaccinate\n");
	}
	//printf("nb vaccinated_1: %d\n", l.vacc1.n);
	//printf("start_search= %d\n", net.start_search);
	//printf("n= %d\n", n);
	//printf("count= %d\n", count);

	// Once n is zero, 
	//exit(1);
}
//-------------------------------------------------------------------------------
void G::secondVaccination(Params& par, Lists& l, GSL& gsl, Network &net, Counts& c, float cur_time)
{
  if (par.nb_doses == 1) return;

	for (int i=0; i < l.vacc1.n; i++) {
		float time_since = cur_time - net.node[l.vacc1.v[i]].t_V1;
		int id = l.vacc1.v[i];
		if (time_since >= par.dt_btw_vacc) {
			addToList(&l.new_vacc2, id);
		    c.count_vacc2 += 1;
	        net.node[id].t_V2 = cur_time;
	
			// Higher vaccine effectiveness decreases the transmission rate
	        net.node[id].beta_IS = par.beta[IS] * (1.-par.vacc2_eff); 

			// perhaps have a new entry in the data file for par.vacc1_recov_eff?
			// as recovery effectiveness goes to one, the recovery rate to infinity, so the 
			// recovery time goes to zero, so infection time goes to zero. 

			// Higher vaccine effectiveness increases the recovery rate
	        net.node[id].mu   = par.mu / (1.-par.vacc2_recov_eff); 
			i = removeFromList(&l.vacc1, i);
		    stateTransition(id, id, V1, V2, net.node[id].t_V1, cur_time);
		}
	}
    //printf("vacc1.n= %d, vacc2.n= %d\n", l.vacc1.n, l.vacc2.n);
    //printf("new_vacc1.n= %d, new_vacc2.n= %d\n", l.new_vacc1.n, l.new_vacc2.n);
}
//----------------------------------------------------------------------
void G::infection(Lists& l, Network& net, Params& params, GSL& gsl, Counts& c, float cur_time)
{
  if (l.infectious_asymptomatic.n > 0) {printf("infectious_asymptomatic should be == 0\n"); exit(1); }
  if (l.pre_symptomatic.n > 0) {printf("pre_symptomatic should be == 0\n"); exit(1); }

  // Loop through the infectious symptomatic (check the neighbors of all infected)
  for (int i=0; i < l.infectious_symptomatic.n; i++) {
    infect(l.infectious_symptomatic.v[i], IS, net, params, gsl, l, c, cur_time);
  }
}
//----------------------------------------------------------------------
void G::infect(int source, int type, Network& net, Params& params, GSL& gsl, Lists& l, Counts& c, float cur_time)
{
  int target;
  double prob;
  //static int count_success = 0;
  //static int count_total   = 0;

  if (net.node[source].state != IS) { // Code did not exit
	printf("I expected source state to be IS instead of %d\n", net.node[source].state); exit(1);
  }

  // Loop through neighboards of IS
  for (int j=0; j < net.node[source].k; j++) {  
      target = net.node[source].v[j]; // neighbor j of source
	  // Added if branch for tracking potential infected perform more detailed 
	  // measurements of generation time contraction
#ifdef VARBETA
	  Node& node = net.node[source];
	  // transmission distribution is not a function of the individual. So superspreading is not modeled. 
	  // Furthermore, the distribution is not exponential (that is more realistic)
#ifdef INDIV_VAR
	 double t = (files.it - node.ti_L) * par.dt;
     double betaISt = gsl_ran_weibull_pdf(t, params.beta_scale, params.beta_shape);
	 float beta = par.R0 * betaISt * node.w[j];
#else
	  float beta = par.R0 * getBetaISt(node) * node.w[j];
#endif
#else
	  float beta = net.node[source].beta_IS * net.node[source].w[j];
#endif
	  prob = params.dt * beta;

#if EXP
	    prob = 1.-exp(-prob);   // == prob as prob -> zero
#endif
	  // If the target is to be infected, there are two cases: 
	  // Either it is already infected, so I record the "potential infection"
	  if (net.node[target].state != S) {
		  // 
	      if (gsl_rng_uniform(gsl.random_gsl) < prob) {
		  	stateTransition(source, target, IS, PotL, net.node[source].t_IS, cur_time);
		  }
	  } 
	  // Or it is not yet infected 
	  else {
		// Check whether infection occurs based on betaISt or beta
		// Convert S to L  (infected, not infectious)
	    if (gsl_rng_uniform(gsl.random_gsl) < prob) {

		    addToList(&l.new_latent_symptomatic, target);
			c.count_l_symp += 1;

		  #if 0
	      if (gsl_rng_uniform(gsl.random_gsl) < params.p) { // p = 0
		    addToList(&l.new_latent_asymptomatic, target);
			c.count_l_asymp += 1;
		  } else {
		    addToList(&l.new_latent_symptomatic, target);
			c.count_l_symp += 1;
  		    count_success++;
			double ratio = (double) count_success / (double) count_total;
			printf("count_success= %d, count_total= %d\n", count_success, count_total);
			printf("ratio: success/total: %f\n", ratio);
		  }
          #endif

		  // Values of time interval distribution are incorrect it seems.
		  // Chances there is an error in the next line
	      // Update target data
	      net.node[target].state = L;
	      net.node[target].t_L   = cur_time;
		  Node& ns = net.node[source];
		  Node& nt = net.node[target];
		  stateTransition(source, target, IS, L, ns.t_IS, nt.t_L);
		  stateTransition(source, target, L, L, ns.t_L, nt.t_L);  // Generation time

	      //Various
	      l.n_active++;
	    }
	  } // check whether state is S
  } // for  
}
//----------------------------------------------------------------------
void G::latency(Params& par, Lists& l, GSL& gsl, Network &net, Counts& c, float cur_time)
{
  int id;

  // Transition from L to IS at rate epsilon_symptomatic
  // Must stay in L state at for a time of at least dt, even if prob=1
#if 1
  for (int i=0; i < l.latent_symptomatic.n; i++) {
      id = l.latent_symptomatic.v[i];  
#if EXP
	  double prob = (1.-exp(-par.dt*par.epsilon_symptomatic));
#else
	  double prob = par.dt*par.epsilon_symptomatic;
#endif
	  if (gsl_rng_uniform(gsl.random_gsl) < prob) {
	    addToList(&l.new_infectious_symptomatic, id);
		c.count_i_symp += 1;
	    net.node[id].state = IS;
	    net.node[id].t_IS = cur_time;
	    i = removeFromList(&l.latent_symptomatic, i);
		stateTransition(id, id, L, IS, net.node[id].t_L, cur_time);
	  }
  }
#endif
}
//----------------------------------------------------------------------
void G::IaToR(){}
//----------------------------------------------------------------------
void G::preToI(){}
//----------------------------------------------------------------------
void G::IsTransition(Params& par, Lists& l, Network& net, Counts& c, GSL& gsl, float cur_time)
{
  int id;

  // Modified by GE to implement simple SIR model. No hospitalizations, ICU, etc.
  // Transition from IS to R at rate mu*dt
  // Perhaps it is the infectious that should be a negative Binomial? It is the infectious time that partially 
  // dictates R0.
  // Is there a distribution of recovery times? 

  for (int i=0; i < l.infectious_symptomatic.n; i++) {
    id = l.infectious_symptomatic.v[i];  
#ifdef CONST_INFECTION_TIME
	if ((cur_time-net.node[id].t_IS) >= INFECTION_TIME) {
#else
 #if EXP
    double prob = 1. - exp(-par.dt*net.node[id].mu);
 #else
    double prob = par.dt * par.mu * net.node[id].mu;
 #endif
    if (gsl_rng_uniform(gsl.random_gsl) < prob) { //days to R/Home
#endif
      addToList(&l.new_recovered, id);
	  c.count_recov      += 1;
      net.node[id].state  = R;
	  net.node[id].t_R    = cur_time;
      l.n_active--;
      i = removeFromList(&l.infectious_symptomatic, i);

	  stateTransition(id, id, IS, R, net.node[id].t_IS, cur_time);
	}
  }
  return;
}
//----------------------------------------------------------------------
void G::homeTransition(){}
//----------------------------------------------------------------------
void G::hospitals(){}
//----------------------------------------------------------------------
void G::updateTime()
{ 
  files.t += par.dt;
  files.it++;
}
//----------------------------------------------------------------------
void G::resetVariables(Lists& l, Files& files)
{  
  l.susceptible.n = 0;
  l.latent_asymptomatic.n = 0;
  l.latent_symptomatic.n = 0;
  l.infectious_asymptomatic.n = 0;
  l.pre_symptomatic.n = 0;
  l.infectious_symptomatic.n = 0;
  l.home.n = 0;
  l.hospital.n = 0;
  l.icu.n = 0;
  l.recovered.n = 0;
  l.vacc1.n = 0;
  l.vacc2.n = 0;

  for(int i=0; i < NAGE; i++)
  {
      l.susceptible.cum[i] = 0;
      l.latent_asymptomatic.cum[i] = 0;
      l.latent_symptomatic.cum[i] = 0;
      l.infectious_asymptomatic.cum[i] = 0;
      l.pre_symptomatic.cum[i] = 0;
      l.infectious_symptomatic.cum[i] = 0;
      l.home.cum[i] = 0;
      l.hospital.cum[i] = 0;
      l.icu.cum[i] = 0;
      l.recovered.cum[i] = 0;
	  l.vacc1.cum[i] = 0;
	  l.vacc2.cum[i] = 0;
  }

  files.t = 0;
  l.n_active = 0;

  l.id_from.resize(0);
  l.id_to.resize(0);
  l.state_from.resize(0);
  l.state_to.resize(0);
  l.from_time.resize(0);
  l.to_time.resize(0);

  // Transmission profile
  // We will use a Weibull Distribution with scale 5.665 and shape 2.826, 
  // from the paper by Ferretti (2020), "Quantifying SARS-COV-2" transmission 
  // suggests epidemic control with digital contact tracing."
  par.betaISt.resize(3000);
  double shape = 2.826;
  double scale = 5.665;
  double sum = 0.0;

  // 3000 is overkill, but it allows me to avoid an if statement in getBetISt()
  for (int i=0; i < 3000; i++) {
	 double t = i * par.dt;
     par.betaISt[i] = gsl_ran_weibull_pdf(t, scale, shape);
     printf("beta: %f\n", par.betaISt[i]);
     sum += par.betaISt[i];
	 printf("beta[%f]= %f\n", i*par.dt, par.betaISt[i]);
  }
  printf("integral of betaISt= %f\n", sum*par.dt);
}
//----------------------------------------------------------------------
void G::resetNodes(Params& par, Network& net)
{
    for(int i=0; i < par.N; i++) {
        net.node[i].state = S;
		net.node[i].is_vacc = 0;
		net.node[i].beta_IS = par.beta[IS]; 
		net.node[i].mu = par.mu;
        net.node[i].vacc_infect  = 1.0;
        net.node[i].vacc_suscept = 1.0;
    }

    net.start_search = 0;
}
//----------------------------------------------------------------------
void G::resetNew(Lists& l)
{
  l.new_latent_asymptomatic.n = 0;
  l.new_latent_symptomatic.n = 0;
  l.new_infectious_asymptomatic.n = 0;
  l.new_pre_symptomatic.n = 0;
  l.new_infectious_symptomatic.n = 0;
  l.new_home.n = 0;
  l.new_hospital.n = 0;
  l.new_icu.n = 0;
  l.new_recovered.n = 0;
  l.new_vacc1.n = 0;
  l.new_vacc2.n = 0;
}
//----------------------------------------------------------------------
void G::updateLists(Lists& l, Network& n)
{
  updateList(&l.latent_asymptomatic, &l.new_latent_asymptomatic, n);//LA
  updateList(&l.latent_symptomatic, &l.new_latent_symptomatic, n);//LS
  updateList(&l.infectious_asymptomatic, &l.new_infectious_asymptomatic, n);//IA
  updateList(&l.pre_symptomatic, &l.new_pre_symptomatic, n);//PS
  updateList(&l.infectious_symptomatic, &l.new_infectious_symptomatic, n);//IS
  updateList(&l.home, &l.new_home, n); //Home
  updateList(&l.hospital, &l.new_hospital, n);//H
  updateList(&l.icu, &l.new_icu, n);//ICU
  updateList(&l.recovered, &l.new_recovered, n);//R
  updateList(&l.vacc1, &l.new_vacc1, n);//VACC1
  updateList(&l.vacc2, &l.new_vacc2, n);//VACC2
}

//----------------------------------------------------------------------
void G::printResults(int run, Lists& l, Files& f)
{
  //Cumulative values
  for(int i=0;i<NAGE;i++)
    fprintf(f.f_cum,"%d %d %d %d %d %d %d %d %d %d %d %d\n",
       l.latent_asymptomatic.cum[i],
       l.latent_symptomatic.cum[i],
       l.infectious_asymptomatic.cum[i],
       l.pre_symptomatic.cum[i],
       l.infectious_symptomatic.cum[i],
       l.home.cum[i],
       l.hospital.cum[i],
       l.icu.cum[i],
       l.recovered.cum[i],
       l.vacc1.cum[i],
       l.vacc2.cum[i],
       run);
}
//----------------------------------------------------------------------
//G::read
void G::readData(Params& params, Lists& lists, Network& network, Files& files)
{
  // Change params.N according to node file
  printf("readNetwork\n");
  readNetwork(params, lists, network, files);
  printf("readNodes\n");
  readNodes(params, files, network);

  // I am NO LONGER READING VACCINATIONS from a file since I manage them 
  // within the program. This was a debugging tool. The call requires
  // access to the file "vaccines.csv", stored within the argument "files"
  //printf("readVaccinations\n");
  // Add parameter to parameter file. Run vaccinations, 0/1
  //readVaccinations(params, files, network, lists);
}
//----------------------------------------------------------------------
void G::readParameters(char* parameter_file, Params& params)
{
  FILE *f;
  char trash[100];

  int parameters = 0; // fake entry
  sprintf(trash, parameter_file, parameters);
  printf("parameter_file= %s\n", parameter_file);
  f = fopen(trash, "r");

  params.N 					  = readInt(f);
  params.r 					  = readFloat(f);
  params.epsilon_asymptomatic = readFloat(f);
  params.epsilon_symptomatic  = readFloat(f); //symptomatic latent period-1
  params.p                    = readFloat(f); //proportion of asymptomatic
  params.gammita              = readFloat(f);
  params.mu                   = readFloat(f);

  for(int i=0; i < NAGE; i++){ //symptomatic case hospitalization ratio
    params.alpha[i] = readFloat(f) ;
  }
  for(int i=0; i < NAGE; i++) {
	params.xi[i]    = readFloat(f);
  }

  params.delta        = readFloat(f);
  params.muH          = readFloat(f);
  params.muICU        = readFloat(f);
  params.k            = readFloat(f);
  params.beta_normal  = readFloat(f);
  params.dt           = readFloat(f);
  params.vacc1_rate   = readFloat(f);
  params.vacc2_rate   = readFloat(f);
  params.vacc1_eff = readFloat(f);
  params.vacc2_eff = readFloat(f);

  // By default, same as vaccine effectiveness
  // The vaccine will shorten the time to recovery
  // Limit to 0.99 since one has to divide by recovery rate by (1-vacc1_recov_eff)
  params.vacc1_recov_eff = params.vacc1_eff < 0.99 ? 
  		params.vacc1_eff : 0.99;
  params.vacc2_recov_eff = params.vacc2_eff < 0.99 ? 
  		params.vacc1_eff : 0.99;

  params.vacc2_rate   = readFloat(f);
  params.dt_btw_vacc  = readFloat(f);
  printf("read dt_btw_vacc: %f\n", params.dt_btw_vacc);

  params.epsilon_asymptomatic = 1.0/params.epsilon_asymptomatic;
  params.epsilon_symptomatic  = 1.0/params.epsilon_symptomatic;
  params.delta   = 1.0/params.delta;
  params.muH     = 1.0/params.muH;
  params.muICU   = 1.0/params.muICU;
  params.gammita = 1.0/params.gammita;
  params.mu      = 1.0/params.mu;  // mu is now a rate

  for(int i=0; i < NAGE; i++) {
    params.alpha[i] = params.alpha[i]/100;
    params.xi[i]    = params.xi[i]/100;
  }

  fclose(f);
}
//----------------------------------------------------------------------
void G::readNetwork(Params& params, Lists& lists, Network& network, Files& files)
{
  int s, t;
  double w;
  FILE *f;

  FILE* fd = fopen(files.node_file, "r");
  fscanf(fd, "%d", &params.N);
  printf("params.N= %d\n", params.N);

  network.node = (Node*) malloc(params.N * sizeof * network.node);

  for(int i=0;i<params.N;i++)
    {
      network.node[i].k = 0;
      network.node[i].v = (int*) malloc(sizeof * network.node[i].v);
      network.node[i].w = (float*) malloc(sizeof * network.node[i].w);
      network.node[i].t_L  = 0.;
      network.node[i].t_IS = 0.;
      network.node[i].t_R  = 0.;
      network.node[i].ti_L = 0;
      network.node[i].t_V1  = -1.;  // uninitialized if negative
      network.node[i].t_V2  = -1.;
    }

  printf("files.network_file= %s\n", files.network_file);
  printf("files.node_file= %s\n", files.node_file);

  f = fopen(files.network_file, "r");
  int nb_edges;
  fscanf(f, "%d", &nb_edges);
  printf("nb_edges= %d\n", nb_edges);

  for (int i=0; i < nb_edges; i++) {
	  fscanf(f, "%d%d%lf", &s, &t, &w);
	  //printf("s,t,w= %d, %d, %f\n", s, t, w);
      network.node[s].k++;
      //Update size of vectors
      network.node[s].v = (int*) realloc(network.node[s].v, network.node[s].k * sizeof *network.node[s].v);
      network.node[s].w = (float*) realloc(network.node[s].w, network.node[s].k * sizeof *network.node[s].w);
      //Write data
      network.node[s].v[network.node[s].k-1] = t;
      network.node[s].w[network.node[s].k-1] = w;

	  // The input data was an undirected graph
	  // This code requires directed edges
	  if (t != s) {
		network.node[t].k++;
      	network.node[t].v = (int*) realloc(network.node[t].v, network.node[t].k * sizeof *network.node[t].v);
        network.node[t].w = (float*) realloc(network.node[t].w, network.node[t].k * sizeof *network.node[t].w);
        network.node[t].v[network.node[t].k-1] = s;
        network.node[t].w[network.node[t].k-1] = w;
	  }
    }
  fclose(f);
}

//----------------------------------------------------------------------
void G::readNodes(Params& params, Files& files, Network& network)
{
  // Ideally, the graph nodes should be randomized before reading them. 
  // Randomization is difficult after read-in
  int age;
  FILE *f;
  int nb_nodes;

  f = fopen(files.node_file, "r");
  fscanf(f, "%d", &nb_nodes);   // WHY is \n required? (only eats up spaces)

  if (params.N != nb_nodes) {
      printf("nb_nodes not equal to params.N. Fix error\n");
	  exit(1);
  }

  int N = 0;
  int node;

  for (int i=0; i < nb_nodes; i++) {
    fscanf(f, "%d%d", &node, &age);
	network.node[node].age = age;
	N++;
  }

  printf("params.N= %d, N= %d (should be identical)\n", params.N, N);
  params.N = N;
  printf("params.N= %d, N= %d (should be identical)\n", params.N, N);

  fclose(f);
}
//----------------------------------------------------------------------
void G::readVaccinations(Params& params, Files& files, Network& network, Lists& l)
{
  FILE *f;
  int nb_nodes;

  f = fopen(files.vaccination_file, "r");
  fscanf(f, "%d", &nb_nodes);

  if (params.N != nb_nodes) {
      printf("nb_nodes (%d) not equal to params.N (%d). Fix error\n", nb_nodes, params.N);
	  exit(1);
  }
 
  int node;
  int* vaccinated = new int [nb_nodes];
  float efficacy;

  for (int i=0; i < nb_nodes; i++) {
	  fscanf(f, "%d%d%f", &node, &vaccinated[i], &efficacy);
	  if (vaccinated[i] == 1) {
		l.people_vaccinated.push_back(i);
	  }
	  //printf("node, vaccinated, efficiency: %d, %d, %f\n", node, vaccinated, efficacy);
  }

  delete [] vaccinated;

  // Set the vaccinated people to recovered

  fclose(f);
}
//----------------------------------------------------------------------
//utilities
void G::allocateMemory(Params& p, Lists& l)
{
  //Spreading
  // Memory: 4.4Mbytes for these lists for Leon County, including vaccines
  printf("memory: %lu bytes\n", p.N * sizeof(*l.latent_asymptomatic.v) * 11 * 2);
  printf("sizeof(int*)= %ld\n", sizeof(int*));
  l.susceptible.v = (int*) malloc(p.N * sizeof * l.susceptible.v);
  l.latent_asymptomatic.v = (int*) malloc(p.N * sizeof * l.latent_asymptomatic.v);
  l.latent_symptomatic.v = (int*) malloc(p.N * sizeof * l.latent_symptomatic.v);
  l.infectious_asymptomatic.v = (int*) malloc(p.N * sizeof * l.infectious_asymptomatic.v);
  l.pre_symptomatic.v = (int*) malloc(p.N * sizeof * l.pre_symptomatic.v);
  l.infectious_symptomatic.v = (int*) malloc(p.N * sizeof * l.infectious_symptomatic.v);
  l.home.v = (int*) malloc(p.N * sizeof * l.home.v);
  l.hospital.v = (int*) malloc(p.N * sizeof * l.hospital.v);
  l.icu.v = (int*) malloc(p.N * sizeof * l.icu.v);
  l.recovered.v = (int*) malloc(p.N * sizeof * l.recovered.v);
  l.vacc1.v = (int*) malloc(p.N * sizeof * l.vacc1.v);
  l.vacc2.v = (int*) malloc(p.N * sizeof * l.vacc2.v);
  
  //New spreading
  l.new_latent_asymptomatic.v = (int*) malloc(p.N * sizeof * l.new_latent_asymptomatic.v);
  l.new_latent_symptomatic.v = (int*) malloc(p.N * sizeof * l.new_latent_symptomatic.v);
  l.new_infectious_asymptomatic.v = (int*) malloc(p.N * sizeof * l.new_infectious_asymptomatic.v);
  l.new_pre_symptomatic.v = (int*) malloc(p.N * sizeof * l.new_pre_symptomatic.v);
  l.new_infectious_symptomatic.v = (int*) malloc(p.N * sizeof * l.new_infectious_symptomatic.v);
  l.new_home.v = (int*) malloc(p.N * sizeof * l.new_home.v);
  l.new_hospital.v = (int*) malloc(p.N * sizeof * l.new_hospital.v);
  l.new_icu.v = (int*) malloc(p.N * sizeof * l.new_icu.v);
  l.new_recovered.v = (int*) malloc(p.N * sizeof * l.new_recovered.v);
  l.new_vacc1.v = (int*) malloc(p.N * sizeof * l.new_vacc1.v);
  l.new_vacc2.v = (int*) malloc(p.N * sizeof * l.new_vacc2.v);
}
//----------------------------------------------------------------------
void G::initRandom(int seed, GSL& gsl)
{
  //GSL gsl;
  gsl_rng_env_setup();
  gsl.T = gsl_rng_default;
  gsl.random_gsl = gsl_rng_alloc(gsl.T);
  gsl_rng_set(gsl.random_gsl,seed);
}

//----------------------------------------------------------------------
//void G::openFiles(char* cum_baseline, char* data_baseline)
void G::openFiles(Files& files)
{
  //char name_cum[100], name_data[100];

  int parameters = 0;  // Do not know where this comes from
  sprintf(files.name_cum, "%s/cum_baseline_p%d.txt", files.result_folder, parameters);
  sprintf(files.name_data, "%s/data_baseline_p%d.txt",files.result_folder, parameters);

  //sprintf(name_cum,"Results/cum_baseline_p%d.txt",parameters);
  //sprintf(name_data,"Results/data_baseline_p%d.txt",parameters);

  files.f_cum = fopen(files.name_cum,"w");
  files.f_data = fopen(files.name_data,"w");
}
//----------------------------------------------------------------------
void G::setBeta(Params& p)
{
  double beta_pre;

  beta_pre = p.beta_normal * p.gammita*p.k/(p.mu*(1-p.k));
    
  for(int i=0; i < NCOMPARTMENTS; i++)
    p.beta[i] = 0;

  //printf("r= %f\n", p.r);
  //printf("beta_normal= %f\n", p.beta_normal);
  p.beta[IA] = p.r * p.beta_normal;
  p.beta[IS] = p.beta_normal;
  p.beta[PS] = beta_pre;
  p.beta[PS] = 0.0;  // I want infected under these conditions
  //printf("beta[IA] = %f\n", p.beta[IA]);
  //printf("beta[IS] = %f\n", p.beta[IS]);
  //printf("beta[PS] = %f\n", p.beta[PS]);
}

//----------------------------------------------------------------------
void G::addToList(List *list, int id)
{
  list->v[list->n] = id;
  list->n++;
}
//----------------------------------------------------------------------
int G::removeFromList(List *list, int i)
{
  list->n--;
  list->v[i] = list->v[list->n];

  return i-1;
}

//----------------------------------------------------------------------
void G::updateList(List* list, List *newl, Network& network)
{
  for(int i=0;i<newl->n;i++)
    {
      list->v[list->n] = newl->v[i];
      list->n++;
      list->cum[network.node[newl->v[i]].age]++;
    }
}
//----------------------------------------------------------------------
void G::closeFiles(Files& files)
{
  fclose(files.f_cum);
  fclose(files.f_data);
}
//----------------------------------------------------------------------
void G::print(int t0)
{
  printf("Execution time %d seconds\n",(int)time(NULL)-t0);
}
//----------------------------------------------------------------------
void G::freeMemory(Params& p, Network& network, Lists& l, GSL& gsl)
{
  for(int i=0;i< p.N;i++)
    {
      free(network.node[i].v);
      free(network.node[i].w);
    }
  free(network.node);

  free(l.susceptible.v);
  free(l.latent_asymptomatic.v);
  free(l.latent_symptomatic.v);
  free(l.infectious_asymptomatic.v);
  free(l.pre_symptomatic.v);
  free(l.infectious_symptomatic.v);
  free(l.home.v);
  free(l.hospital.v);
  free(l.icu.v);
  free(l.recovered.v);
  free(l.vacc1.v);
  free(l.vacc2.v);

  free(l.new_latent_asymptomatic.v);
  free(l.new_latent_symptomatic.v);
  free(l.new_infectious_asymptomatic.v);
  free(l.new_pre_symptomatic.v);
  free(l.new_infectious_symptomatic.v);
  free(l.new_home.v);
  free(l.new_hospital.v);
  free(l.new_icu.v);
  free(l.new_recovered.v);
  free(l.new_vacc1.v);
  free(l.new_vacc2.v);
         
  gsl_rng_free(gsl.random_gsl);
}
//----------------------------------------------------------------------
void G::printTransitionStats()
{
	Lists& l = lists;

	int lg = l.id_from.size();
	FILE* fd = fopen("transition_stats.csv", "w");
	fprintf(fd, "from_id,to_id,from_state,to_state,from_time,to_time\n");

	for (int i=0; i < lg; i++) {
		fprintf(fd, "%d, %d, %d, %d, %f, %f\n", l.id_from[i], l.id_to[i], l.state_from[i], l.state_to[i], l.from_time[i], l.to_time[i]);
	}
	fclose(fd);
}
//----------------------------------------------------------------------
void G::stateTransition(int source, int target, int from_state, int to_state, float from_time, float to_time)
{
	Lists& l = lists; 
	l.id_from.push_back(source);
	l.id_to.push_back(target);
	l.state_from.push_back(from_state);
	l.state_to.push_back(to_state);
	l.from_time.push_back(from_time);
	l.to_time.push_back(to_time);
}
//----------------------------------------------------------------------
float G::readFloat(FILE* fd)
{
	char junk[500], junk1[500];
	float f;
	junk[0] = '\0'; 
	junk1[0] = '\0';
	fscanf(fd, "%s%f%[^\n]s", junk, &f, junk1);
	printf("==> readFloat: %s, %f, %s\n", junk, f, junk1);
	return f;
}
//----------------------------------------------------------------------
float G::readInt(FILE* fd)
{
	char junk[500], junk1[500];
	int i;
	junk[0] = '\0'; 
	junk1[0] = '\0';
	fscanf(fd, "%s%d%[^\n]s", junk, &i, junk1);
	printf("%s, %d, %s\n", junk, i, junk1);
	printf("==> readFloat: %s, %d, %s\n", junk, i, junk1);
	return i;
}
//----------------------------------------------------------------------
void G::parse(int argc, char** argv, Params& par)
{
	try {
        cxxopts::Options options(argv[0], " - example command line options");
        options
          .positional_help("[optional args]")
          .show_positional_help();
    
        options
          //.allow_unrecognised_options()
          .add_options()
		  ("N", "Number of nodes", cxxopts::value<int>())
		  ("gammainv", "Has to do with presymtomatic beta [days]", cxxopts::value<float>())
		  ("muinv", " Average recovery time [days]", cxxopts::value<float>())
		  ("betaIS", " Transmissibility rate", cxxopts::value<float>())
		  ("dt", " Time step", cxxopts::value<float>())
		  ("vacc1_rate", " Rate of 1st vaccine dose", cxxopts::value<float>())
		  ("vacc2_rate", " Rate of 2nd vaccine dose", cxxopts::value<float>())
		  ("vacc1_eff", " First vaccine dose efficacy [0-1]", cxxopts::value<float>())
		  ("vacc2_eff", " Second vaccine dose efficacy [0-1]", cxxopts::value<float>())
		  ("dt_btw_vacc", " Time between 1st and 2nd vaccine doses", cxxopts::value<float>())
		  ("max_nb_avail_doses", " Maximum number of vaccine doses", cxxopts::value<int>()->default_value("-1"))
		  ("nb_doses", " Number of vaccine doses (1/2)", cxxopts::value<int>()->default_value("2"))
		  ("epsilonSinv", "Average latent interval [days]", cxxopts::value<float>())
		  ("R0", "Initial R0 in the absence of behavior change", cxxopts::value<float>())
		  ("beta_shape", "Shape parameter for infectivity profile", cxxopts::value<float>()->default_value("2.826"))
		  ("beta_scale", "Scale parameter for infectivity profile", cxxopts::value<float>()->default_value("5.665"))
          ("help", "Print help")
        ;

    	auto res = options.parse(argc, argv);
  
		if (res.count("help")) {
      		std::cout << options.help({"", "Group"}) << std::endl;
      		exit(0);
    	}
		if (res.count("N") == 1) {
			par.N = res["N"].as<int>();
		    printf("arg: par.N= %d\n", par.N);
	    }
		if (res.count("gammitainv") == 1) {
			par.gammita = 1. / res["gammita"].as<float>();
		    printf("arg: par.gammita= %f [1/days]\n", par.gammita);
	    }
		if (res.count("mu") == 1) {
			par.mu = 1. / res["muinv"].as<float>(); // rate
		    printf("arg: par.mu= %f [1/days]\n", par.mu);
	    }
		if (res.count("betaIS") == 1) {
			par.beta_normal = res["betaIS"].as<float>();
		    printf("arg: par.beta_normal= %f\n", par.beta_normal);
	    }
		if (res.count("dt") == 1) {
			par.dt = res["dt"].as<float>();
		    printf("arg: par.dt= %f\n", par.dt);
	    }
		if (res.count("vacc1_rate") == 1) {
			par.vacc1_rate = res["vacc1_rate"].as<float>();
		    printf("arg: par.vacc1_rate= %f\n", par.vacc1_rate);
	    }
		if (res.count("vacc2_rate") == 1) {
			par.vacc2_rate = res["vacc2_rate"].as<float>();
		    printf("arg: par.vacc2_rate= %f\n", par.vacc2_rate);
	    }
		if (res.count("vacc1_eff") == 1) {
			par.vacc1_eff = res["vacc1_eff"].as<float>();
		    printf("arg: par.vacc1_eff= %f\n", par.vacc1_eff);
	    }
		if (res.count("vacc2_eff") == 1) {
			par.vacc2_eff = res["vacc2_eff"].as<float>();
		    printf("arg: par.vacc2_eff= %f\n", par.vacc2_eff);
	    }
		if (res.count("dt_btw_vacc") == 1) {
			par.dt_btw_vacc = res["dt_btw_vacc"].as<float>();
		    printf("arg: par.dt_btw_vacc= %f\n", par.dt_btw_vacc);
	    }
		if (res.count("max_nb_avail_doses") == 1) {
			par.max_nb_avail_doses = res["max_nb_avail_doses"].as<int>();
			// Multiply by two since there are a max of 2 doses per individual
			if (par.max_nb_avail_doses == -1) par.max_nb_avail_doses = 2*par.N;
		    printf("arg: par.max_nb_avail_doses= %d\n", par.max_nb_avail_doses);
	    }
		if (res.count("nb_doses") == 1) {
			par.nb_doses = res["nb_doses"].as<int>();
		    printf("arg: par.nb_doses= %d\n", par.nb_doses);
	    }
		if (res.count("R0") == 1) {
			par.R0 = res["R0"].as<float>();
		    printf("arg: par.nb_doses= %f\n", par.R0);
	    }
		if (res.count("epsilonSinv") == 1) {
			par.epsilon_symptomatic = 1. / res["epsilonSinv"].as<float>();
		    printf("arg: epsilonS-1= %f days\n", 1. / par.epsilon_symptomatic);
		    printf("arg: par.epsilon_symptomatic= %f [1/days]\n", par.epsilon_symptomatic);
	    }
		if (res.count("beta_shape") == 1) {
			par.beta_shape = res["beta_shape"].as<float>();
		    printf("arg: beta_shape= %f\n", par.beta_shape);
		}
		if (res.count("beta_scale") == 1) {
			par.beta_scale = res["beta_scale"].as<float>();
		    printf("arg: beta_scale= %f\n", par.beta_scale);
	    }

    //auto arguments = res.arguments();
    } catch(const cxxopts::OptionException& e) {
    		printf("error parse on options\n");
    		std::cout << "error parsing options: " << e.what() << std::endl;
    		exit(1);
	}
	
	return;
}
//----------------------------------------------------------------------
double G::getBetaISt(Node& node)
// return \beta(\tau, t) = \beta(tau)
{
	// No bounds checking. There is enough space reserved in par.betaISt.
	int dti = files.it - node.ti_L;
	return par.betaISt[dti];
}
//----------------------------------------------------------------------
void G::weibull(vector<double>& samples, GSL& gsl, double shape, double scale, int n)
{
	for (int i=0; i < n; i++) {
    	samples[i] = gsl_ran_weibull(gsl.r_rng, scale, shape);
	}
}
//----------------------------------------------------------------------
void G::lognormal(vector<double>& samples, GSL& gsl, double mu, double sigma2, int n)
// See Wikipedia article
{
	for (int i=0; i < n; i++) {
    	samples[i] = gsl_ran_lognormal(gsl.r_rng, mu, sigma2);
	}
}
//----------------------------------------------------------------------
