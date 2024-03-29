/* Copyright (C) 2020-2021 GSI Helmholtzzentrum fuer Schwerionenforschung, Darmstadt
   SPDX-License-Identifier: GPL-3.0-only
   Authors: Volker Friese [committer], Dominik Smith */

/** @file run_reco.C
 ** @author Volker Friese <v.friese@gsi.de>
 ** @since 14 November 2020
 **/


// --- Includes needed for IDE
#include <RtypesCore.h>
#if !defined(__CLING__)
#include "CbmBuildEventsFromTracksReal.h"
#include "CbmBuildEventsIdeal.h"
#include "CbmBuildEventsQa.h"
#include "CbmDefs.h"
#include "CbmFindPrimaryVertex.h"
#include "CbmKF.h"
#include "CbmL1.h"
#include "CbmL1StsTrackFinder.h"
#include "CbmLitFindGlobalTracks.h"
#include "CbmMCDataManager.h"
#include "CbmMatchRecoToMC.h"
#include "CbmMuchFindHitsGem.h"
#include "CbmMvdClusterfinder.h"
#include "CbmMvdHitfinder.h"
#include "CbmPVFinderKF.h"
#include "CbmPrimaryVertexFinder.h"
#include "CbmPsdHitProducer.h"
#include "CbmRecoSts.h"
#include "CbmRichHitProducer.h"
#include "CbmRichReconstruction.h"
#include "CbmSetup.h"
#include "CbmStsFindTracks.h"
#include "CbmStsFindTracksEvents.h"
#include "CbmStsTrackFinder.h"
#include "CbmTaskBuildRawEvents.h"
#include "CbmTofSimpClusterizer.h"
#include "CbmTrdClusterFinder.h"
#include "CbmTrdHitProducer.h"

#include <FairFileSource.h>
#include <FairMonitor.h>
#include <FairParAsciiFileIo.h>
#include <FairParRootFileIo.h>
#include <FairRunAna.h>
#include <FairRuntimeDb.h>
#include <FairSystemInfo.h>

#include <TStopwatch.h>
#endif


/** @brief Macro for CBM reconstruction
 ** @author Volker Friese <v.friese@gsi.de>
 ** @since  14 November 2020
 ** @param input          Name of input file (w/o extension .raw.root)
 ** @param nTimeSlices    Number of time-slices to process
 ** @param firstTimeSlice First time-slice (entry) to be processed
 ** @param output         Name of output file (w/o extension .rec.root)
 ** @param sEvBuildRaw    Option for raw event building
 ** @param traFile        Transport ROOT file (w/o extension .tra.root)
 ** @param debugWithMC          Option to provide the trackfinder with MC information
 **
 ** This macro performs from the digis in a time-slice. It can be used
 ** for simulated data (result of run_digi.C) or real data after unpacking.
 **
 ** The macro covers both time-based reconstruction and event-based
 ** reconstruction using raw events build from digis. This can be selected
 ** by the forth argument. If left empty, no raw event builder will be
 ** employed and reconstruction will be time-based. The option "Ideal"
 ** selects the ideal raw event builder, which associates digis to events
 ** based on the MC truth. The option "Real" selects a real raw event builder
 ** (latest version, for older versions use "Real2018" or "Real2019").
 ** 
 **
 ** The file names must be specified without extensions. The convention is
 ** that the raw (input) file is [input].raw.root. The output file
 ** will be [input].rec.root if not specified by the user. The parameter file
 ** has the extension .par.root. It is assumed to be [input].par.root if
 ** not specified by the user.
 **
 ** If no argument is specified, the input will be set to "test". This allows
 ** to execute the macro chain (run_tra_file.C, run_digi.C and run_reco.C)
 ** from the ROOT prompt without user intervention.
 **
 **/

