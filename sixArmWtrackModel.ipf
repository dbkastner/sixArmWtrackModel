#pragma TextEncoding = "MacRoman"
#pragma rtGlobals=3		// Use modern global access method and strict wave access.

// Main function is doModel(anWv,prms)
//	inputs are:
//		1) anWv: a N x  7 (second dimension has to be at least 7 collumns, but could be more) matrix
//			This matrix should contain information about the behavior of an animal to be compared to the model.
//			N is the number of total trials (arms visits) the animal performed and the model will perform
//				column 0 contains the session information, going sequentially from 0 to the number of total sessions the animal performed
//				column 1 contains the arms visited on each trial
//				column 2 contains whether that arm was reward (1) or not (0)
//				column 3 containts the identity of the center arm for the task
//				column 4 containts the identity of the left outer arm for the task
//				columns 5 and above can be blank
//		2) prms: a vector with 3 entries that contain the parameters of the model
//			entry 1 is gamma, the temporal discounting
//			entry 2 is alpha, the learning rate
//			entry 3 is omega, the forgetting rate
//
//	The doModel function creates 2 vectors for both the model and animal
//		1) modelOut: the outbound error likelihood across all trials for the repeats of the model
//		2) modelIn: the inbound error likelihood across all trials for the repeats of the model
//
//		1) anOut: the outbound error likelihood across all trials for the animal
//		2) anIn: the inbound error likelihood across all trials for the animal
//
//	The constant SmNum set the smoothing parameter of the Gaussian smoothing filter
//	The constant ModelReps determines how many repeats of the model contribute to the output of the model
//
//
//	The function that implements model M3 is an XOP written in C++. It has to be compiled and placed into the Igor Extensions folder in the WaveMetrics folder.
//	(See Igor XOP manual for details). The version here was implemented using Igor Pro 7.
//	sinThrdSfXOP(thisWave,trialsWv,cntgWv,prms)
//		Inputs are:
//		1) thisWave: a N x 7 (second dimension has to be at least 7 collumns, but could be more) matrix
//			most of the matrix can be blank, but column 1 must contain the session information (just as anWv described above)
//			column 3 must containt the identity of the center arm for the task
//			column 4 must containt the identity of the left outer arm for the task
//			column 6 must contain a set of random numbers from [0,1]
//		2) trialsWv: is a vector the length of which is dictated by the number of sessions, each entry contains the number of trials in that session
//		3) cntgWv: is a vector the length of which is dictated by the number of sessions, each entry contains the contingency number for that session
//			if the value is <0 then the model just reproduces the exact behavior of the input animal
//		4) prms: a vector with 3 entries that contain the parameters of the model
//			entry 1 is gamma, the temporal discounting
//			entry 2 is alpha, the learning rate
//			entry 3 is omega, the forgetting rate

CONSTANT SmNum=10,ModelReps=200

function doModel(anWv,prms)
	wave anWv,prms
	processSingle(anWv)
	duplicate /o anWv allVisits
	doMany_ThrdSf(allVisits,prms)
end

function processSingle(wv)
	wave wv
	getCotingNum(wv)
	wave wConting,numTrials
	duplicate /o wConting conting4model
	conting4model-=1
	
	viewOne_ThrdSf(wv,numTrials,0)
	wave singleIn0,singleOut0
	duplicate /o singleIn0 anIn
	duplicate /o singleOut0 anOut
end

function getCotingNum(wv)
	wave wv
	wavestats /q/rmd=[][0] wv
	make /o/n=(dimsize(wv,0)) hold
	variable mn=v_min
	variable mx=v_max
	make /o/n=(mx-mn+1) numTrials,wConting
	variable start,finish
	variable home=NaN,outer=NaN
	variable i,j
	for(i=mn;i<=mx;i+=1)
		hold=selectnumber(wv[p][0]==i,NaN,p)
		wavestats /q hold
		numTrials[i-mn]=v_npnts
		start=v_min
		if(i==mn)
			wConting[i-mn]=0
			home=wv[start][3]
			outer=wv[start][4]		
		elseif(wv[start][3]>=0)
				if(home!=wv[start][3] || outer!=wv[start][4])
					wConting[i-mn]=wConting[i-mn-1]+1
					home=wv[start][3]
					outer=wv[start][4]
				else
					wConting[i-mn]=wConting[i-mn-1]
				endif
		elseif((wv[start][3]>=0)==0)
			if(home>=0)
				wConting[i-mn]=wConting[i-mn-1]+1
				home=wv[start][3]
				outer=wv[start][4]
			else
				wConting[i-mn]=wConting[i-mn-1]
			endif
		endif
	endfor
end

function doMany_ThrdSf(wv,prms)
	wave wv,prms
	wave numTrials,conting4model
	make /o/n=(dimsize(wv,0),modelReps) everyIn,everyOut
	
	Variable nthreads= ThreadProcessorCount
	Variable threadGroupID= ThreadGroupCreate(nthreads)
	
	variable i,repNum
	for(repNum=0; repNum<ModelReps;)
		for(i=0;i<nthreads;i+=1)
			ThreadStart threadGroupID,i,myThreadFunc(wv,numTrials,conting4model,prms,everyIn,everyOut,repNum)
			repNum+=1
			if( repNum>=ModelReps )
				break
			endif
		endfor
		
		do
			Variable threadGroupStatus = ThreadGroupWait(threadGroupID,100)
		while( threadGroupStatus != 0 )
	endfor
	Variable dummy= ThreadGroupRelease(threadGroupID)
	
	wave everyIn,everyOut
	getAvg(everyIn)
	wave avg
	duplicate /o avg modelIn
	getAvg(everyOut)
	duplicate /o avg modelOut
