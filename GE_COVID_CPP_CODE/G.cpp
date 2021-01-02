#include "stdlib.h"
#include "G.h"
#include "head.h"

#define EXP 1       // Exponential distribution of infection times
#define CONST_INFECTION_TIME 0   // constant recovery time
#define INFECTION_TIME 3.

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

  if (argc != 3) {
	printf("To run the code, \n");
	printf("  ./executable DataFolder ResultFolder\n");
	printf("-------------------------------------\n");
	exit(1);
  }

  strcpy(files.data_folder, argv[1]);
  strcat(files.data_folder, "/");
  strcpy(files.result_folder, argv[2]);
  strcat(files.result_folder, "/");

  strcpy(files.parameter_file, files.result_folder);
  strcat(files.parameter_file, "parameters_0.txt");
  //printf("parameter_file= %s\n", files.parameter_file); 

  strcpy(files.node_file, files.data_folder);
  strcat(files.node_file, "nodes.txt");
  printf("**** node_file: %s\n", files.node_file);

  strcpy(files.network_file, files.data_folder);
  strcat(files.network_file, "network.txt");
  printf("**** network_file: %s\n", files.network_file);

  strcpy(files.vaccination_file, files.data_folder);
  strcat(files.vaccination_file, "vaccines.csv");

  readParameters(files.parameter_file, params);
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

        init(params, c, n, gsl, lists, f);

        while(lists.n_active>0) {
	      spread(run, f, lists, n, params, gsl, c);
		}

  		printf("total number latent symp:  %d\n", c.count_l_symp);
  		printf("total number latent presymp:  %d\n", c.count_l_presymp);
  		printf("total number infectious symp:  %d\n", c.count_i_symp);
  		printf("total number recovered:  %d\n", c.count_recov);
        results(run, lists, f);

		printTransitionStats();
     }
  }
}
//----------------------------------------------------------------------
//spread
void G::count_states(Params& params, Counts& c, Network& n)
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
  printf("Counts: S,L,IS,R: %d, %d, %d, %d\n", c.countS, c.countL, c.countIS, c.countR);
}
//----------------------------------------------------------------------
void G::init(Params& p, Counts& c, Network& n, GSL& gsl, Lists& l, Files& f)
{
  resetVariables(l, f);
  resetNodes(p, n);
  printf("after resetN\n");
  
  //Start
  seedInfection(p, c, n, gsl, l, f);
  printf("after seed\n");
}
//----------------------------------------------------------------------
void G::seedInfection(Params& par, Counts& c, Network& n, GSL& gsl, Lists& l, Files& f)
{
  int seed;
  count_states(par, c, n);
  printf("inside seedInf\n");
  int nb_vaccinated = l.people_vaccinated.size();
  //for (int i=0; i < 100; i++) {
     //printf("person %d is vaccinated\n");
  //}

  // set Recovered to those vaccinated
  for (int i=0; i < nb_vaccinated; i++) {
	    int person_vaccinated = l.people_vaccinated[i];
	  	n.node[person_vaccinated].state = R;
  }

  // Use a permutation to make sure that there are no duplicates when 
  // choosing more than one initial infected
  gsl_permutation* p = gsl_permutation_alloc(par.N);
  gsl_rng_env_setup();
  gsl.T = gsl_rng_default;
  gsl.r_rng = gsl_rng_alloc(gsl.T); // *****
  gsl_permutation_init(p);
  gsl_ran_shuffle(gsl.r_rng, p->data, par.N, sizeof(size_t));

  float rho = 0.001; // infectivity percentage at t=0 (SHOULD BE IN PARAMS)
  int ninfected = rho * par.N;
  //ninfected = 1;
  printf("N= %d, ninfected= %d\n", par.N, ninfected);

  for (int i=0; i < ninfected; i++) { 
  	seed = p->data[i];
  	n.node[seed].state = L;
    addToList(&l.latent_symptomatic, seed); // orig 
  }

  // Count number in recovered state
  int count = 0;
  for (int i=0; i < par.N; i++) {
      if (n.node[i].state == R) { count++; }
  }
  printf("nb recovered: %d\n", count);

  gsl_permutation_free(p);

  l.n_active = ninfected;
  c.count_l_symp += ninfected;
  printf("added %d latent_symptomatic individuals\n", ninfected);

  f.t = par.dt;
}
//----------------------------------------------------------------------
//void G::spread(int){}
void G::spread(int run, Files& f, Lists& l, Network& net, Params& params, GSL& gsl, Counts& c)
{  
  resetNew(l);
  double cur_time = f.t;

  // S to L
  //printf("before infection()\n"); count_states(params, c, net);
  infection(l, net, params, gsl, c, cur_time);
  // L to P
  //printf("before latency()\n"); count_states(params, c, net);
  latency(params, l, gsl, net, c, cur_time);
  // P to I
  //preToI();
  // I to R
  //printf("before IsTransition()\n"); count_states(params, c, net);
  IsTransition(params, l, net, c, gsl, cur_time);


  updateLists(l, net);
  //printf("----------------------------------------\n");
  updateTime();

  //Write data
  fprintf(f.f_data,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %f\n",
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
	run,
    f.t
	);
}
//----------------------------------------------------------------------
void G::infection(Lists& l, Network& net, Params& params, GSL& gsl, Counts& c, double cur_time)
{
  if (l.infectious_asymptomatic.n > 0) {printf("infectious_asymptomatic should be == 0\n"); exit(1); }
  if (l.pre_symptomatic.n > 0) {printf("pre_symptomatic should be == 0\n"); exit(1); }

  //Infectious symptomatic (check the neighbors of all infected)
  for (int i=0; i < l.infectious_symptomatic.n; i++) {
	//printf("call infect(infectious_symptomatic) %d\n", i);
    infect(l.infectious_symptomatic.v[i], IS, net, params, gsl, l, c, cur_time);
  }
}
//----------------------------------------------------------------------
void G::infect(int source, int type, Network& net, Params& params, GSL& gsl, Lists& l, Counts& c, double cur_time)
{
  int target;
  double prob;
  static int count_success = 0;
  //static int count_total   = 0;

  if (net.node[source].state != IS) { // Code did not exit
	printf("I expected source state to be IS\n"); exit(1);
  }
 
  //printf("infect,source= %d,  ...k= %d\n", source, net.node[source].k);  //all zero
  for (int j=0; j < net.node[source].k; j++) {
      target = net.node[source].v[j];
	  // Added if branch for tracking potential infected perform more detailed 
	  // measurements of generation time contraction
#if EXP
	    prob = 1.-exp(-params.dt * params.beta[type] * net.node[source].w[j]);
#else
	    prob = params.dt * params.beta[type] * net.node[source].w[j];
#endif
	  if (net.node[target].state != S) {
	      if (gsl_rng_uniform(gsl.random_gsl) < prob) {
		  	stateTransition(source, target, IS, PotL, net.node[source].t_IS, cur_time);
		  }
	  } else {
#if 0
		if (net.node[source].state != IS) {
			printf("I expected source to be IS\n"); exit(1);
			// Code did not exit
		}
		float a = 1. / params.beta_normal;
	    float b = 1. / 0.37;
		float g = gsl_ran_gamma(gsl.r_rng, a, b);  // not yet used
#endif
		//printf("infect: prob= %f\n", prob);
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
		  stateTransition(source, target, IS, L, net.node[source].t_IS, net.node[target].t_L);

	      //Various
	      l.n_active++;
	    }
	  } // check whether state is S
  } // for  
}
//----------------------------------------------------------------------
void G::latency(Params& par, Lists& l, GSL& gsl, Network &net, Counts& c, double cur_time)
{
  int id;

  // prob goes to 1 as eps_S -> 0
#if 1
  for (int i=0; i < l.latent_symptomatic.n; i++) {
      id = l.latent_symptomatic.v[i];  
#if EXP
	  double prob = (1.-exp(-par.dt*par.epsilon_symptomatic));
#else
	  double prob = par.dt*par.epsilon_symptomatic;
#endif
	  //printf("prob L->IS (epsilon_symptomatic): %f, inv: %f\n", prob/par.dt, par.dt/prob);
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
void G::IsTransition(Params& par, Lists& l, Network& net, Counts& c, GSL& gsl, double cur_time)
{
  int id;

  // Modified by GE to implement simple SIR model. No hospitalizations, ICU, etc.
  // Go from IS to R
  for (int i=0; i < l.infectious_symptomatic.n; i++) {
    id = l.infectious_symptomatic.v[i];  
#if CONST_INFECTION_TIME
	if ((cur_time-net.node[id].t_IS) >= INFECTION_TIME) {
#else
 #if EXP
    double prob = 1. - exp(-par.dt*par.mu);
 #else
    double prob = par.dt * par.mu;
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
  //printf("t = %f\n", files.t);
  //f.t++;
  //printf("     Update Time, t= %d\n", t);
}
//----------------------------------------------------------------------
void G::resetVariables(Lists& l, Files& files)
{  
  l.latent_asymptomatic.n = 0;
  l.latent_symptomatic.n = 0;
  l.infectious_asymptomatic.n = 0;
  l.pre_symptomatic.n = 0;
  l.infectious_symptomatic.n = 0;
  l.home.n = 0;
  l.hospital.n = 0;
  l.icu.n = 0;
  l.recovered.n = 0;

  for(int i=0;i<NAGE;i++)
    {
      l.latent_asymptomatic.cum[i] = 0;
      l.latent_symptomatic.cum[i] = 0;
      l.infectious_asymptomatic.cum[i] = 0;
      l.pre_symptomatic.cum[i] = 0;
      l.infectious_symptomatic.cum[i] = 0;
      l.home.cum[i] = 0;
      l.hospital.cum[i] = 0;
      l.icu.cum[i] = 0;
      l.recovered.cum[i] = 0;
    }

  files.t = 0;
  l.n_active = 0;

  l.id_from.resize(0);
  l.id_to.resize(0);
  l.state_from.resize(0);
  l.state_to.resize(0);
  l.from_time.resize(0);
  l.to_time.resize(0);
}
//----------------------------------------------------------------------
void G::resetNodes(Params& par, Network& net)
{
  for(int i=0; i < par.N; i++)
    net.node[i].state = S;
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
}

//----------------------------------------------------------------------
void G::results(int run, Lists& l, Files& f)
{
  //Cumulative values
  for(int i=0;i<NAGE;i++)
    fprintf(f.f_cum,"%d %d %d %d %d %d %d %d %d %d \n",
       l.latent_asymptomatic.cum[i],
       l.latent_symptomatic.cum[i],
       l.infectious_asymptomatic.cum[i],
       l.pre_symptomatic.cum[i],
       l.infectious_symptomatic.cum[i],
       l.home.cum[i],
       l.hospital.cum[i],
       l.icu.cum[i],
       l.recovered.cum[i],
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
  printf("readVaccinations\n");
  readVaccinations(params, files, network, lists);
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
  fscanf(f,"%s %d", trash, &params.N); //Nodes
  fscanf(f,"%s %lf", trash, &params.r); //relative inf. of asymptomatic individuals
  fscanf(f,"%s %lf", trash, &params.epsilon_asymptomatic); //asymptomatic latent period-1
  params.epsilon_asymptomatic = 1.0/params.epsilon_asymptomatic;
  fscanf(f,"%s %lf", trash, &params.epsilon_symptomatic); //symptomatic latent period-1
  params.epsilon_symptomatic = 1.0/params.epsilon_symptomatic;
  fscanf(f,"%s %lf", trash, &params.p); //proportion of asymptomatic
  fscanf(f,"%s %lf", trash, &params.gammita); //pre-symptomatic period
  params.gammita = 1.0/params.gammita;
  fscanf(f,"%s %lf", trash, &params.mu); //time to recover
  params.mu = 1.0/params.mu;
  for(int i=0;i<NAGE;i++){ //symptomatic case hospitalization ratio
    fscanf(f,"%s %lf", trash, params.alpha+i);
    params.alpha[i] = params.alpha[i]/100;
  }
  for(int i=0;i<NAGE;i++){
    fscanf(f,"%s %lf", trash, params.xi+i); //ICU ratio
    params.xi[i] = params.xi[i]/100;
  }
  fscanf(f,"%s %lf", trash, &params.delta); //time to hospital
  params.delta = 1.0/params.delta;
  fscanf(f,"%s %lf", trash, &params.muH); //recovery in hospital
  params.muH = 1.0/params.muH;
  fscanf(f,"%s %lf", trash, &params.muICU); //recovery in ICU
  params.muICU = 1.0/params.muICU;
  fscanf(f,"%s %lf", trash, &params.k); //k
  fscanf(f,"%s %lf", trash, &params.beta_normal); //infectivity
  fscanf(f,"%s %lf", trash, &params.dt); //discrete time step
  printf("fscanf, beta_normal= %lf\n", params.beta_normal);
  printf("fscanf, r= %lf\n", params.r);

  fscanf(f,"%s %lf", trash, &params.vacc1_rate); // nb first doses per day (Poisson)
  fscanf(f,"%s %lf", trash, &params.vacc2_rate); // nb second doses per day (Poisson)
  fscanf(f,"%s %lf", trash, &params.dt_btw_vacc); // constant time between the two vaccine doses
  params.vacc1_rate = 1. / params.vacc1_rate;
  params.vacc2_rate = 1. / params.vacc2_rate;
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
      network.node[i].w = (double*) malloc(sizeof * network.node[i].w);
      network.node[i].t_L  = 0.;
      network.node[i].t_IS = 0.;
      network.node[i].t_R  = 0.;
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
      network.node[s].w = (double*) realloc(network.node[s].w, network.node[s].k * sizeof *network.node[s].w);
      //Write data
      network.node[s].v[network.node[s].k-1] = t;
      network.node[s].w[network.node[s].k-1] = w;

	  // The input data was an undirected graph
	  if (t != s) {
		network.node[t].k++;
      	network.node[t].v = (int*) realloc(network.node[t].v, network.node[t].k * sizeof *network.node[t].v);
        network.node[t].w = (double*) realloc(network.node[t].w, network.node[t].k * sizeof *network.node[t].w);
        network.node[t].v[network.node[t].k-1] = s;
        network.node[t].w[network.node[t].k-1] = w;
	  }
    }
  fclose(f);
}

//----------------------------------------------------------------------
void G::readNodes(Params& params, Files& files, Network& network)
{
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
		printf("readVaccinations: vaccinate person %d\n", i);
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
  printf("sizeof(int*)= %d\n", sizeof(int*));
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
  printf("beta[IS] = %f\n", p.beta[IS]);
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
void G::stateTransition(int source, int target, int from_state, int to_state, double from_time, double to_time)
{
	Lists& l = lists; 
	l.id_from.push_back(source);
	l.id_to.push_back(target);
	l.state_from.push_back(from_state);
	l.state_to.push_back(to_state);
	l.from_time.push_back(from_time);
	l.to_time.push_back(to_time);
}