// -----   Check output file name   -------------------------------------------
bool CheckOutFileName(TString fileName, Bool_t overwrite)
{
  string fName = "run_reco_json_config";
  // --- Protect against overwriting an existing file
  if ((!gSystem->AccessPathName(fileName.Data())) && (!overwrite)) {
    cout << fName << ": output file " << fileName << " already exists!";
    return false;
  }

  // --- If the directory does not yet exist, create it
  const char* directory = gSystem->DirName(fileName.Data());
  if (gSystem->AccessPathName(directory)) {
    Int_t success = gSystem->mkdir(directory, kTRUE);
    if (success == -1) {
      cout << fName << ": output directory " << directory << " does not exist and cannot be created!";
      return false;
    }
    else
      cout << fName << ": created directory " << directory;
  }
  return true;
}

// --------------------------------------------------------------------------
void run_reco_json_config(TString input = "", Int_t nTimeSlices = -1, Int_t firstTimeSlice = 0, TString output = "", bool overwrite = false,
              TString sEvBuildRaw = "", std::string config = "", TString traFile = "", Bool_t debugWithMC = false)
{

  // ========================================================================
  //          Adjust this part according to your requirements

  // --- Logger settings ----------------------------------------------------
  TString logLevel     = "INFO";
  TString logVerbosity = "LOW";
  // ------------------------------------------------------------------------

  // -----   Environment   --------------------------------------------------
  TString myName = "run_reco";                     // this macro's name for screen output
  TString srcDir = gSystem->Getenv("VMCWORKDIR");  // top source directory
  // ------------------------------------------------------------------------


  // -----   In- and output file names   ------------------------------------
  if (input.IsNull()) input = "test";
  TString rawFile = input + ".raw.root";
  TString parFile = input + ".par.root";
  if (output.IsNull()) output = input;
  TString outFile = output + ".reco.root";
  TString monFile = output + ".moni_reco.root";
  if (traFile.IsNull()) traFile = input;
  traFile+=".tra.root";
  if(!CheckOutFileName(outFile, overwrite)) return;
  std::cout << "Inputfile " << rawFile << std::endl;
  std::cout << "Outfile " << outFile << std::endl;
  std::cout << "Parfile " << parFile << std::endl;

  // -----   Load the geometry setup   -------------------------------------
  CbmSetup* geo = CbmSetup::Instance();
  if (config == "") config = Form("%s/macro/run/config.json", gSystem->Getenv("VMCWORKDIR"));
  boost::property_tree::ptree pt;
  CbmTransportConfig::LoadFromFile(config, pt);
  CbmTransportConfig::SetGeometry(geo, pt.get_child(CbmTransportConfig::GetModuleTag()));
  // ------------------------------------------------------------------------


  // -----   Some global switches   -----------------------------------------
  Bool_t eventBased = !sEvBuildRaw.IsNull();
  Bool_t useMvd     = geo->IsActive(ECbmModuleId::kMvd);
  Bool_t useSts     = geo->IsActive(ECbmModuleId::kSts);
  Bool_t useRich    = geo->IsActive(ECbmModuleId::kRich);
  Bool_t useMuch    = geo->IsActive(ECbmModuleId::kMuch);
  Bool_t useTrd     = geo->IsActive(ECbmModuleId::kTrd);
  Bool_t useTof     = geo->IsActive(ECbmModuleId::kTof);
  Bool_t usePsd     = geo->IsActive(ECbmModuleId::kPsd);
  // ------------------------------------------------------------------------


  // -----   Parameter files as input to the runtime database   -------------
  std::cout << std::endl;
  std::cout << "-I- " << myName << ": Defining parameter files " << std::endl;
  TList* parFileList = new TList();
  TString geoTag;

  // - TRD digitisation parameters
  if (CbmSetup::Instance()->GetGeoTag(ECbmModuleId::kTrd, geoTag)) {
    const Char_t* npar[4] = {"asic", "digi", "gas", "gain"};
    TObjString* trdParFile(NULL);
    for (Int_t i(0); i < 4; i++) {
      trdParFile = new TObjString(srcDir + "/parameters/trd/trd_" + geoTag + "." + npar[i] + ".par");
      parFileList->Add(trdParFile);
      std::cout << "-I- " << myName << ": Using parameter file " << trdParFile->GetString() << std::endl;
    }
  }

  // - TOF digitisation parameters
  if (CbmSetup::Instance()->GetGeoTag(ECbmModuleId::kTof, geoTag)) {
    TObjString* tofBdfFile = new TObjString(srcDir + "/parameters/tof/tof_" + geoTag + ".digibdf.par");
    parFileList->Add(tofBdfFile);
    std::cout << "-I- " << myName << ": Using parameter file " << tofBdfFile->GetString() << std::endl;
  }
  // ------------------------------------------------------------------------

  // In general, the following parts need not be touched
  // ========================================================================


  // -----   Timer   --------------------------------------------------------
  TStopwatch timer;
  timer.Start();
  // ------------------------------------------------------------------------


  // -----   FairRunAna   ---------------------------------------------------
  FairRunAna* run             = new FairRunAna();
  FairFileSource* inputSource = new FairFileSource(rawFile);
  if (debugWithMC) { inputSource->AddFriend(traFile); }
  run->SetSource(inputSource);
  run->SetOutputFile(outFile);
  run->SetGenerateRunInfo(kTRUE);
  FairMonitor::GetMonitor()->EnableMonitor(kTRUE, monFile);
  // ------------------------------------------------------------------------

  // -----   MCDataManager  -----------------------------------
  if (debugWithMC) {
    CbmMCDataManager* mcManager = new CbmMCDataManager("MCDataManager", 0);
    mcManager->AddFile(traFile);
    run->AddTask(mcManager);
  }
  // ------------------------------------------------------------------------

  // -----   Logger settings   ----------------------------------------------
  FairLogger::GetLogger()->SetLogScreenLevel(logLevel.Data());
  FairLogger::GetLogger()->SetLogVerbosityLevel(logVerbosity.Data());
  // ------------------------------------------------------------------------


  // -----   Raw event building from digis   --------------------------------
  if (eventBased) {
    if (sEvBuildRaw.EqualTo("Ideal", TString::ECaseCompare::kIgnoreCase)) {
      FairTask* evBuildRaw = new CbmBuildEventsIdeal();
      run->AddTask(evBuildRaw);
      std::cout << "-I- " << myName << ": Added task " << evBuildRaw->GetName() << std::endl;
      eventBased = kTRUE;
    }  //? Ideal raw event building
    else if (sEvBuildRaw.EqualTo("Real", TString::ECaseCompare::kIgnoreCase)) {
      CbmTaskBuildRawEvents* evBuildRaw = new CbmTaskBuildRawEvents();

      //Choose between NoOverlap, MergeOverlap, AllowOverlap
      evBuildRaw->SetEventOverlapMode(EOverlapModeRaw::NoOverlap);

      // Remove detectors where digis not found
      if (!useRich) evBuildRaw->RemoveDetector(kRawEventBuilderDetRich);
      if (!useMuch) evBuildRaw->RemoveDetector(kRawEventBuilderDetMuch);
      if (!usePsd) evBuildRaw->RemoveDetector(kRawEventBuilderDetPsd);
      if (!useTof) evBuildRaw->RemoveDetector(kRawEventBuilderDetTof);
      if (!useTrd) evBuildRaw->RemoveDetector(kRawEventBuilderDetTrd);
      if (!useSts) {
        std::cerr << "-E- " << myName << ": Sts must be present for raw event "
                  << "building using ``Real2019'' option. Terminating macro." << std::endl;
        return;
      }
      // Set STS as reference detector
      evBuildRaw->SetReferenceDetector(kRawEventBuilderDetSts);

      // Use sliding window seed builder with STS
      //evBuildRaw->SetReferenceDetector(kRawEventBuilderDetUndef);
      //evBuildRaw->AddSeedTimeFillerToList(kRawEventBuilderDetSts);
      //evBuildRaw->SetSlidingWindowSeedFinder(1000, 500, 500);
      //evBuildRaw->SetSeedFinderQa(true);  // optional QA information for seed finder

      evBuildRaw->SetTsParameters(0.0, 1.e7, 0.0); //Double_t dTsStartTime, Double_t dTsLength, Double_t dTsOverLength

      // Use CbmMuchDigi instead of CbmMuchBeamtimeDigi
      evBuildRaw->ChangeMuchBeamtimeDigiFlag(kFALSE);

      evBuildRaw->SetTriggerMinNumber(ECbmModuleId::kSts, 1);
      evBuildRaw->SetTriggerMaxNumber(ECbmModuleId::kSts, -1);
      evBuildRaw->SetTriggerWindow(ECbmModuleId::kSts,-1000., 1.e5);

      run->AddTask(evBuildRaw);
      std::cout << "-I- " << myName << ": Added task " << evBuildRaw->GetName() << std::endl;
      eventBased = kTRUE;
    }  //? Real raw event building
    else {
      std::cerr << "-E- " << myName << ": Unknown option " << sEvBuildRaw
                << " for raw event building! Terminating macro execution." << std::endl;
      return;
    }
  }  //? event-based reco
  // ------------------------------------------------------------------------


  // ----------- QA for raw event builder -----------------------------------
  if (eventBased && debugWithMC) {
    CbmBuildEventsQa* evBuildQA = new CbmBuildEventsQa();
    run->AddTask(evBuildQA);
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in MVD   ----------------------------------
  if (useMvd) {

    CbmMvdClusterfinder* mvdCluster = new CbmMvdClusterfinder("MVD Cluster Finder", 0, 0);
    run->AddTask(mvdCluster);
    std::cout << "-I- : Added task " << mvdCluster->GetName() << std::endl;

    CbmMvdHitfinder* mvdHit = new CbmMvdHitfinder("MVD Hit Finder", 0, 0);
    mvdHit->UseClusterfinder(kTRUE);
    run->AddTask(mvdHit);
    std::cout << "-I- " << myName << ": Added task " << mvdHit->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in STS   ----------------------------------
  if (useSts) {
    CbmRecoSts* stsReco = new CbmRecoSts(ECbmRecoMode::kCbmRecoTimeslice);
    if (eventBased) stsReco->SetMode(ECbmRecoMode::kCbmRecoEvent);
    run->AddTask(stsReco);
    std::cout << "-I- " << myName << ": Added task " << stsReco->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in RICH   ---------------------------------
  if (useRich) {
    CbmRichHitProducer* richHitProd = new CbmRichHitProducer();
    run->AddTask(richHitProd);
    std::cout << "-I- " << myName << ": Added task " << richHitProd->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in MUCH   ---------------------------------
  if (useMuch) {

    // --- Parameter file name
    TString geoTag;
    geo->GetGeoTag(ECbmModuleId::kMuch, geoTag);
    Int_t muchFlag  = (geoTag.Contains("mcbm") ? 1 : 0);
    TString parFile = gSystem->Getenv("VMCWORKDIR");
    parFile += "/parameters/much/much_" + geoTag(0, 4) + "_digi_sector.root";
    std::cout << "Using parameter file " << parFile << std::endl;

    // --- Hit finder for GEMs
    FairTask* muchReco = new CbmMuchFindHitsGem(parFile.Data(), muchFlag);
    run->AddTask(muchReco);
    std::cout << "-I- " << myName << ": Added task " << muchReco->GetName() << " with parameter file " << parFile
              << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in TRD   ----------------------------------
  if (useTrd) {

    Double_t triggerThreshold       = 0.5e-6;  // SIS100
    CbmTrdClusterFinder* trdCluster = new CbmTrdClusterFinder();
    if (eventBased) trdCluster->SetTimeBased(kFALSE);
    else
      trdCluster->SetTimeBased(kTRUE);
    trdCluster->SetNeighbourEnable(true, false);
    trdCluster->SetMinimumChargeTH(triggerThreshold);
    trdCluster->SetRowMerger(true);

    // Uncomment if you want to use all available digis.
    // In that case clusters hits will not be added to the CbmEvent
    // trdCluster->SetUseOnlyEventDigis(kFALSE);

    run->AddTask(trdCluster);
    std::cout << "-I- " << myName << ": Added task " << trdCluster->GetName() << std::endl;

    CbmTrdHitProducer* trdHit = new CbmTrdHitProducer();
    run->AddTask(trdHit);
    std::cout << "-I- " << myName << ": Added task " << trdHit->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in TOF   ----------------------------------
  if (useTof) {
    CbmTofSimpClusterizer* tofCluster = new CbmTofSimpClusterizer("TofSimpClusterizer", 0);
    tofCluster->SetOutputBranchPersistent("TofHit", kTRUE);
    tofCluster->SetOutputBranchPersistent("TofDigiMatch", kTRUE);
    run->AddTask(tofCluster);
    std::cout << "-I- " << myName << ": Added task " << tofCluster->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------


  // -----   Local reconstruction in PSD   ----------------------------------
  if (usePsd) {
    CbmPsdHitProducer* psdHit = new CbmPsdHitProducer();
    run->AddTask(psdHit);
    std::cout << "-I- " << myName << ": Added task " << psdHit->GetName() << std::endl;
  }
  // ------------------------------------------------------------------------
  if (debugWithMC) {
    CbmMatchRecoToMC* match1 = new CbmMatchRecoToMC();
    run->AddTask(match1);
  }

  // -----   Track finding in STS (+ MVD)    --------------------------------
  if (useMvd || useSts) {
    CbmKF* kalman = new CbmKF();
    run->AddTask(kalman);
    CbmL1* l1 = 0;
    if (debugWithMC) { l1 = new CbmL1("L1", 2, 3); }
    else {
      l1 = new CbmL1("L1", 0);
    }

    // --- Material budget file names
    TString mvdGeoTag;
    if (geo->GetGeoTag(ECbmModuleId::kMvd, mvdGeoTag)) {
      TString parFile = gSystem->Getenv("VMCWORKDIR");
      parFile += "/parameters/mvd/mvd_matbudget_" + mvdGeoTag + ".root";
      std::cout << "Using material budget file " << parFile << std::endl;
      l1->SetMvdMaterialBudgetFileName(parFile.Data());
    }
    TString stsGeoTag;
    if (geo->GetGeoTag(ECbmModuleId::kSts, stsGeoTag)) {
      TString parFile = gSystem->Getenv("VMCWORKDIR");
      parFile += "/parameters/sts/sts_matbudget_" + stsGeoTag + ".root";
      std::cout << "Using material budget file " << parFile << std::endl;
      l1->SetStsMaterialBudgetFileName(parFile.Data());
    }
    run->AddTask(l1);
    std::cout << "-I- " << myName << ": Added task " << l1->GetName() << std::endl;

    CbmStsTrackFinder* stsTrackFinder = new CbmL1StsTrackFinder();
    if (eventBased) {
      FairTask* stsFindTracks = new CbmStsFindTracksEvents(stsTrackFinder, useMvd);
      run->AddTask(stsFindTracks);
      std::cout << "-I- " << myName << ": Added task " << stsFindTracks->GetName() << std::endl;
    }
    else {
      FairTask* stsFindTracks = new CbmStsFindTracks(0, stsTrackFinder, useMvd);
      run->AddTask(stsFindTracks);
      std::cout << "-I- " << myName << ": Added task " << stsFindTracks->GetName() << std::endl;
    }
  }
  // ------------------------------------------------------------------------


  // ==== From here on, the time-based and the event-based reconstruction
  // ==== chains differ, since time-based version of primary vertex finding
  // ==== and global tracking are not yet available. For time-based
  // ==== reconstruction, a track-based event finder is used; no global
  // ==== tracks are produced.

  if (eventBased) {

    // -----   Primary vertex finding   -------------------------------------
    CbmPrimaryVertexFinder* pvFinder = new CbmPVFinderKF();
    CbmFindPrimaryVertex* findVertex = new CbmFindPrimaryVertex(pvFinder);
    run->AddTask(findVertex);
    std::cout << "-I- " << myName << ": Added task " << findVertex->GetName() << std::endl;
    // ----------------------------------------------------------------------


    // ---   Global track finding   -----------------------------------------
    CbmLitFindGlobalTracks* finder = new CbmLitFindGlobalTracks();
    finder->SetTrackingType("branch");
    finder->SetMergerType("nearest_hit");
    run->AddTask(finder);
    std::cout << "-I- : Added task " << finder->GetName() << std::endl;
    // ----------------------------------------------------------------------

    // ---   Particle Id in TRD   -----------------------------------------
    if (useTrd) {
      CbmTrdSetTracksPidLike* trdLI = new CbmTrdSetTracksPidLike("TRDLikelihood", "TRDLikelihood");
      trdLI->SetUseMCInfo(kTRUE);
      trdLI->SetUseMomDependence(kTRUE);
      run->AddTask(trdLI);
      std::cout << "-I- : Added task " << trdLI->GetName() << std::endl;
    }
    // ------------------------------------------------------------------------


    // -----   RICH reconstruction   ----------------------------------------
    if (useRich) {
      CbmRichReconstruction* richReco = new CbmRichReconstruction();
      run->AddTask(richReco);
      std::cout << "-I- : Added task " << richReco->GetName() << std::endl;
    }
    // ----------------------------------------------------------------------

  }  //? event-based reco

  else {

    // -----   Event building from STS tracks   -----------------------------
    run->AddTask(new CbmBuildEventsFromTracksReal());
    // ----------------------------------------------------------------------

  }  //? time-based reco


  // -----  Parameter database   --------------------------------------------
  std::cout << std::endl << std::endl;
  std::cout << "-I- " << myName << ": Set runtime DB" << std::endl;
  FairRuntimeDb* rtdb        = run->GetRuntimeDb();
  FairParRootFileIo* parIo1  = new FairParRootFileIo();
  FairParAsciiFileIo* parIo2 = new FairParAsciiFileIo();
  parIo1->open(parFile.Data(), "UPDATE");
  rtdb->setFirstInput(parIo1);
  if (!parFileList->IsEmpty()) {
    parIo2->open(parFileList, "in");
    rtdb->setSecondInput(parIo2);
  }
  // ------------------------------------------------------------------------


  // -----   Run initialisation   -------------------------------------------
  std::cout << std::endl;
  std::cout << "-I- " << myName << ": Initialise run" << std::endl;
  run->Init();
  rtdb->setOutput(parIo1);
  rtdb->saveOutput();
  rtdb->print();
  // ------------------------------------------------------------------------


  // -----   Register light ions (d, t, He3, He4)   -------------------------
  std::cout << std::endl;
  TString registerLightIonsMacro = gSystem->Getenv("VMCWORKDIR");
  registerLightIonsMacro += "/macro/KF/registerLightIons.C";
  std::cout << "Loading macro " << registerLightIonsMacro << std::endl;
  gROOT->LoadMacro(registerLightIonsMacro);
  gROOT->ProcessLine("registerLightIons()");
  // ------------------------------------------------------------------------

  // -----   Start run   ----------------------------------------------------
  std::cout << std::endl << std::endl;
  std::cout << "-I- " << myName << ": Starting run" << std::endl;
  run->Run(firstTimeSlice, nTimeSlices);
  // ------------------------------------------------------------------------


  // -----   Finish   -------------------------------------------------------
  timer.Stop();
  FairMonitor::GetMonitor()->Print();
  Double_t rtime = timer.RealTime();
  Double_t ctime = timer.CpuTime();
  std::cout << std::endl << std::endl;
  std::cout << "Macro finished successfully." << std::endl;
  std::cout << "Output file is    " << outFile << std::endl;
  std::cout << "Parameter file is " << parFile << std::endl;
  std::cout << "Real time " << rtime << " s, CPU time " << ctime << " s" << std::endl;
  FairSystemInfo sysInfo;
  Float_t maxMemory = sysInfo.GetMaxMemory();
  std::cout << "<DartMeasurement name=\"MaxMemory\" type=\"numeric/double\">";
  std::cout << maxMemory;
  std::cout << "</DartMeasurement>" << std::endl;
  Float_t cpuUsage = ctime / rtime;
  std::cout << "<DartMeasurement name=\"CpuLoad\" type=\"numeric/double\">";
  std::cout << cpuUsage;
  std::cout << "</DartMeasurement>" << std::endl;
  // ------------------------------------------------------------------------


  // -----   This is to prevent a malloc error when exiting ROOT   ----------
  // The source of the error is unknown. Related to TGeoManager.
  RemoveGeoManager();
  // ------------------------------------------------------------------------

}  // End of main macro function