end

ThreadSafe function myThreadFunc(wv,trialsWv,cntgWv,prms,inWv,outWv,repNum)
	wave wv,trialsWv,cntgWv,prms,inWv,outWv
	variable repNum
	duplicate /o wv $"visits"+num2str(repNum)
	wave thisWave=$"visits"+num2str(repNum)
	setRandomSeed /BETR (repNum+1)/1000
	thisWave[][6]=abs(enoise(1))
	sinThrdSfXOP(thisWave,trialsWv,cntgWv,prms)
	viewOne_ThrdSf(thisWave,trialsWv,repNum)
	wave thisSingIn=$"singleIn"+num2str(repNum)
	wave thisSingOut=$"singleOut"+num2str(repNum)
	inWv[][repNum]=thisSingIn[p]
	outWv[][repNum]=thisSingOut[p]
	
	return stopMSTimer(-2)		// time when we finished
end

ThreadSafe function viewOne_ThrdSf(wv,trialsWv,val)
	wave wv,trialsWv
	variable val

	getSubErrors_ThrdSf(wv,trialsWv,val)
	wave thisInE=$"inErrors"+num2str(val)
	wave thisOuE=$"outErrors"+num2str(val)
	wave thisInT=$"inTot"+num2str(val)
	wave thisOuT=$"outTot"+num2str(val)
	duplicate /o thisInE,$"singleIn"+num2str(val),$"singleOut"+num2str(val)
	wave thisSingIn=$"singleIn"+num2str(val)
	wave thisSingOut=$"singleOut"+num2str(val)
	sort thisInT,thisInT,thisInE
	wavestats /q/m=1 thisInT
	deletepoints v_npnts,v_numNaNs,thisInT,thisInE
	sort thisOuT,thisOuT,thisOuE
	wavestats /q/m=1 thisOuT
	deletepoints v_npnts,v_numNaNs,thisOuT,thisOuE
	smooth smNum,thisInE
	smooth smNum,thisOuE
	interpolate2 /T=1/Y=thisSingIn/i=3 thisInT,thisInE
	interpolate2 /T=1/Y=thisSingOut/i=3 thisOuT,thisOuE 
end

ThreadSafe function getSubErrors_ThrdSf(wv,trialsWv,vl)
	wave wv,trialsWv
	variable vl
	variable startTrial,val
	make /o/n=(dimsize(wv,0)) $"outErrors"+num2str(vl)=0
	wave thisOuE=$"outErrors"+num2str(vl)
	make /o/n=(dimsize(wv,0)) $"inErrors"+num2str(vl)=0
	wave thisInE=$"inErrors"+num2str(vl)
	make /o/n=(dimsize(wv,0)) $"inTot"+num2str(vl)=NaN
	wave thisInT=$"inTot"+num2str(vl)
	make /o/n=(dimsize(wv,0)) $"outTot"+num2str(vl)=NaN
	wave thisOuT= $"outTot"+num2str(vl)
	make /o/n=(dimsize(wv,0)) $"hold"+num2str(vl)
	wave thisHold=$"hold"+num2str(vl)
	variable home
	variable i,j,l
	for(i=0;i<numpnts(trialsWv);i+=1)
		val=trialsWv[i]
		for(j=startTrial;j<startTrial+val;j+=1)
			if(wv[j][3]>5)
				home=wv[j][3]-5
			else
				home=wv[j][3]
			endif
			if(home>=0)
				if(j==startTrial)
					thisInT[l]=l
				elseif(wv[j-1][1]==home)
					thisOuT[l]=l
				else
					thisInT[l]=l
				endif
				if(wv[j][2]==0)
					if(j==startTrial)
						if(wv[j][3]>5)
							if(wv[j][1]==home && wv[j][7]==1)
								thisInE[l]=0
							else
								thisInE[l]=1
							endif
						else
							thisInE[l]=1
						endif
					elseif(wv[j-1][1]==home)
						thisOuE[l]=1
					else
						if(wv[j][3]>5)
							if(wv[j][1]==home && wv[j][7]==1)
								thisInE[l]=0
							else
								thisInE[l]=1
							endif
						else
							thisInE[l]=1
						endif
					endif
				endif
			else
				thisInT[l]=l
				thisOuT[l]=l
				if(wv[j][2]==0)
					thisInE[l]=1
					thisOuE[l]=0
				else
					thisInE[l]=0
					thisOuE[l]=0
				endif
			endif
			l+=1
		endfor
		startTrial+=val
	endfor
end

function getAvg(wv)
	wave wv
	make /o/n=(dimsize(wv,0)) avg
	setscale /p x,dimoffset(wv,0),dimdelta(wv,0),avg
	variable i
	for(i=0;i<numpnts(avg);i+=1)
		wavestats /q/m=1/rmd=[i][] wv
		avg[i]=v_avg
	endfor
end
