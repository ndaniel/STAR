#include "IncludeDefine.h"
#include "Parameters.h"
#include "Transcript.h"
#include "ReadAlign.h"
#include "SequenceFuns.h"
#include "stitchWindowAligns.h"
#include "sjSplitAlign.cpp"
#include "PackedArray.h"
#include "alignSmithWaterman.h"
#include "GlobalVariables.h"
#include <time.h>

void ReadAlign::stitchPieces(char **R, char **Q, char *G, PackedArray& SA, uint Lread) {
    
       //zero-out winBin
    memset(winBin[0],255,sizeof(winBin[0][0])*P->winBinN);
    memset(winBin[1],255,sizeof(winBin[0][0])*P->winBinN);

    
//     for (uint iWin=0;iWin<nWall;iWin++) {//zero out winBin
//         if (WC[iWin][WC_gStart]<=WC[iWin][WC_gEnd]) {//otherwise the window is dead
//             memset(&(winBin[WC[iWin][WC_Str]][WC[iWin][WC_gStart]]),255,sizeof(winBin[0][0])*(WC[iWin][WC_gEnd]-WC[iWin][WC_gStart]+1));
//         };
// //         for (uint ii=C[iWin][WC_gStart]; ii<WC[iWin][WC_gEnd]; ii++) {
// //             winBin[WC[WC_Str]
// //         };
//     };
    
//     //debug
//     for (uint ii=0;ii<P->winBinN;ii++){
//         if (winBin[0][ii]!=uintWinBinMax || winBin[1][ii]!=uintWinBinMax) {
//             cerr<< "BUG in stitchPieces: ii="<<ii<<"   "<< winBin[0][ii] <<"   "<<winBin[1][ii] <<"   iRead="<<iRead<<"   nW="<<nW<<endl;
//             for (uint iWin=0;iWin<nW;iWin++) {
//                 cerr <<WC[iWin][WC_gStart]<<"   " <<WC[iWin][WC_gEnd] <<"   "<<WC[iWin][WC_Str] <<endl;
//             };
//             exit(1);
//         };
//     };
    
    
    nW=0; //number of windows
    for (uint iP=0; iP<nP; iP++) {//scan through all anchor pieces, create alignment windows

//          if (PC[iP][PC_Nrep]<=P->winAnchorMultimapNmax || PC[iP][PC_Length]>=readLength[PC[iP][PC_iFrag]] ) {//proceed if piece is an anchor, i.e. maps few times or is long enough
       if (PC[iP][PC_Nrep]<=P->winAnchorMultimapNmax ) {//proceed if piece is an anchor, i.e. maps few times
            
            uint aDir   = PC[iP][PC_Dir];     
            uint aLength= PC[iP][PC_Length];            

            for (uint iSA=PC[iP][PC_SAstart]; iSA<=PC[iP][PC_SAend]; iSA++) {//scan through all alignments of this piece

                uint a1 = SA[iSA];
                uint aStr = a1 >> P->GstrandBit;           
                a1 &= P->GstrandMask; //remove strand bit

                //convert to positive strand
                if (aDir==1 && aStr==0) {
                    aStr=1;
                } else if (aDir==0 && aStr==1) {
                    a1 = P->nGenome - (aLength+a1);
                } else if (aDir==1 && aStr==1) {
                    aStr=0;
                    a1 = P->nGenome - (aLength+a1);         
                };

                //final strand            
                if (revertStrand) { //modified strand according to user input CHECK!!!!
                    aStr=1-aStr;
                };   

                if (a1>=P->sjGstart) {//this is sj align
                    uint a1D, aLengthD, a1A, aLengthA, sj1;              
                    if (sjAlignSplit(a1, aLength, P, a1D, aLengthD, a1A, aLengthA, sj1)) {//align crosses the junction

                        int addStatus=createExtendWindowsWithAlign(a1D, aStr);//add donor piece
                        if (addStatus==EXIT_createExtendWindowsWithAlign_TOO_MANY_WINDOWS) {//too many windows
                            break;
                        };         
                        addStatus=createExtendWindowsWithAlign(a1A, aStr);//add acceptor piece
                        if (addStatus==EXIT_createExtendWindowsWithAlign_TOO_MANY_WINDOWS) {//too many windows
                            break;
                        };                       
                    };
                } else {//this is a normal genomic read
                    int addStatus=createExtendWindowsWithAlign(a1, aStr);
                    if (addStatus==EXIT_createExtendWindowsWithAlign_TOO_MANY_WINDOWS) {//too many windows
                        break;
                    };                
                };           
            }; //for (uint iSA=PC[iP][PC_SAstart]; iSA<=PC[iP][PC_SAend]; iSA++) //scan through all alignments of this piece
        };//if (PC[iP][PC_Nrep]<=P->winAnchorMultimapNmax) //proceed if anchor
    };//for (uint iP=0; iP<nP; iP++) //scan through all anchor pieces, create alignment windows
    
    
    for (uint iWin=0;iWin<nW;iWin++) {//extend windows with flanks
        if (WC[iWin][WC_gStart]<=WC[iWin][WC_gEnd]) {//otherwise the window is dead
                       
            uint wb=WC[iWin][WC_gStart];
            for (uint ii=0; ii<P->winFlankNbins && wb>0 && P->chrBin[(wb-1) >> P->winBinChrNbits]==WC[iWin][WC_Chr];ii++) {
                wb--;
                winBin[ WC[iWin][WC_Str] ][ wb ]=(uintWinBin) iWin;
            };
            WC[iWin][WC_gStart] = wb;
            
            wb=WC[iWin][WC_gEnd];
            for (uint ii=0; ii<P->winFlankNbins && wb+1<P->winBinN && P->chrBin[(wb+1) >> P->winBinChrNbits]==WC[iWin][WC_Chr];ii++) {
                wb++;
                winBin[ WC[iWin][WC_Str] ][ wb ]=(uintWinBin) iWin;
            };
            WC[iWin][WC_gEnd] = wb;
            
          
        };
        nWA[iWin]=0; //initialize nWA
        WALrec[iWin]=0; //initialize rec-length        
        WlastAnchor[iWin]=-1;
    };
    
    nWall=nW;
    
    for (uint iP=0; iP<nP; iP++) {//scan through all pieces/aligns, add them to alignment windows, create alignment coordinates
        uint aNrep=PC[iP][PC_Nrep];
        uint aFrag=PC[iP][PC_iFrag];  
        uint aLength=PC[iP][PC_Length];      
        uint aDir=PC[iP][PC_Dir];     
        
        bool aAnchor=(aNrep<=P->winAnchorMultimapNmax); //this align is an anchor or not            

        for (uint ii=0;ii<nW;ii++) {//initialize nWAP
            nWAP[ii]=0;
        };
        
        for (uint iSA=PC[iP][PC_SAstart]; iSA<=PC[iP][PC_SAend]; iSA++) {//scan through all alignments

            uint a1 = SA[iSA];
            uint aStr = a1 >> P->GstrandBit;           
            a1 &= P->GstrandMask; //remove strand bit
            uint aRstart=PC[iP][PC_rStart];

            //convert to positive strand
            if (aDir==1 && aStr==0) {
                aStr=1;
                aRstart = Lread - (aLength+aRstart);                
            } else if (aDir==0 && aStr==1) {
                aRstart = Lread - (aLength+aRstart);                                
                a1 = P->nGenome - (aLength+a1);
            } else if (aDir==1 && aStr==1) {
                aStr=0;
                a1 = P->nGenome - (aLength+a1);         
            };
     
            //final strand            
            if (revertStrand) { //modified strand according to user input CHECK!!!!
                aStr=1-aStr;
            };             

            
            if (a1>=P->sjGstart) {//this is sj read
                uint a1D, aLengthD, a1A, aLengthA, isj1;              
                if (sjAlignSplit(a1, aLength, P, a1D, aLengthD, a1A, aLengthA, isj1)) {//align crosses the junction

                        assignAlignToWindow(a1D, aLengthD, aStr, aNrep, aFrag, aRstart, aAnchor, isj1);
                        assignAlignToWindow(a1A, aLengthA, aStr, aNrep, aFrag, aRstart+aLengthD, aAnchor, isj1);
                        
                    } else {//align does not cross the junction
                        continue; //do not check this align, continue to the next one
                    };
                    
                } else {//this is a normal genomic read
                    assignAlignToWindow(a1, aLength, aStr, aNrep, aFrag, aRstart, aAnchor, -1);
                };
        };
        
//         for (uint ii=0;ii<nW;ii++) {//check of some pieces created too many aligns in some windows, and remove those from WA (ie shift nWA indices
//             if (nWAP[ii]>P->seedNoneLociPerWindow) nWA[ii] -= nWAP[ii];
//         };
    };
    
    //TODO remove windows that have too many alignments
    //aligns are still sorted by original read coordinates, change direction for negative strand
    // DOES NOT HELP!!!
//     for ( uint iW=0;iW<nW;iW++ ) {
//         if (WA[iW][0][WA_rStart]>WA[iW][nWA[iW]-1][WA_rStart]) {//swap
//             for (uint iA=0;iA<nWA[iW]/2;iA++) {
//                 for (uint ii=0;ii<WA_SIZE;ii++) {
//                     uint dummy=WA[iW][iA][ii];
//                     WA[iW][iA][ii]=WA[iW][nWA[iW]-1-iA][ii];
//                     WA[iW][nWA[iW]-1-iA][ii]=dummy;
//                 };
//             };
//         };
//     };
    
#define PacBio    
#if defined PacBio
    if (P->swMode==1) {//stitching is done with Smith-Waterman against the windows
    uint swWinCovMax=0;
        for (uint iWin=0;iWin<nW;iWin++) {//check each window
            swWinCov[iWin]=0;
            if (nWA[iWin]>0) {
                //select good windows by coverage
                uint rLast=0;
                swWinGleft[iWin]=P->chrStart[P->nChrReal]; swWinGright[iWin]=0; swWinRleft[iWin]=0; swWinRright[iWin]=0;
                
                for (uint ia=0; ia<nWA[iWin]; ia++) {//calculate coverage from all aligns
                    uint L1=WA[iWin][ia][WA_Length];
                    uint r1=WA[iWin][ia][WA_rStart];
                    
                    //record ends
                    swWinRright[iWin]=max(swWinRright[iWin],r1+L1-1);
                    swWinGleft[iWin]=min(swWinGleft[iWin],WA[iWin][ia][WA_gStart]);
                    swWinGright[iWin]=max(swWinGright[iWin],WA[iWin][ia][WA_gStart]+L1-1);;

                    
                    if (r1+L1>rLast+1) {
                        if (r1>rLast) {
                            swWinCov[iWin] += L1;
                        } else {
                            swWinCov[iWin] += r1+L1-(rLast+1);
                        };
                        rLast=r1+L1-1;
                    };                    
                };//for (uint ia=0; ia<nWA[iWin]; ia++)
                
                swWinCovMax=max(swWinCovMax,swWinCov[iWin]);
            };//if (nWA[iWin]>0)
        };//for (uint iWin=0;iWin<nW;iWin++)
        
        //debug: read correct loci
        uint trStart,trEnd,trStr,trChr;
        char oneChar;
        istringstream stringStream1;
        stringStream1.str(readName);
        stringStream1 >> oneChar >> trChr >> oneChar >>trStart >> oneChar >> trEnd >> oneChar >>trStr;
        trStart += P->chrStart[trChr];
        trEnd   += P->chrStart[trChr];
        
        uint trNtotal=0, iW1=0;
        trBest = trNext = trInit; //initialize next/best
        for (uint iWin=0;iWin<nW;iWin++) {//check each window
                if (swWinCov[iWin]*100/swWinCovMax >= P->swWinCoverageMinP) {//S-W on all good windows, record the transcripts
                    //full S-W against the window
                    trA=*trInit; //that one is initialized
                    trA.Chr = WC[iWin][WC_Chr];
                    trA.Str = WC[iWin][WC_Str];
                    trA.roStr = revertStrand ? 1-trA.Str : trA.Str; //original strand of the read
                    trA.maxScore=0;

                    uint winLeft =swWinGleft[iWin] -5000;
                    uint winRight=swWinGright[iWin]+5000;
                    
                    //debug: process only correct windows
                    if (!( winLeft<trStart && winRight>trEnd && trA.Str==trStr) ) continue;
                    
                    intSWscore swScore=alignSmithWaterman(R[trA.roStr==0 ? 0:2],Lread,G+winLeft,winRight-winLeft,\
                            (intSWscore) 200, (intSWscore) 200, (intSWscore) 200, (intSWscore) 1, swT, P->swHsize, trA);
                    
                    trA.maxScore = (uint) swScore;
                    
                    trA.cStart  = trA.exons[0][EX_G] + winLeft - P->chrStart[trA.Chr];                     
                    trA.gLength = trA.exons[trA.nExons-1][EX_G]+1;
                    for (uint ii=0;ii<trA.nExons;ii++) {
                        trA.exons[ii][EX_G]+=winLeft;
                    };
                    
//                         uint gg=trA.exons[ii][EX_G]-(trA.exons[ii-1][EX_G]+trA.exons[ii-1][EX_L]);                        
//                         uint rg=trA.exons[ii][EX_R]-trA.exons[ii-1][EX_R]-trA.exons[ii-1][EX_L];
//                         if (gg>P->alignIntronMin) {
//                             trA.canonSJ[ii-1]=0; //sj
//                         } else if (gg>0) {
//                             trA.canonSJ[ii-1]=-1;//deletion
//                         };
//                         if (rg>0) trA.canonSJ[ii-1]=-2;                    
                    
                    trA.rLength=1;
                    trA.nMatch=1;

                    trAll[iW1]=trArrayPointer+trNtotal;
                    *(trAll[iW1][0])=trA;
                    
                    if (trAll[iW1][0]->maxScore > trBest->maxScore || (trAll[iW1][0]->maxScore == trBest->maxScore && trAll[iW1][0]->gLength < trBest->gLength ) ) {
                        trNext=trBest;
                        trBest=trAll[iW1][0];
                    };                    
                    
                    
                    nWinTr[iW1]=1;
                    trNtotal++;
                    iW1++;
                    //output all windows
                    P->inOut->logMain << iRead <<"\t"<< swWinCov[iWin]*100/Lread <<"\t"<< WC[iWin][WC_Str]<<"\t"<< WC[iWin][WC_gStart] \
                            <<"\t"<< WC[iWin][WC_gEnd] <<"\t"<< WA[iWin][0][WA_rStart] <<"\t"<< swWinRright[iWin] \
                            <<"\t"<<swWinGleft[iWin] <<"\t"<< swWinGright[iWin]<<"\t"<< swWinGright[iWin]-swWinGleft[iWin]<<"\t"<<Lread<<"\t"<<trA.maxScore<<endl;                
//                     outputTranscript(&trA, nW, &P->inOut->outBED);                    
                };            
        }; 
        nW=iW1;//number of windows with recorded transcripts
        return;
    };//if (P->swMode==1)
#endif //#if defined PacBio

#ifdef COMPILE_FOR_LONG_READS
uint swWinCovMax=0;
for (uint iW=0;iW<nW;iW++) {//check each window
    swWinCov[iW]=0;
    if (nWA[iW]>0) {
        //select good windows by coverage
        uint rLast=0;

        for (uint ia=0; ia<nWA[iW]; ia++) {//calculate coverage from all aligns
            uint L1=WA[iW][ia][WA_Length];
            uint r1=WA[iW][ia][WA_rStart];

            if (r1+L1>rLast+1) {
                if (r1>rLast) {
                    swWinCov[iW] += L1;
                } else {
                    swWinCov[iW] += r1+L1-(rLast+1);
                };
                rLast=r1+L1-1;
            };                    
        };//for (uint ia=0; ia<nWA[iW]; ia++)

        if (swWinCov[iW]>swWinCovMax) swWinCovMax=swWinCov[iW];
    };//if (nWA[iW]>0)
};//for (uint iW=0;iW<nW;iW++)
for (uint iW=0;iW<nW;iW++) {
    if (swWinCov[iW]<swWinCovMax*5/10) {//remove windows that are not good enough
        nWA[iW]=0;
    } else {//merge pieces that are adjacent in R- and G-spaces
        uint ia1=0;
        for (uint ia=1; ia<nWA[iW]; ia++) {
            if ( WA[iW][ia][WA_rStart] == (WA[iW][ia1][WA_rStart]+WA[iW][ia1][WA_Length]) \
              && WA[iW][ia][WA_gStart] == (WA[iW][ia1][WA_gStart]+WA[iW][ia1][WA_Length]) \
              && WA[iW][ia][WA_iFrag]  ==  WA[iW][ia1][WA_iFrag]     ) {//merge
                
                WA[iW][ia1][WA_Length] += WA[iW][ia][WA_Length];
                WA[iW][ia1][WA_Anchor]=max(WA[iW][ia1][WA_Anchor],WA[iW][ia][WA_Anchor]);
                //NOTE: I am not updating sjA and Nrep fields - this could cause trouble in some cases
                
            } else {//do not merge
                ia1++;
                if (ia1!=ia) {//move from ia to ia1
                    for (uint ii=0; ii<WA_SIZE; ii++) {
                        WA[iW][ia1][ii]=WA[iW][ia][ii];
                    };
                };
            };
        };
        nWA[iW]=ia1+1;
    };
};

//mapping time initialize
std::time(&timeStart);
#endif
    
    
    //generate transcript for each window, choose the best
    trInit->nWAmax=0;
    trBest = trNext = trInit; //initialize next/best
    uint iW1=0;//index of non-empty windows
    uint trNtotal=0; //total number of recorded transcripts
   
    for (uint iW=0; iW<nW; iW++) {//transcripts for all windows

        if (nWA[iW]==0) continue; //the window does not contain any aligns because it was merged with other windows        

//         {//debug
//             for (uint ii=0;ii<nWA[iW];ii++) {
//                         cout << iRead+1 <<"\t"<< WA[iW][ii][WA_Length] <<"\t"<< WA[iW][ii][WA_rStart] <<"\t"<< WA[iW][ii][WA_gStart] << "\n";
//             };
//             continue;
//             cout << nWA[iW]<<" "<<swWinCov[iW]*100/Lread <<"   "<<flush;
//     //         continue;
//         
//         };
        
        if (WlastAnchor[iW]<nWA[iW]) {
            WA[ iW ][ WlastAnchor[iW] ][ WA_Anchor]=2; //mark the last anchor
        };
        
        for (uint ii=0;ii<nWA[iW];ii++) WAincl[ii]=false; //initialize mask
        
        trInit->nWAmax=max(nWA[iW],trInit->nWAmax);        
        trA=*trInit; //that one is initialized
        trA.Chr = WC[iW][WC_Chr];
        trA.Str = WC[iW][WC_Str];
        trA.roStr = revertStrand ? 1-trA.Str : trA.Str; //original strand of the read
        trA.maxScore=0;
        
        trAll[iW1]=trArrayPointer+trNtotal;
        if (trNtotal+P->alignTranscriptsPerWindowNmax > P->alignTranscriptsPerReadNmax) {
            P->inOut->logMain << "WARNING: not enough space allocated for transcript. Did not process all windows for read "<< readName+1 <<endl;
            P->inOut->logMain <<"   SOLUTION: increase alignTranscriptsPerReadNmax and re-run\n" << flush;
            break;
        };
        *(trAll[iW1][0])=trA;
        nWinTr[iW1]=0; //initialize number of transcripts per window
        
        
    #ifdef COMPILE_FOR_LONG_READS
        stitchWindowSeeds(iW, iW1, R[trA.roStr==0 ? 0:2], Q[trA.roStr], G);
    #else
        stitchWindowAligns(0, nWA[iW], 0, WAincl, 0, 0, trA, Lread, WA[iW], R[trA.roStr==0 ? 0:2], Q[trA.roStr], G, sigG, P, trAll[iW1], nWinTr+iW1, this);
    #endif
        trAll[iW1][0]->nextTrScore= nWinTr[iW1]==1 ? 0 : trAll[iW1][1]->maxScore;        
        
        if (trAll[iW1][0]->maxScore > trBest->maxScore || (trAll[iW1][0]->maxScore == trBest->maxScore && trAll[iW1][0]->gLength < trBest->gLength ) ) {
            trNext=trBest;
            trBest=trAll[iW1][0];
        };

        trNtotal += nWinTr[iW1];        
        iW1++;
    };
    
    nW=iW1;//only count windows that had alignments
    
//     {//debug
//         std::time(&timeFinish);
//         double timeDiff=difftime(timeFinish,timeStart);
//         cout << "     "<< timeDiff << "     "<<trBest->maxScore*100/Lread<<"   "<<iRead<<endl;;
//     };
    
    if (trBest->maxScore==0) {//no window was aligned (could happen if for all windows too many reads are multiples)
        mapMarker = MARKER_NO_GOOD_WINDOW;
        nW=0;
        nTr=0;
        return;
    };
            
    nextWinScore=trNext->maxScore;
   
};//end of function

