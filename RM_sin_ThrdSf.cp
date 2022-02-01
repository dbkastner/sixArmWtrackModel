/*	RM_sin_ThrdSf.c -- illustrates Igor external functions.
	
	HR, 091021
		Updated for 64-bit compatibility.

	HR, 2013-02-08
		Updated for Xcode 4 compatibility. Changed to use XOPMain instead of main.
		As a result the XOP now requires Igor Pro 6.20 or later.
*/

#include "XOPStandardHeaders.h"			// Include ANSI headers, Mac headers, IgorXOP.h, XOP.h and XOPSupport.h
#include "RM_sin_ThrdSf.h"
#include <math.h>

/* Global Variables (none) */
const int numStates=43;
const int totalActions=6;

int getState(CountInt lastArm, CountInt memUnit)
{
    int state;
    if (lastArm < 0) {
        state = totalActions * totalActions;
    }
    else if (memUnit < 0) {
        state = lastArm + 1 + totalActions * totalActions;
    }
    else {
        state = lastArm + memUnit * totalActions;
    }
    
    return state;
}

int softMax(float prob[totalActions], float aBias[totalActions], float b1, float b2, CountInt lastArm, CountInt memUnit, float randVal, int anAction, CountInt which)
{
    int i;
    for(i=0; i<totalActions; i++) {
        prob[i] = exp(prob[i]) * exp(aBias[i]);
    }
    
    if(lastArm >= 0) {
        if(lastArm > 0) {
            prob[lastArm-1] *= exp(b1);
        }
        if(lastArm < (totalActions-1)) {
            prob[lastArm+1] *= exp(b1);
        }
        if(lastArm > 1) {
            prob[lastArm-2] *= exp(b2);
        }
        if(lastArm < (totalActions-2)) {
            prob[lastArm+2] *= exp(b2);
        }
        prob[lastArm] = 0;
    }
    
    double sumVal=0;
    for(i=0; i<totalActions; i++) {
        sumVal+=prob[i];
    }
    
    for(i=0; i<totalActions; i++) {
        prob[i] /= sumVal;
    }
    
    int act;
    
    if(which==0)
        act=anAction;
    else {
        float randPicker=0;
        for(i=1; i<totalActions; i++) {
            randPicker += prob[i-1];
            if(randVal < randPicker) {
                act = i-1;
                break;
            }
            if(i==(totalActions-1)) {
                act = i;
            }
        }
    }
    return act;
}

int getRewards(int act, CountInt lastArm, CountInt &lastOuter, int goalArms[3])
{
    int reward;
    int home=goalArms[1];
    int outer1=goalArms[0];
    int outer2=goalArms[2];
    if (home>=0) {
        if (act == home) {
            if (lastArm == act) {
                reward=0;
            }
            else {
                reward=1;
            }
        }
        else if (act == outer1 || act == outer2) {
            if (lastArm == home) {
                if (lastOuter != act) {
                    reward=1;
                }
                else {
                    reward=0;
                }
            }
            else {
                reward=0;
            }
            lastOuter=act;
        }
        else {
            reward=0;
        }
    }
    else {
        reward=1;
    }
    
    return reward;
}

void updateMotorPref(float motPref[numStates][totalActions], float eleg[totalActions], int state, float gamma, float alpha, float omega, float delta)
{
    int i,j;
    for(i=0; i<numStates; i++) {
        for(j=0; j<totalActions; j++) {
            motPref[i][j] *= 1-omega;
            if(i==state)
                motPref[i][j] += delta*alpha*eleg[j];
        }
    }
}

void updateNeigh(float prob[numStates], float &bias, int act, CountInt lastArm, float omega, float alpha, float delta, int whichBias)
{
    float probVals=0;
    
    if(lastArm >= 0) {
        if(lastArm > (whichBias-1)) {
            probVals+=prob[lastArm-whichBias];
        }
        if(lastArm < (totalActions-whichBias)) {
            probVals+=prob[lastArm+whichBias];
        }
    }
    
    IndexInt one = ((lastArm-act)==whichBias || (act-lastArm)==(whichBias));
    
    bias=bias * (1 - omega) + alpha * delta * (one-probVals);
}

