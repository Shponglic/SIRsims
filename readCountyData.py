'''
This script does the following:
1. Loads data from the DOH csv files
2. creates plots for case counts by zipcode, county or just the top 10 counties by case count
'''
import numpy as np
import seaborn, os, sys
from datetime import datetime
import pickle
import pandas as pd
import matplotlib.pyplot as plt
home_dir='zipcode_data/'
convert_nonstring=lambda x: 0 if not x.isnumeric() else x
file_list=os.listdir(home_dir)
county_data_file="county_data_file_combined.pkl"
county_data_file_dates="county_data_file_dates.pkl"
def readData():
    '''
        Read data from pickle file if it exists and reads newer data if available
        If not, reads from .csv files and puts them into pickle file
        Returns list of dates for which data is available
    '''
    if os.path.isfile(county_data_file_dates):
        dates=pickle.load(open(county_data_file_dates,'rb'))
        data=pickle.load(open(county_data_file,'rb'))
        print("Earlier data loaded. Latest date is ",datetime.strptime(str(sorted(dates)[-1]),'%m%d%Y'))
    else:
        dates=set()
        data={} #key is date, value is a dictionary
    flag=0
    for filename in file_list:
        file_date=filename[-12:-4]
        if filename[-3:]!='csv':
            continue
        elif file_date in dates:
            continue
        else:
            flag=1
            dates.add(file_date)
            print("Reading from ",home_dir+filename)
            data_temp=pd.read_csv(home_dir+filename,header=0,usecols=['ZIP','OBJECTID_1','COUNTYNAME','Cases_1'], converters={'Cases_1':convert_nonstring})
            data[file_date]=data_temp

    if flag==0:
        print("No newer data available")
    if flag==1:
        print("New data loaded till ", sorted(dates)[-1])
        pickle.dump(dates,open(county_data_file_dates,'wb'))
        pickle.dump(data,open(county_data_file,'wb'))
    return (dates,data)

def structureData(dates: list, data):
    sorted_dates=list(sorted(dates))
    counties=set()
    for row in data[sorted_dates[0]].iterrows():
        counties.add(row[1].COUNTYNAME)
    county_list=list(counties)
    cumulativeData=np.zeros((len(counties),len(sorted_dates)))
    #Put data into numpy array for easy manipulation and access
    for c, date in enumerate(sorted_dates):
        for row in data[date].iterrows():
            x_idx=county_list.index(row[1].COUNTYNAME)
            cumulativeData[x_idx][c]+=float(row[1].Cases_1)
    #Uncomment following block to print county-wise cumulative data
    # for idx, r in enumerate(cumulativeData):
    #     print(county_list[idx])
    #     for c in r:
    #         print(c, end=" ")
    #     print()
    return (cumulativeData, county_list)

def plotThreshold(threshold, dates, cumulativeData, county_list):
    '''
        This function plots cases over dates for all counties that have a cumulative case count more 
        than the threshold value
    '''
    hr_counties=[]
    for idx, r in enumerate(cumulativeData):
        #print(r)
        if r[-1]>=threshold:
            hr_counties.append(idx)
    plt.figure(figsize=(10,8))
    for v in hr_counties:
        plt.plot(sorted(dates), cumulativeData[v], label=county_list[v])
        plt.annotate(str(cumulativeData[v][-1]),xy=(sorted(dates)[-1],cumulativeData[v][-1]*1.01))
    plt.xticks(rotation=45)
    plt.legend(loc="best")
    title_temp="Counties with cumulative case count over "+ str(threshold)
    plt.title(title_temp)
    figname="Threshold_"+str(threshold)+datetime.now().strftime("%d%m%y%H%M")+".png"
    plt.savefig(figname)
    plt.show()
    

def plotCounty(counties, dates, cumulativeData, county_list, save_c='N'):
    '''
        This function plots cases over dates for the specified counties
    '''
    plt.figure(figsize=(12,9))
    for county in counties:
        idx=county_list.index(county)
        plt.plot(sorted(dates),cumulativeData[idx], label=county)
        plt.annotate(str(cumulativeData[idx][-1]),xy=(sorted(dates)[-1],cumulativeData[idx][-1]*1.01))
    title="Cumulative case count trend for counties"+str(counties)
    plt.title(title)
    plt.legend(loc="best")
    figname="Counties_"+datetime.now().strftime("%d%m%y%H%M")+".png"
    plt.xticks(rotation=45)
    if save_c=='Y' or save_c=='y':
        plt.savefig(figname)
    plt.show()
    
