///  Goodness-of-fit tutorial
///
///  based on the ROOT tutorial:
///  'ADDITION AND CONVOLUTION' RooFit tutorial macro #201
///
/// \author 07/2008 - Wouter Verkerke 
/// \author 02/2018 - Emilien Chapon

#include "RooRealVar.h"
#include "RooDataSet.h"
#include "RooGaussian.h"
#include "RooChebychev.h"
#include "RooAddPdf.h"
#include "RooMsgService.h"
#include "RooPlot.h"
#include "RooFitResult.h"

#include "TCanvas.h"
#include "TAxis.h"
#include "TTree.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TStyle.h"

#include "RooGoF.C"

using namespace RooFit ;


void testGoF()
{
   // S e t u p   c o m p o n e n t   p d f s 
   // ---------------------------------------

   // Declare observable x
   RooRealVar x("x","x",0,10) ;
   x.setBins(100);

   // Create two Gaussian PDFs g1(x,mean1,sigma) anf g2(x,mean2,sigma) and their parameters
   RooRealVar mean("mean","mean of gaussians",5) ;
   RooRealVar sigma1("sigma1","width of gaussians",0.5) ;
   RooRealVar sigma2("sigma2","width of gaussians",1) ;

   RooGaussian sig1("sig1","Signal component 1",x,mean,sigma1) ;  
   RooGaussian sig2("sig2","Signal component 2",x,mean,sigma2) ;  

   // Build Chebychev polynomial p.d.f.  
   RooRealVar a0("a0","a0",0.5,0.,1.) ;
   RooRealVar a1("a1","a1",0.2,0.,1.) ;
   RooChebychev bkg("bkg","Background",x,RooArgSet(a0,a1)) ;

   // A d d   s i g n a l   c o m p o n e n t s 
   // ------------------------------------------

   // Sum the signal components into a composite signal p.d.f.
   RooRealVar sig1frac("sig1frac","fraction of component 1 in signal",0.8,0.,1.) ;
   RooAddPdf sig("sig","Signal",RooArgList(sig1,sig2),sig1frac) ;


   // A d d  s i g n a l   a n d   b a c k g r o u n d
   // ------------------------------------------------

   // Sum the composite signal and background 
   RooRealVar bkgfrac("bkgfrac","fraction of background",0.5,0.,1.) ;
   RooAddPdf  model("model","g1+g2+a",RooArgList(bkg,sig),bkgfrac) ;


   // S a m p l e ,   f i t   a n d   p l o t   m o d e l
   // ---------------------------------------------------

   // Generate a data sample of 1000 events in x from model
   RooDataSet *data = model.generate(x,1000) ;

   // Fit model to data
   RooFitResult *fr = model.fitTo(*data,Save(),NumCPU(4)) ;
   // save best fit parameters
   RooArgSet* params = model.getParameters(x) ;
   RooArgSet* bestFitParams = (RooArgSet*) params->snapshot() ;

   // need to account for the number of fit parameters when computing the chi2 pvalues
   int d_ndf = fr->floatParsFinal().getSize() ;

   // Plot data and PDF overlaid
   RooPlot* xframe = x.frame(Title("Example of composite pdf=(sig1+sig2)+bkg")) ;
   data->plotOn(xframe) ;
   model.plotOn(xframe) ;

   // Draw the frame on the canvas
   TCanvas *c = new TCanvas("rf201_composite","rf201_composite",600,600) ;
   gPad->SetLeftMargin(0.15) ; xframe->GetYaxis()->SetTitleOffset(1.4) ; xframe->Draw() ;
   c->SaveAs("data.pdf");


   // G o o d n e s s - o f - f i t
   // -----------------------------

   // First, print the RooPlot to figure out the name of objects inside
   xframe->Print("v");

   // GoF objects
   // for binned tests
   RooGoF goftest(xframe->getHist("h_modelData"),xframe->getCurve("model_Norm[x]"));
   goftest.setRange(x.getMin(),x.getMax());
   goftest.setRebin(5,false); // better rebin to make sure that all bins have >=5 expected events

   // for unbinned tests
   // RooGoF goftest_unbinned(data,xframe->getCurve("model_Norm[x]"),"x");
   RooGoF goftest_unbinned(data,&model,&x);
   goftest_unbinned.setRange(x.getMin(),x.getMax());
   double pvalue, testStat;

   // unbinned tests
   goftest_unbinned.ADTest(pvalue,testStat);
   cout << "AD (asym.): " << pvalue << ", " << testStat << endl;
   goftest_unbinned.KSTest(pvalue,testStat);
   cout << "KS (asym.): " << pvalue << ", " << testStat << endl;

   // we can also estimate the p-value using toys
   goftest_unbinned.setNtoys(1000,true,NumCPU(4));
   goftest_unbinned.ADTest(pvalue,testStat);
   cout << "AD (toys): " << pvalue << ", " << testStat << endl;
   goftest_unbinned.KSTest(pvalue,testStat);
   cout << "KS (toys): " << pvalue << ", " << testStat << endl;
   
   // binned tests
   int ndf=0;
   goftest.BCChi2Test(pvalue,testStat,ndf,d_ndf);
   cout << "BC: " << pvalue << ", " << testStat << endl;
   goftest.NeymanChi2Test(pvalue,testStat,ndf,d_ndf);
   cout << "Neyman: " << pvalue << ", " << testStat << endl;
   goftest.PearsonChi2Test(pvalue,testStat,ndf,d_ndf);
   cout << "Pearson: " << pvalue << ", " << testStat << endl;
   goftest.RooFitChi2Test(pvalue,testStat,ndf,d_ndf);
   cout << "RooFit: " << pvalue << ", " << testStat << endl;

   // return;




   // T O Y   S T U D Y
   // -----------------
   int ntoys = 1000;

   // we will recycle the distributions of the AD and KS test statistics from toys
   RooStats::SamplingDistribution *sd_AD = goftest_unbinned.getSamplingDist_AD();
   RooStats::SamplingDistribution *sd_KS = goftest_unbinned.getSamplingDist_KS();

   // set up the tree
   double pval_AD_before, ts_AD_before;
   double pval_AD_after, ts_AD_after, pval_AD_after_toys;
   double pval_KS_before, ts_KS_before;
   double pval_KS_after, ts_KS_after, pval_KS_after_toys;
   double pval_BCChi2_before, ts_BCChi2_before;
   double pval_BCChi2_after, ts_BCChi2_after;
   double pval_RooFitChi2_before, ts_RooFitChi2_before;
   double pval_RooFitChi2_after, ts_RooFitChi2_after;
   double pval_PearsonChi2_before, ts_PearsonChi2_before;
   double pval_PearsonChi2_after, ts_PearsonChi2_after;
   double pval_NeymanChi2_before, ts_NeymanChi2_before;
   double pval_NeymanChi2_after, ts_NeymanChi2_after;
   TTree *tr = new TTree("toys_ts","");
   tr->Branch("pval_AD_before",&pval_AD_before,"pval_AD_before/D");
   tr->Branch("ts_AD_before",&ts_AD_before,"ts_AD_before/D");
   tr->Branch("pval_AD_after",&pval_AD_after,"pval_AD_after/D");
   tr->Branch("pval_AD_after_toys",&pval_AD_after_toys,"pval_AD_after_toys/D");
   tr->Branch("ts_AD_after",&ts_AD_after,"ts_AD_after/D");
   tr->Branch("pval_KS_before",&pval_KS_before,"pval_KS_before/D");
   tr->Branch("ts_KS_before",&ts_KS_before,"ts_KS_before/D");
   tr->Branch("pval_KS_after",&pval_KS_after,"pval_KS_after/D");
   tr->Branch("pval_KS_after_toys",&pval_KS_after_toys,"pval_KS_after_toys/D");
   tr->Branch("ts_KS_after",&ts_KS_after,"ts_KS_after/D");
   tr->Branch("pval_BCChi2_before",&pval_BCChi2_before,"pval_BCChi2_before/D");
   tr->Branch("ts_BCChi2_before",&ts_BCChi2_before,"ts_BCChi2_before/D");
   tr->Branch("pval_BCChi2_after",&pval_BCChi2_after,"pval_BCChi2_after/D");
   tr->Branch("ts_BCChi2_after",&ts_BCChi2_after,"ts_BCChi2_after/D");
   tr->Branch("pval_RooFitChi2_before",&pval_RooFitChi2_before,"pval_RooFitChi2_before/D");
   tr->Branch("ts_RooFitChi2_before",&ts_RooFitChi2_before,"ts_RooFitChi2_before/D");
   tr->Branch("pval_RooFitChi2_after",&pval_RooFitChi2_after,"pval_RooFitChi2_after/D");
   tr->Branch("ts_RooFitChi2_after",&ts_RooFitChi2_after,"ts_RooFitChi2_after/D");
   tr->Branch("pval_PearsonChi2_before",&pval_PearsonChi2_before,"pval_PearsonChi2_before/D");
   tr->Branch("ts_PearsonChi2_before",&ts_PearsonChi2_before,"ts_PearsonChi2_before/D");
   tr->Branch("pval_PearsonChi2_after",&pval_PearsonChi2_after,"pval_PearsonChi2_after/D");
   tr->Branch("ts_PearsonChi2_after",&ts_PearsonChi2_after,"ts_PearsonChi2_after/D");
   tr->Branch("pval_NeymanChi2_before",&pval_NeymanChi2_before,"pval_NeymanChi2_before/D");
   tr->Branch("ts_NeymanChi2_before",&ts_NeymanChi2_before,"ts_NeymanChi2_before/D");
   tr->Branch("pval_NeymanChi2_after",&pval_NeymanChi2_after,"pval_NeymanChi2_after/D");
   tr->Branch("ts_NeymanChi2_after",&ts_NeymanChi2_after,"ts_NeymanChi2_after/D");

   // silence RooFit output during toys
   RooFit::MsgLevel oldLevel = RooMsgService::instance().globalKillBelow() ;
   RooMsgService::instance().setGlobalKillBelow(RooFit::FATAL) ;
   RooMsgService::instance().setSilentMode(true) ;


   // main loop for toys
   for (int i=0; i<ntoys; i++) {
      if (i%100==0) cout << i << "/" << ntoys << endl;
      // go back to initial parameters
      *params = *bestFitParams;
      
      // generate pseudo-dataset
      RooDataSet *datatoy = model.generate(x,1000) ;
      
      // plot it on tmp frame
      RooPlot* xframetoy = x.frame(Title("toy dataset")) ;
      datatoy->plotOn(xframetoy) ;
      model.plotOn(xframetoy) ;

      // do gof before
      RooGoF goftesttoy(xframetoy->getHist("h_modelData"),xframetoy->getCurve("model_Norm[x]"));
      goftesttoy.setRange(x.getMin(),x.getMax());
      goftesttoy.setRebin(5,false); 
      // RooGoF goftesttoy_unbinned(datatoy,xframetoy->getCurve("model_Norm[x]"),"x");
      RooGoF goftesttoy_unbinned(datatoy,&model,&x);
      goftesttoy_unbinned.setRange(x.getMin(),x.getMax());
      
      goftesttoy_unbinned.ADTest(pval_AD_before,ts_AD_before);
      goftesttoy_unbinned.KSTest(pval_KS_before,ts_KS_before);
      goftesttoy.BCChi2Test(pval_BCChi2_before,ts_BCChi2_before,ndf);
      goftesttoy.RooFitChi2Test(pval_RooFitChi2_before,ts_RooFitChi2_before,ndf);
      goftesttoy.NeymanChi2Test(pval_NeymanChi2_before,ts_NeymanChi2_before,ndf);
      goftesttoy.PearsonChi2Test(pval_PearsonChi2_before,ts_PearsonChi2_before,ndf);

      // do the fit
      model.fitTo(*datatoy,NumCPU(4)) ;

      // do gof after
      RooPlot* xframetoy2 = x.frame(Title("toy dataset")) ;
      datatoy->plotOn(xframetoy2) ;
      model.plotOn(xframetoy2) ;
      RooGoF goftesttoy2(xframetoy2->getHist("h_modelData"),xframetoy2->getCurve("model_Norm[x]"));
      goftesttoy2.setRange(x.getMin(),x.getMax());
      goftesttoy2.setRebin(5,false); 
      // RooGoF goftesttoy2_unbinned(datatoy,xframetoy2->getCurve("model_Norm[x]"),"x");
      RooGoF goftesttoy2_unbinned(datatoy,&model,&x);
      RooGoF goftesttoy2_unbinned_t(datatoy,&model,&x);
      goftesttoy2_unbinned.setRange(x.getMin(),x.getMax());
      goftesttoy2_unbinned_t.setRange(x.getMin(),x.getMax());
      goftesttoy2_unbinned_t.setNtoys(100,true);
      goftesttoy2_unbinned_t.setSamplingDist_AD(sd_AD);
      goftesttoy2_unbinned_t.setSamplingDist_KS(sd_KS);

      goftesttoy2_unbinned.ADTest(pval_AD_after,ts_AD_after);
      goftesttoy2_unbinned.KSTest(pval_KS_after,ts_KS_after);
      double tmp;
      goftesttoy2_unbinned_t.ADTest(pval_AD_after_toys,tmp);
      goftesttoy2_unbinned_t.KSTest(pval_KS_after_toys,tmp);
      goftesttoy2.BCChi2Test(pval_BCChi2_after,ts_BCChi2_after,ndf,d_ndf);
      goftesttoy2.RooFitChi2Test(pval_RooFitChi2_after,ts_RooFitChi2_after,ndf,d_ndf);
      goftesttoy2.NeymanChi2Test(pval_NeymanChi2_after,ts_NeymanChi2_after,ndf,d_ndf);
      goftesttoy2.PearsonChi2Test(pval_PearsonChi2_after,ts_PearsonChi2_after,ndf,d_ndf);

      // fill the tree
      tr->Fill();

      // clean up
      delete datatoy;
      delete xframetoy;
      delete xframetoy2;
   }

   RooMsgService::instance().setGlobalKillBelow(oldLevel) ;

   // plot the toy results
   TCanvas c1("c1","c1");
   c1.Print("plots.pdf[");
   gStyle->SetOptTitle(1);
   gStyle->SetOptStat("emr");
   tr->Draw("pval_AD_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_AD_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_AD_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_AD_after_toys");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_AD_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_KS_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_KS_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_KS_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_KS_after_toys");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_KS_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_BCChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_BCChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_BCChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_BCChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_RooFitChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_RooFitChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_RooFitChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_RooFitChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_NeymanChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_NeymanChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_NeymanChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_NeymanChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_PearsonChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_PearsonChi2_before");
   c1.SaveAs("plots.pdf");
   tr->Draw("pval_PearsonChi2_after");
   c1.SaveAs("plots.pdf");
   tr->Draw("ts_PearsonChi2_after");
   c1.SaveAs("plots.pdf");
   c1.Print("plots.pdf]");

   // save tree to file
   TFile *f = new TFile("toys.root","RECREATE");
   tr->Write();
   f->Close();
}