int doIterate(int goalArms[3], float motPref[numStates][totalActions], float vle[numStates], float aBias[totalActions], float &b1, float &b2, float gamma, float alpha, float omega, CountInt &lastArm, CountInt &memUnit, CountInt &lastOuter, int anAction, int anReward, CountInt which, float randVal) {
    
    int state=getState(lastArm,memUnit);
    
    float probs[totalActions];
    int i;
    for(i=0;i<totalActions;i++) {
        probs[i]=motPref[state][i];
    }
    
    int act=softMax(probs,aBias,b1,b2,lastArm,memUnit,randVal,anAction,which);
    
    float eleg[totalActions];
    for(i=0; i<totalActions; i++) {
        if (i==act)
            eleg[i] = 1-probs[i];
        else
            eleg[i] = -probs[i];
    }
    
    int reward;
    if(which==1)
        reward=getRewards(act,lastArm,lastOuter,goalArms);
    else
        reward=anReward;
    
    memUnit=lastArm;
    lastArm=act;
    
    int newState=getState(lastArm,memUnit);
    
    float delta=reward+gamma*vle[newState]-vle[state];
    
    for(i=0; i<numStates; i++) {
        vle[i] *= 1-omega;
        if(i==state)
            vle[i] += delta*alpha;
    }
    
    updateMotorPref(motPref,eleg,state,gamma,alpha,omega,delta);
    
    for(i=0; i<totalActions; i++) {
        aBias[i] *= 1-omega;
        if (i==act)
            aBias[i] += delta*alpha*(1-probs[i]);
        else
            aBias[i] += delta*alpha*(-probs[i]);
    }
    
    updateNeigh(probs,b1,act,memUnit,omega,alpha,delta,1);
    updateNeigh(probs,b2,act,memUnit,omega,alpha,delta,2);
    
    return reward;
}

#pragma pack(2)        // All structures passed to Igor are two-byte aligned.
struct sinThrdSfXOPParams  {
    waveHndl wParams;
    waveHndl wConting;
    waveHndl wTrials;
    waveHndl wOut;
    UserFunctionThreadInfoPtr tp;
    double result;
};
typedef struct sinThrdSfXOPParams sinThrdSfXOPParams;
#pragma pack()        // Reset structure alignment to default.

extern "C" int
sinThrdSfXOP(sinThrdSfXOPParams* p)
{
    waveHndl wavHtrial;
    waveHndl wavHconting;
    waveHndl wavHout;
    waveHndl wavHparams;
    int waveType;
    int numDimensions;
    CountInt dimensionSizes[MAX_DIMENSIONS+1];
    char* dataStartPtr;
    IndexInt dataOffset;
    CountInt numRows;
    CountInt pointsPerColumnOut;
    float *fpTrial0, *fpTrial;
    float *fpConting0, *fpConting;
    float *fpOut0, *fpOut;
    float *fpParams0, *fpParams;
    int result;
    
    wavHtrial = p->wTrials;
    if (wavHtrial == NIL)
        return NOWAV;
    wavHconting = p->wConting;
    if (wavHconting == NIL)
        return NOWAV;
    wavHout = p->wOut;
    if (wavHout == NIL)
        return NOWAV;
    wavHparams = p->wParams;
    if (wavHout == NIL)
        return NOWAV;
    
    waveType = WaveType(wavHtrial);
    if (waveType != NT_FP32)
        return NOWAV;
    waveType = WaveType(wavHconting);
    if (waveType != NT_FP32)
        return NOWAV;
    waveType = WaveType(wavHout);
    if (waveType != NT_FP32)
        return NOWAV;
    waveType = WaveType(wavHparams);
    if (waveType != NT_FP32)
        return NOWAV;
    
    if (result = MDGetWaveDimensions(wavHtrial, &numDimensions, dimensionSizes))
        return result;
    numRows=dimensionSizes[0];
    if (result = MDGetWaveDimensions(wavHout, &numDimensions, dimensionSizes))
        return result;
    pointsPerColumnOut =dimensionSizes[0];
    
    if (result = MDAccessNumericWaveData(wavHtrial, kMDWaveAccessMode0, &dataOffset))
        return result;
    dataStartPtr = (char*)(*wavHtrial) + dataOffset;
    fpTrial0 = (float*)dataStartPtr;
    if (result = MDAccessNumericWaveData(wavHconting, kMDWaveAccessMode0, &dataOffset))
        return result;
    dataStartPtr = (char*)(*wavHconting) + dataOffset;
    fpConting0 = (float*)dataStartPtr;
    if (result = MDAccessNumericWaveData(wavHout, kMDWaveAccessMode0, &dataOffset))
        return result;
    dataStartPtr = (char*)(*wavHout) + dataOffset;
    fpOut0 = (float*)dataStartPtr;
    if (result = MDAccessNumericWaveData(wavHparams, kMDWaveAccessMode0, &dataOffset))
        return result;
    dataStartPtr = (char*)(*wavHparams) + dataOffset;
    fpParams0 = (float*)dataStartPtr;
    result=0;
    
    IndexInt i,j;
    float motorPref[numStates][totalActions];
    float value[numStates];
    for(i=0;i<numStates;i++){
        value[i]=0;
        for(j=0;j<totalActions;j++){
            motorPref[i][j]=0;
        }
    }
    
    float aBiasVals[totalActions];
    for(i=0;i<totalActions;i++){
        aBiasVals[i]=0;
    }
    float b1=0;
    float b2=0;
    
    fpParams = fpParams0 + 0;
    float gamma=*fpParams;
    fpParams = fpParams0 + 1;
    float alpha=*fpParams;
    fpParams = fpParams0 + 2;
    float omega=*fpParams;
    
    IndexInt row,trialCnt;
    CountInt numTrials,which,trial,conting,lastOuter,lastArm,memUnit;
    int goalArms[3];
    int anAction,anReward;
    int reward;
    float randVal;
    trial=0;
    for(row=0; row<numRows; row++){
        fpTrial = fpTrial0 + row;
        numTrials = *fpTrial;
        fpConting = fpConting0 + row;
        conting = *fpConting;
        if(conting>=0) {
            fpOut=fpOut0 + pointsPerColumnOut*3 + trial;
            goalArms[1]=*fpOut;
            fpOut=fpOut0 + pointsPerColumnOut*4 + trial;
            goalArms[0]=*fpOut;
            goalArms[2]=goalArms[1]+(goalArms[1]-goalArms[0]);
            which=1;
        }
        else {
            goalArms[0]=NAN;
            goalArms[1]=NAN;
            goalArms[2]=NAN;
            which=0;
        }
        lastArm=-1;
        memUnit=-1;
        lastOuter=-1;
        for(trialCnt=0; trialCnt<numTrials; trialCnt++) {
            fpOut=fpOut0 + pointsPerColumnOut*1 + trial;
            anAction=*fpOut;
            fpOut=fpOut0 + pointsPerColumnOut*2 + trial;
            anReward=*fpOut;
            fpOut=fpOut0 + pointsPerColumnOut*6 + trial;
            randVal=*fpOut;
            reward=doIterate(goalArms,motorPref,value,aBiasVals,b1,b2,gamma,alpha,omega,lastArm,memUnit,lastOuter,anAction,anReward,which,randVal);
            
            fpOut=fpOut0 + pointsPerColumnOut*1 + trial;
            *fpOut=lastArm;
            fpOut=fpOut0 + pointsPerColumnOut*2 + trial;
            *fpOut=reward;
            trial++;
        }
    }
    
    return(0);                    /* XFunc error code */
}