def plotZipcodes(zipcodes, dates, data, save_c='N'):
    '''
        This function plots cases over dates for the specified county
    '''
    plt.figure(figsize=(12,9))
    for zipcode in zipcodes:
        #print(zipcode)
        temp_data=[]
        for date in sorted(dates):
            temp_df=data[date][data[date]['ZIP']==zipcode]
            case_count=0
            for val in temp_df['Cases_1']:
                case_count+=int(val)
            temp_data.append(case_count)
        plt.plot(sorted(dates),temp_data, label=zipcode)
        plt.annotate(str(temp_data[-1]),xy=(sorted(dates)[-1],temp_data[-1]*1.01))
    title="Cumulative case count for zipcodes\n"+str(zipcodes)
    plt.title(title)
    plt.legend(loc="best")
    plt.xticks(rotation=45)
    figname="Zipcodes_"+datetime.now().strftime("%d%m%y%H%M")+".png"
    if save_c=='Y' or save_c=='y':
        plt.savefig(figname)
    plt.show()
    

def plotDefault(dates, cumulativeData, county_list, save_c='N'):
    '''
        This function plots cases over dates for the top 10 counties in case count
    '''
    top_10_idxs=np.argsort(cumulativeData[:,-1])[-10:]
    top_10=cumulativeData[top_10_idxs]
    title="Top 10 counties in cumulative case count as of "+str(sorted(dates)[-1])
    figname="Top 10_"+datetime.now().strftime("%d%m%y%H%M")+".png"
    plt.figure(figsize=(10,8))
    for i in range(10):
        plt.plot(sorted(dates),top_10[i], label=county_list[top_10_idxs[i]])
        plt.annotate(str(top_10[i][-1]),xy=(sorted(dates)[-1],top_10[i][-1]*1.01))
    plt.xticks(rotation=45)
    plt.title(title)
    plt.legend(loc="best")
    if save_c=='Y' or save_c=='y':
        plt.savefig(figname)
    plt.show()
    

def main(default=None,threshold: int=None, county: str=None, zipcode: str=None, save_c: str='N'):
    '''
        To run this program, you will need files as given below:
        home_dir='zipcode_data/' - This will be the folder containing all the csv files
        county_data_file="county_data_file_combined.pkl" - Contains older data. If unavailable, it will be generated. If newer data is present, this will be updated
        county_data_file_dates="county_data_file_dates.pkl" - Contains list of dates. If unavailable, it will be generated. If newer data is present, this will be updated
    '''
    dates,data=readData()
    cumulativeData,county_list=structureData(dates, data)
    if zipcode:
        zipcodes=zipcode.split(",")
        zipcodes=list(map(lambda x : int(x) if x.isnumeric() else None, zipcodes))
        plotZipcodes(zipcodes, dates, data, save_c)
    elif threshold:
        plotThreshold(threshold, dates, cumulativeData, county_list, save_c)
    elif county:
        counties=county.split(',')
        plotCounty(counties, dates, cumulativeData, county_list, save_c)
    elif default:
        plotDefault(dates, cumulativeData, county_list, save_c)

if __name__=="__main__":
    save_c=input("Save plots? [Y/N] ")
    if len(sys.argv)==1:
        main()
        print("Other Usage:\n[-d|-default]\n[-county|-c] countyname\n[-zipcode|-z] zipcode list(comma-separated without spaces)\n[-threshold|-t] threshold")
    elif sys.argv[1]=='-county' or sys.argv[1]=='-c':
        main(county=''.join(sys.argv[2:]), save_c=save_c)
    elif sys.argv[1]=='-default' or sys.argv[1]=='-d':
        main(default=1,  save_c=save_c)
    elif sys.argv[1]=='-zipcode' or sys.argv[1]=='-z':
            main(zipcode=''.join(sys.argv[2:]),  save_c=save_c)
    elif sys.argv[1]=='-threshold' or sys.argv[1]=='-t':
        if sys.argv[2].isnumeric():
            main(int(sys.argv[2]),  save_c=save_c)
        else:
            print("Invalid threshold value")

    