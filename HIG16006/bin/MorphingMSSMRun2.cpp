#include <string>
#include <map>
#include <set>
#include <iostream>
#include <utility>
#include <vector>
#include <cstdlib>
#include "boost/algorithm/string/predicate.hpp"
#include "boost/program_options.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/regex.hpp"
#include "CombineHarvester/CombineTools/interface/CombineHarvester.h"
#include "CombineHarvester/CombineTools/interface/Observation.h"
#include "CombineHarvester/CombineTools/interface/Process.h"
#include "CombineHarvester/CombineTools/interface/Utilities.h"
#include "CombineHarvester/CombineTools/interface/Systematics.h"
#include "CombineHarvester/CombineTools/interface/BinByBin.h"
#include "CombineHarvester/CombineTools/interface/AutoRebin.h"
#include "CombineHarvester/CombinePdfs/interface/MorphFunctions.h"
#include "CombineHarvester/CombineTools/interface/HttSystematics.h"
#include "RooWorkspace.h"
#include "RooRealVar.h"
#include "TH2.h"

using namespace std;
using boost::starts_with;
namespace po = boost::program_options;

template <typename T>
void To1Bin(T* proc)
{
    TH1F *hist = new TH1F("hist","hist",1,0,1);
    hist->SetDirectory(0);
    hist->SetBinContent(1,proc->ClonedScaledShape()->Integral(0,proc->ClonedScaledShape()->GetNbinsX()+1));
    proc->set_shape(*hist,true);
}