static XOPIORecResult
RegisterFunction()
{
	int funcIndex;

	funcIndex = (int)GetXOPItem(0);	/* which function invoked ? */
	switch (funcIndex) {
		case 0:						/* RM_sin_ThrdSfAdd(p1, p2) */
			return (XOPIORecResult)sinThrdSfXOP;
			break;
	}
	return 0;
}

/*	DoFunction()

	This will actually never be called because all of the functions use the direct method.
	It would be called if a function used the message method. See the XOP manual for
	a discussion of direct versus message XFUNCs.
*/
static int
DoFunction()
{
	int funcIndex;
	void *p;				/* pointer to structure containing function parameters and result */
	int err;

	funcIndex = (int)GetXOPItem(0);	/* which function invoked ? */
	p = (void*)GetXOPItem(1);		/* get pointer to params/result */
	switch (funcIndex) {
		case 0:						/* RM_sin_ThrdSfAdd(p1, p2) */
			err = sinThrdSfXOP((sinThrdSfXOPParams*)p);
			break;
	}
	return(err);
}

/*	XOPEntry()

	This is the entry point from the host application to the XOP for all messages after the
	INIT message.
*/

extern "C" void
XOPEntry(void)
{	
	XOPIORecResult result = 0;

	switch (GetXOPMessage()) {
		case FUNCTION:								/* our external function being invoked ? */
			result = DoFunction();
			break;

		case FUNCADDRS:
			result = RegisterFunction();
			break;
	}
	SetXOPResult(result);
}

/*	XOPMain(ioRecHandle)

	This is the initial entry point at which the host application calls XOP.
	The message sent by the host must be INIT.
	
	XOPMain does any necessary initialization and then sets the XOPEntry field of the
	ioRecHandle to the address to be called for future messages.
*/

HOST_IMPORT int
XOPMain(IORecHandle ioRecHandle)			// The use of XOPMain rather than main means this XOP requires Igor Pro 6.20 or later
{	
	XOPInit(ioRecHandle);					// Do standard XOP initialization
	SetXOPEntry(XOPEntry);					// Set entry point for future calls
	
	if (igorVersion < 620) {
		SetXOPResult(OLD_IGOR);
		return EXIT_FAILURE;
	}
	
	SetXOPResult(0);
	return EXIT_SUCCESS;
}
