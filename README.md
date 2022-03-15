Code implementing model from paper “Spatial preferences account for inter-animal variability during the continual learning of a dynamic cognitive task.“

Code can be run within Igor Pro version 7.0 or later. XOP function written for Igor version 7.0.

anData.txt contains data from the animal displayed in Fig. 3A. Structure of the data is explained in the sixArmWtrackModel.ipf file. The parameters used for that animal are: gamma=0.020538045, alpha=0.96187174, omega=0.0042890618.

allData.txt contain the data for all animals used in this study. The structure of the file is as follows:
column 0 contains the animal identity number.
column 1 contains the session information, going sequentially from 0 to the number of total sessions the animal performed.
column 2 contains the arms visited on each trial.
column 3 contains whether that arm was rewarded (1) or not (0).
column 4 containts the identity of the center arm for the task.
column 5 containts the identity of the left outer arm for the task.
column 6 contains whether the poke broke the beam of the reward well (1) or it if was a missed poke (0).