int main(int argc, char** argv) {
  // First define the location of the "auxiliaries" directory where we can
  // source the input files containing the datacard shapes
  string SM125= "";
  string mass = "mA";
  string output_folder = "mssm_run2";
  string input_folder="Imperial/datacards-76X/";
  string postfix="";
  bool auto_rebin = false;
  bool manual_rebin = false;
  int control_region = 0;
  po::variables_map vm;
  po::options_description config("configuration");
  config.add_options()
    ("mass,m", po::value<string>(&mass)->default_value(mass))
    ("input_folder", po::value<string>(&input_folder)->default_value("Imperial/datacards-76X"))
    ("postfix", po::value<string>(&postfix)->default_value(""))
    ("auto_rebin", po::value<bool>(&auto_rebin)->default_value(false))
    ("manual_rebin", po::value<bool>(&manual_rebin)->default_value(false))
    ("output_folder", po::value<string>(&output_folder)->default_value("mssm_run2"))
    ("SM125,h", po::value<string>(&SM125)->default_value(SM125))
    ("control_region", po::value<int>(&control_region)->default_value(0));
  po::store(po::command_line_parser(argc, argv).options(config).run(), vm);
  po::notify(vm);
  
  typedef vector<string> VString;
  typedef vector<pair<int, string>> Categories;
  string input_dir =
      //string(getenv("CMSSW_BASE")) + "/src/auxiliaries/shapes/Imperial/";
      //"./datacards-2501/";
      "./shapes/"+input_folder+"/";

  VString chns =
      //{"tt"};
   //   {"mt"};
      //{"mt","et","tt","em"};
      {"mt","et"};

  RooRealVar mA(mass.c_str(), mass.c_str(), 90., 3200.);
  RooRealVar mH("mH", "mH", 90., 3200.);
  RooRealVar mh("mh", "mh", 90., 3200.);

  map<string, VString> bkg_procs;
  bkg_procs["et"] = {"W", "QCD", "ZL", "ZJ", "TT", "VV","ZTT"};
  bkg_procs["mt"] = {"W", "QCD", "ZL", "ZJ", "TT", "VV","ZTT"};
  bkg_procs["tt"] = {"W", "QCD", "ZL", "ZJ", "TT", "VV","ZTT"};
  bkg_procs["em"] = {"W", "QCD", "ZLL", "TT", "VV", "ZTT"};

  //Example - could fill this map with hardcoded binning for different
  //categories if manual_rebin is turned on
  map<string, vector<double> > binning;
  binning["et_nobtag"] = {500, 700, 900, 3900};
  binning["et_btag"] = {500,3900};
  binning["mt_nobtag"] = {500,700,900,1300,1700,1900,3900};
  binning["mt_btag"] = {500,1300,3900};
  binning["tt_nobtag"] = {500,3900};
  binning["tt_btag"] = {500,3900};
  binning["em_nobtag"] = {500,3900};
  binning["em_btag"] = {500,3900};

  // Create an empty CombineHarvester instance that will hold all of the
  // datacard configuration and histograms etc.
  ch::CombineHarvester cb;
  // Uncomment this next line to see a *lot* of debug information
  // cb.SetVerbosity(3);
 
  // Here we will just define two categories for an 8TeV analysis. Each entry in
  // the vector below specifies a bin name and corresponding bin_id.
  //
  map<string,Categories> cats;
  cats["et_13TeV"] = {
    {8, "et_nobtag"},
    {9, "et_btag"}
    };

  cats["em_13TeV"] = {
    {8, "em_nobtag"},
    {9, "em_btag"}
    };

  cats["tt_13TeV"] = {
    {8, "tt_nobtag"},
    {9, "tt_btag"}
    };
  
  cats["mt_13TeV"] = {
    {8, "mt_nobtag"},
    {9, "mt_btag"}
    };

  if (control_region > 0){
      // for each channel use the categories >= 10 for the control regions
      // the control regions are ordered in triples (10,11,12),(13,14,15)...
      for (auto chn:chns){
          // for em do nothing
          if ( chn == "em") continue;
          Categories queue;
          int binid = 10;
          for (auto cat:cats[chn+"_13TeV"]){
            queue.push_back(make_pair(binid,cat.second+"_wjetscr"));
            queue.push_back(make_pair(binid+1,cat.second+"_qcdcr"));
            queue.push_back(make_pair(binid+2,cat.second+"_wjetscr_ss"));
            binid += 3;
          }
          cats[chn+"_13TeV"].insert(cats[chn+"_13TeV"].end(),queue.begin(),queue.end());
      }
  }

  vector<string> masses = {"90","100","110","120","130","140","160","180", "250", "300", "450", "500", "600", "700", "900","1000","1200","1500","1600","1800","2000","2600","2900","3200"}; //Not all mass points available for fall15
  //vector<string> masses = {"90","100","110","120","130","140","160","180", "200", "250", "300", "350", "400", "450", "500", "600", "700", "800", "900","1000","1200","1400","1500","1600","1800","2000","2300","2600","2900","3200"};

  map<string, VString> signal_types = {
    {"ggH", {"ggh_htautau", "ggH_Htautau", "ggA_Atautau"}},
    {"bbH", {"bbh_htautau", "bbH_Htautau", "bbA_Atautau"}}
  };
  if(mass=="MH"){
    signal_types = {
      {"ggH", {"ggH"}},
      {"bbH", {"bbH"}}
    };
  }
    vector<string> sig_procs = {"ggH","bbH"};
  for(auto chn : chns){
    cb.AddObservations({"*"}, {"htt"}, {"13TeV"}, {chn}, cats[chn+"_13TeV"]);

    cb.AddProcesses({"*"}, {"htt"}, {"13TeV"}, {chn}, bkg_procs[chn], cats[chn+"_13TeV"], false);
  
    cb.AddProcesses(masses, {"htt"}, {"13TeV"}, {chn}, signal_types["ggH"], cats[chn+"_13TeV"], true);
    cb.AddProcesses(masses, {"htt"}, {"13TeV"}, {chn}, signal_types["bbH"], cats[chn+"_13TeV"], true);
    }
  if (control_region > 0){
      // filter QCD in W+jets control regions
      // filter signal processes in control regions
      cb.FilterAll([](ch::Object const* obj) {
              return (((obj->bin().find("wjetscr") != std::string::npos) && (obj->process() == "QCD")) || (boost::regex_search(obj->bin(),boost::regex{"(wjets|qcd)cr"}) && obj->signal()));
              });
  } 

  ch::AddMSSMRun2Systematics(cb,control_region);
  //! [part7]
  for (string chn:chns){
    cb.cp().channel({chn}).backgrounds().ExtractShapes(
        input_dir + "htt_"+chn+".inputs-mssm-13TeV"+postfix+".root",
        "$BIN/$PROCESS",
        "$BIN/$PROCESS_$SYSTEMATIC");
    cb.cp().channel({chn}).process(signal_types["ggH"]).ExtractShapes(
        input_dir + "htt_"+chn+".inputs-mssm-13TeV"+postfix+".root",
        "$BIN/ggH$MASS",
        "$BIN/ggH$MASS_$SYSTEMATIC");
    cb.cp().channel({chn}).process(signal_types["bbH"]).ExtractShapes(
        input_dir + "htt_"+chn+".inputs-mssm-13TeV"+postfix+".root",
        "$BIN/bbH$MASS",
        "$BIN/bbH$MASS_$SYSTEMATIC");
   }
   //Replacing observation with the sum of the backgrounds (asimov) - nice to ensure blinding 
    auto bins = cb.cp().bin_set();
    for (auto b : bins) {
        cb.cp().bin({b}).ForEachObs([&](ch::Observation *obs) {
        obs->set_shape(cb.cp().bin({b}).backgrounds().GetShape(), true);
        });
    } 
    cb.cp().FilterAll([](ch::Object const* obj) { return ! (boost::regex_search(obj->bin(),boost::regex{"(wjets|qcd)cr"}));}).ForEachProc(To1Bin<ch::Process>);
    cb.cp().FilterAll([](ch::Object const* obj) { return ! (boost::regex_search(obj->bin(),boost::regex{"(wjets|qcd)cr"}));}).ForEachObs(To1Bin<ch::Observation>);
  

  auto rebin = ch::AutoRebin()
       .SetBinThreshold(0.)
   //    .SetBinUncertFraction(0.05)
       .SetRebinMode(1)
       .SetPerformRebin(true)
       .SetVerbosity(1);
  if(auto_rebin) rebin.Rebin(cb, cb);

  if(manual_rebin) {
      for(auto b : bins) {
        std::cout << "Rebinning by hand for bin: " << b <<  std::endl;
        cb.cp().bin({b}).VariableRebin(binning[b]);    
      }
  }
  
  cout << "Generating bbb uncertainties...";
  auto bbb = ch::BinByBinFactory()
    .SetAddThreshold(0.)
    .SetMergeThreshold(0.4)
    .SetFixNorm(true);
  bbb.MergeAndAdd(cb.cp().process({"ZTT", "QCD", "W", "ZJ", "ZL", "TT", "VV", "Ztt", "ttbar", "EWK", "Fakes", "ZMM", "TTJ", "WJets", "Dibosons"}).FilterAll([](ch::Object const* obj) {
              return (boost::regex_search(obj->bin(),boost::regex{"(wjets|qcd)cr"}));
              }), cb);
  cout << " done\n";

  //Switch JES over to lnN:
  //cb.cp().syst_name({"CMS_scale_j_13TeV"}).ForEachSyst([](ch::Systematic *sys) { sys->set_type("lnN");})};

  // This function modifies every entry to have a standardised bin name of
  // the form: {analysis}_{channel}_{bin_id}_{era}
  // which is commonly used in the htt analyses
  ch::SetStandardBinNames(cb);
  //! [part8]
    

  //! [part9]
  // First we generate a set of bin names:
  RooWorkspace ws("htt", "htt");

  TFile demo("htt_mssm_demo.root", "RECREATE");

  bool do_morphing = true;
  map<string, RooAbsReal *> mass_var = {
    {"ggh_htautau", &mh}, {"ggH_Htautau", &mH}, {"ggA_Atautau", &mA},
    {"bbh_htautau", &mh}, {"bbH_Htautau", &mH}, {"bbA_Atautau", &mA}
  };
  if(mass=="MH"){
    mass_var = {
      {"ggH", &mA},
      {"bbH", &mA}
    };
  }
  if (do_morphing) {
    auto bins = cb.bin_set();
    for (auto b : bins) {
      auto procs = cb.cp().bin({b}).signals().process_set();
      for (auto p : procs) {
        ch::BuildRooMorphing(ws, cb, b, p, *(mass_var[p]),
                             "norm", true, true, false, &demo);
      }
    }
  }
  demo.Close();
  cb.AddWorkspace(ws);
  cb.cp().process(ch::JoinStr({signal_types["ggH"], signal_types["bbH"]})).ExtractPdfs(cb, "htt", "$BIN_$PROCESS_morph");
  cb.PrintAll();
  
  string folder = "output/"+output_folder+"/cmb";
  boost::filesystem::create_directories(folder);
 
 //Write out datacards. Naming convention important for rest of workflow. We
 //make one directory per chn-cat, one per chn and cmb. In this code we only
 //store the individual datacards for each directory to be combined later, but
 //note that it's also possible to write out the full combined card with CH
 
 cout << "Writing datacards ...";
  

  //Individual channel-cats  
  for (string chn : chns) {
     string folderchn = "output/"+output_folder+"/"+chn;
     auto bins = cb.cp().channel({chn}).bin_set();
      for (auto b : bins) {
        string folderchncat = "output/"+output_folder+"/"+b;
        boost::filesystem::create_directories(folderchn);
        boost::filesystem::create_directories(folderchncat);
        TFile output((folder + "/"+b+"_input.root").c_str(), "RECREATE");
        TFile outputchn((folderchn + "/"+b+"_input.root").c_str(), "RECREATE");
        TFile outputchncat((folderchncat + "/"+b+"_input.root").c_str(), "RECREATE");
        cb.cp().channel({chn}).bin({b}).mass({"*"}).WriteDatacard(folderchn + "/" + b + ".txt", outputchn);
        cb.cp().channel({chn}).bin({b}).mass({"*"}).WriteDatacard(folderchncat + "/" + b + ".txt", outputchncat);
        cb.cp().channel({chn}).bin({b}).mass({"*"}).WriteDatacard(folder + "/" + b + ".txt", output);
        output.Close();
        outputchn.Close();
        outputchncat.Close();
        if(b.find("8")!=string::npos) {
            string foldercat = "output/"+output_folder+"/htt_cmb_8_13TeV/";
            boost::filesystem::create_directories(foldercat);
            TFile outputcat((folder + "/"+b+"_input.root").c_str(), "RECREATE");
            cb.cp().channel({chn}).bin({b}).mass({"*"}).WriteDatacard(foldercat + "/" + b + ".txt", outputcat);
            outputcat.Close();
        }
        else if(b.find("9")!=string::npos) {
            string foldercat = "output/"+output_folder+"/htt_cmb_9_13TeV/";
            boost::filesystem::create_directories(foldercat);
            TFile outputcat((folder + "/"+b+"_input.root").c_str(), "RECREATE");
            cb.cp().channel({chn}).bin({b}).mass({"*"}).WriteDatacard(foldercat + "/" + b + ".txt", outputcat);
            outputcat.Close();
        }
      }
  }
     
  cb.PrintAll();
  cout << " done\n";


}

