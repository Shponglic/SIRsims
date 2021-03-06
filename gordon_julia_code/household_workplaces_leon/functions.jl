function age2AgeBin()
    age_bins = [(5*i,5*i+4) for i in 0:15]
    age_bins[end] = (75,120)

    age2bin_dict = Dict()
    for age in 0:99
        if age >= 75
            age2bin_dict[age] = 16
        else
            age2bin_dict[age] = (div(age, 5) + 1)
        end
    end
    println("length(age2bin_dict): ", length(age2bin_dict))
    return age2bin_dict
end

function ageBinCol(all_df, age2bin_dict)
    age_bin = zeros(Int64, nrow(all_df))
    age = all_df.age
    @show age[1:10]

    for r in 1:nrow(all_df)
        age_bin[r] = age2bin_dict[age[r]]
    end
    return age_bin
end


# Each house id can appear multiple times. I wish to change the id values to numbers between 1 and max_value

# Generate Household and Workplace graphs (one for each)
# The idea is to apply multigraphs. Both Households and Workplace graphs
# are composed of multiple disconnected graphs, one per household and one
# per workplace
function generateDemographicGraphs(params)
    p = params

    # symbols take 8 bytes
    #work_classes = [:default, :unemployed, :school,]
    #duties = [:None, :school, :work,]

    # Id's are converted to integers. Missing data is -1 index
    all_df = readData()
    # Replace missing values by -1 (assume these are all integers)
    all_df = coalesce.(all_df, -1)
    df = all_df  # Check against original code

    # person_id start at zero. Since I am programming in Julia, to avoid potential
    # errors, the numbering should start from 1
    df.person_id .+= 1

    # Create a dictionary: age2bin[age] return the bin number 1 thru 16
    age_bin = ageBinCol(df, age2AgeBin())
    # add an age_bins column to all_df
    df.age_bin = age_bin


    # Each house id can appear multiple times. I wish to change the id values to numbers between 1 and max_value
    # Create 2 databases, with missing values removed
    #   (person_id, school_id)
    #   (person_id, work_id)

    school_df = df[[:person_id, :school_id, :age_bin]]
    work_df = df[[:person_id, :work_id, :age_bin]]
    school_df = school_df[school_df.school_id .!= -1, :]
    work_df   = work_df[work_df.work_id .!= -1, :]

    school_ids_dict = idToIndex(school_df.school_id)
    work_ids_dict   = idToIndex(work_df.work_id)

    # Replace the columns in the three databases with the respective indexes
	# orig_school_id: as found in the synthetic population data
	# school_id: numbered 1 through #schools
    school_df.orig_school_id = school_df.school_id
    school_df.school_id = [school_ids_dict[is] for is in school_df.school_id]
    work_df.orig_work_id = work_df.work_id
    work_df.work_id = [work_ids_dict[is] for is in work_df.work_id]

    #println("school_ids_dict")
    #for k in keys(school_ids_dict)
        #println("$k, $(school_ids_dict[k])")
    #end

    #println("school_df")
    #println(first(school_df, 10))

    #return nothing, nothing, nothing

    printSchoolDF() = for r in 1:nrow(school_df) println(school_df[r,:]) end

    # The databases now have renumbered Ids
    #df_age = ageDistribution(df)   # NOT USED

    # Categories is a list of categories
    #column1 in each group is the person_id. I should rename the colums of the DataFrame
    # column name person_id became person
    categories = [:sp_hh_id, :work_id, :school_id]
    groups = sortPopulace(df, categories)

    # Regenerate the groups
    school_groups = groupby(school_df, :school_id)
    work_groups   = groupby(work_df,   :work_id)

    # These does not appear to be -1s any longer
    println("school df")
    println("work df")

    # investigate min/max sizes in groups
    sizes = []
    for grp in school_groups
        push!(sizes, nrow(grp))
    end
    println("min/max sizes in schools: $(minimum(sizes)), $(maximum(sizes)))")

    sizes = []
    for grp in work_groups
        push!(sizes, nrow(grp))
    end
    println("min/max sizes in workplace: $(minimum(sizes)), $(maximum(sizes)))")


    # Create a Home Master Graph. Homes are disconnected
    # Every person is in a home

    # Create a Work Master Graph. Homes are disconnected
    # Ideally the small world parameter 0.31, should come from a distribution
    home_graph = nothing
    work_graph = nothing
    school_graph = nothing

    tot_nb_people = nrow(df)

    #println("========================================")
    # make true once I get the school stuff working
	# Initialize in case
	work_dict = Dict()
	school_dict = Dict()

    if true
        println("===========================================")
        println("=== Generate Workplace graphs =============")

        weight = 0.8
        #@time work_graph, work_dict = createWorkGraph("workplace", tot_nb_people, work_df, work_groups, 0.1, weight, cmm)
        #@time work_graph, work_dict = createWorkGraph("workplaces", tot_nb_people, work_df, work_groups, 0.1, weight)  # ORIGINAL
        @time work_dict = createWorkGraph("workplaces", tot_nb_people, work_df, work_groups, 0.1, weight)
        #set_prop!(work_graph, :node_ids, work_df[:person_id])  # ORIGiNAL
		work_graph = nothing
    end

    #println("school_df: ", first(school_df, 5))

    if true
        println("========================================")
        println("=== Generate school graphs =============")
        weight = 0.4
		# First element of tuple in school_dict values is an edge_list
        @time school_dict = createWorkGraph("schools", tot_nb_people, school_df, school_groups, 0.1, weight) # OWN EDGE LIST
		school_graph = nothing
	end

    weight = 0.6
	println("===================================")
	println("==== Create Home Graphs ===========")
    @time home_graph = createHomeGraph(tot_nb_people, df, groups, weight)
    set_prop!(home_graph, :node_ids, df[:person_id])
    println("finished home")

    # TODO: Must make all graphs weighted

    return home_graph, work_graph, school_graph, work_dict, school_dict
end

# ---------------------------------------------------
