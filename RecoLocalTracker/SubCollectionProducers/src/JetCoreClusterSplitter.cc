#include "FWCore/Framework/interface/stream/EDProducer.h"

#include "DataFormats/Common/interface/DetSetVectorNew.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/SiPixelCluster/interface/SiPixelCluster.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include "RecoLocalTracker/ClusterParameterEstimator/interface/PixelClusterParameterEstimator.h"
#include "RecoLocalTracker/Records/interface/TkPixelCPERecord.h"

#include "Geometry/CommonDetUnit/interface/GlobalTrackingGeometry.h"
#include "Geometry/CommonTopologies/interface/PixelTopology.h"
#include "Geometry/Records/interface/GlobalTrackingGeometryRecord.h"
#include "DataFormats/GeometryVector/interface/VectorUtil.h"

#include "DataFormats/TrackerCommon/interface/TrackerTopology.h"

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/JetReco/interface/Jet.h"

#include <algorithm>
#include <vector>
#include <utility>

class JetCoreClusterSplitter : public edm::stream::EDProducer<> {
public:
  JetCoreClusterSplitter(const edm::ParameterSet& iConfig);
  ~JetCoreClusterSplitter() override = default;
  void produce(edm::Event& iEvent, const edm::EventSetup& iSetup) override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  bool split(const SiPixelCluster& aCluster,
             edmNew::DetSetVector<SiPixelCluster>::FastFiller& filler,
             float expectedADC,
             int sizeY,
             int sizeX,
             float jetZOverRho);
  std::vector<SiPixelCluster> fittingSplit(const SiPixelCluster& aCluster,
                                           float expectedADC,
                                           int sizeY,
                                           int sizeX,
                                           float jetZOverRho,
                                           unsigned int nSplitted);
  std::pair<float, float> closestClusters(const std::vector<float>& distanceMap);
  std::multimap<float, int> secondDistDiffScore(const std::vector<std::vector<float>>& distanceMap);
  std::multimap<float, int> secondDistScore(const std::vector<std::vector<float>>& distanceMap);
  std::multimap<float, int> distScore(const std::vector<std::vector<float>>& distanceMap);

  edm::ESGetToken<GlobalTrackingGeometry, GlobalTrackingGeometryRecord> const tTrackingGeom_;
  edm::ESGetToken<PixelClusterParameterEstimator, TkPixelCPERecord> const tCPE_;
  edm::ESGetToken<TrackerTopology, TrackerTopologyRcd> const tTrackerTopo_;

  bool verbose;
  double ptMin_;
  double deltaR_;
  double chargeFracMin_;
  float expSizeXAtLorentzAngleIncidence_;
  float expSizeXDeltaPerTanAlpha_;
  float expSizeYAtNormalIncidence_;
  float tanLorentzAngle_;
  float tanLorentzAngleBarrelLayer1_;
  edm::EDGetTokenT<edmNew::DetSetVector<SiPixelCluster>> pixelClusters_;
  edm::EDGetTokenT<reco::VertexCollection> vertices_;
  edm::EDGetTokenT<edm::View<reco::Candidate>> cores_;
  double forceXError_;
  double forceYError_;
  double fractionalWidth_;
  double chargePerUnit_;
  double centralMIPCharge_;
};

void JetCoreClusterSplitter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  desc.add<std::string>("pixelCPE", "PixelCPEGeneric");
  desc.add<bool>("verbose", false);
  desc.add<double>("ptMin", 200.0);
  desc.add<double>("deltaRmax", 0.05);
  desc.add<double>("chargeFractionMin", 2.0);
  desc.add<edm::InputTag>("pixelClusters", edm::InputTag("siPixelCluster"));
  desc.add<edm::InputTag>("vertices", edm::InputTag("offlinePrimaryVertices"));
  desc.add<edm::InputTag>("cores", edm::InputTag("ak5CaloJets"));
  desc.add<double>("forceXError", 100.0);
  desc.add<double>("forceYError", 150.0);
  desc.add<double>("fractionalWidth", 0.4);
  desc.add<double>("chargePerUnit", 2000.0);
  desc.add<double>("centralMIPCharge", 26e3);

  desc.add<double>("expSizeXAtLorentzAngleIncidence", 1.5);
  desc.add<double>("expSizeXDeltaPerTanAlpha", 0.0);
  desc.add<double>("expSizeYAtNormalIncidence", 1.3);
  desc.add<double>("tanLorentzAngle", 0.0);
  desc.add<double>("tanLorentzAngleBarrelLayer1", 0.0);

  descriptions.addWithDefaultLabel(desc);
}

JetCoreClusterSplitter::JetCoreClusterSplitter(const edm::ParameterSet& iConfig)
    : tTrackingGeom_(esConsumes()),
      tCPE_(esConsumes(edm::ESInputTag("", iConfig.getParameter<std::string>("pixelCPE")))),
      tTrackerTopo_(esConsumes()),
      verbose(iConfig.getParameter<bool>("verbose")),
      ptMin_(iConfig.getParameter<double>("ptMin")),
      deltaR_(iConfig.getParameter<double>("deltaRmax")),
      chargeFracMin_(iConfig.getParameter<double>("chargeFractionMin")),
      expSizeXAtLorentzAngleIncidence_(iConfig.getParameter<double>("expSizeXAtLorentzAngleIncidence")),
      expSizeXDeltaPerTanAlpha_(iConfig.getParameter<double>("expSizeXDeltaPerTanAlpha")),
      expSizeYAtNormalIncidence_(iConfig.getParameter<double>("expSizeYAtNormalIncidence")),
      tanLorentzAngle_(iConfig.getParameter<double>("tanLorentzAngle")),
      tanLorentzAngleBarrelLayer1_(iConfig.getParameter<double>("tanLorentzAngleBarrelLayer1")),
      pixelClusters_(
          consumes<edmNew::DetSetVector<SiPixelCluster>>(iConfig.getParameter<edm::InputTag>("pixelClusters"))),
      vertices_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("vertices"))),
      cores_(consumes<edm::View<reco::Candidate>>(iConfig.getParameter<edm::InputTag>("cores"))),
      forceXError_(iConfig.getParameter<double>("forceXError")),
      forceYError_(iConfig.getParameter<double>("forceYError")),
      fractionalWidth_(iConfig.getParameter<double>("fractionalWidth")),
      chargePerUnit_(iConfig.getParameter<double>("chargePerUnit")),
      centralMIPCharge_(iConfig.getParameter<double>("centralMIPCharge"))

{
  produces<edmNew::DetSetVector<SiPixelCluster>>();
}

bool SortPixels(const SiPixelCluster::Pixel& i, const SiPixelCluster::Pixel& j) { return (i.adc > j.adc); }

void JetCoreClusterSplitter::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  const auto& geometry = &iSetup.getData(tTrackingGeom_);
  const auto& topology = &iSetup.getData(tTrackerTopo_);

  Handle<edmNew::DetSetVector<SiPixelCluster>> inputPixelClusters;
  iEvent.getByToken(pixelClusters_, inputPixelClusters);

  Handle<std::vector<reco::Vertex>> vertices;
  iEvent.getByToken(vertices_, vertices);
  const reco::Vertex& pv = (*vertices)[0];

  Handle<edm::View<reco::Candidate>> cores;
  iEvent.getByToken(cores_, cores);

  const PixelClusterParameterEstimator* pp = &iSetup.getData(tCPE_);
  auto output = std::make_unique<edmNew::DetSetVector<SiPixelCluster>>();

  edmNew::DetSetVector<SiPixelCluster>::const_iterator detIt = inputPixelClusters->begin();
  for (; detIt != inputPixelClusters->end(); detIt++) {
    edmNew::DetSetVector<SiPixelCluster>::FastFiller filler(*output, detIt->id());
    const edmNew::DetSet<SiPixelCluster>& detset = *detIt;
    const GeomDet* det = geometry->idToDet(detset.id());
    float pitchX, pitchY;
    std::tie(pitchX, pitchY) = static_cast<const PixelTopology&>(det->topology()).pitch();
    float thickness = det->surface().bounds().thickness();
    float tanLorentzAngle = tanLorentzAngle_;
    if (DetId(detset.id()).subdetId() == 1 /* px barrel */ && topology->pxbLayer(detset.id()) == 1) {
      tanLorentzAngle = tanLorentzAngleBarrelLayer1_;
    }
    for (auto cluster = detset.begin(); cluster != detset.end(); cluster++) {
      const SiPixelCluster& aCluster = *cluster;
      bool hasBeenSplit = false;
      bool shouldBeSplit = false;
      GlobalPoint cPos =
          det->surface().toGlobal(pp->localParametersV(aCluster, (*geometry->idToDetUnit(detIt->id())))[0].first);
      GlobalPoint ppv(pv.position().x(), pv.position().y(), pv.position().z());
      GlobalVector clusterDir = cPos - ppv;
      for (unsigned int ji = 0; ji < cores->size(); ji++) {
        if ((*cores)[ji].pt() > ptMin_) {
          const reco::Candidate& jet = (*cores)[ji];
          GlobalVector jetDir(jet.px(), jet.py(), jet.pz());
          if (Geom::deltaR(jetDir, clusterDir) < deltaR_) {
            // check if the cluster has to be splitted

            LocalVector jetDirLocal = det->surface().toLocal(jetDir);
            float jetTanAlpha = jetDirLocal.x() / jetDirLocal.z();
            float jetTanBeta = jetDirLocal.y() / jetDirLocal.z();
            float jetZOverRho = std::sqrt(jetTanAlpha * jetTanAlpha + jetTanBeta * jetTanBeta);
            float expSizeX = expSizeXAtLorentzAngleIncidence_ +
                             std::abs(expSizeXDeltaPerTanAlpha_ * (jetTanAlpha - tanLorentzAngle));
            float expSizeY = std::sqrt((expSizeYAtNormalIncidence_ * expSizeYAtNormalIncidence_) +
                                       thickness * thickness / (pitchY * pitchY) * jetTanBeta * jetTanBeta);
            if (expSizeX < 1.f)
              expSizeX = 1.f;
            if (expSizeY < 1.f)
              expSizeY = 1.f;
            float expCharge = std::sqrt(1.08f + jetZOverRho * jetZOverRho) * centralMIPCharge_;

            if (aCluster.charge() > expCharge * chargeFracMin_ &&
                (aCluster.sizeX() > expSizeX + 1 || aCluster.sizeY() > expSizeY + 1)) {
              shouldBeSplit = true;
              if (verbose)
                std::cout << "Trying to split: charge and deltaR " << aCluster.charge() << " "
                          << Geom::deltaR(jetDir, clusterDir) << " size x y " << aCluster.sizeX() << " "
                          << aCluster.sizeY() << " exp. size (x,y) " << expSizeX << " " << expSizeY << " detid "
                          << detIt->id() << std::endl;
              if (verbose)
                std::cout << "jetZOverRho=" << jetZOverRho << std::endl;

              if (split(aCluster, filler, expCharge, expSizeY, expSizeX, jetZOverRho)) {
                hasBeenSplit = true;
              }
            }
          }
        }
      }
      if (!hasBeenSplit) {
        SiPixelCluster c = aCluster;
        if (shouldBeSplit) {
          // blowup the error if we failed to split a splittable cluster (does
          // it ever happen)
          const float fromCentiToMicro = 1e4;
          c.setSplitClusterErrorX(c.sizeX() *
                                  (pitchX * fromCentiToMicro / 3.f));  // this is not really blowing up .. TODO: tune
          c.setSplitClusterErrorY(c.sizeY() * (pitchY * fromCentiToMicro / 3.f));
        }
        filler.push_back(c);
        std::push_heap(filler.begin(), filler.end(), [](SiPixelCluster const& cl1, SiPixelCluster const& cl2) {
          return cl1.minPixelRow() < cl2.minPixelRow();
        });
      }
    }  // loop over clusters
    std::sort_heap(filler.begin(), filler.end(), [](SiPixelCluster const& cl1, SiPixelCluster const& cl2) {
      return cl1.minPixelRow() < cl2.minPixelRow();
    });

  }  // loop over det
  iEvent.put(std::move(output));
}

bool JetCoreClusterSplitter::split(const SiPixelCluster& aCluster,
                                   edmNew::DetSetVector<SiPixelCluster>::FastFiller& filler,
                                   float expectedADC,
                                   int sizeY,
                                   int sizeX,
                                   float jetZOverRho) {
  // This function should test several configuration of splitting, and then
  // return the one with best chi2

  std::vector<SiPixelCluster> sp = fittingSplit(
      aCluster, expectedADC, sizeY, sizeX, jetZOverRho, std::floor(aCluster.charge() / expectedADC + 0.5f));

  // for the config with best chi2
  for (unsigned int i = 0; i < sp.size(); i++) {
    filler.push_back(sp[i]);
    std::push_heap(filler.begin(), filler.end(), [](SiPixelCluster const& cl1, SiPixelCluster const& cl2) {
      return cl1.minPixelRow() < cl2.minPixelRow();
    });
  }

  return (!sp.empty());
}

// order with fast algo and pick first and second instead?
std::pair<float, float> JetCoreClusterSplitter::closestClusters(const std::vector<float>& distanceMap) {
  float minDist = std::numeric_limits<float>::max();
  float secondMinDist = std::numeric_limits<float>::max();
  for (unsigned int i = 0; i < distanceMap.size(); i++) {
    float dist = distanceMap[i];
    if (dist < minDist) {
      secondMinDist = minDist;
      minDist = dist;
    } else if (dist < secondMinDist) {
      secondMinDist = dist;
    }
  }
  return std::pair<float, float>(minDist, secondMinDist);
}

std::multimap<float, int> JetCoreClusterSplitter::secondDistDiffScore(
    const std::vector<std::vector<float>>& distanceMap) {
  std::multimap<float, int> scores;
  for (unsigned int j = 0; j < distanceMap.size(); j++) {
    std::pair<float, float> d = closestClusters(distanceMap[j]);
    scores.insert(std::pair<float, int>(d.second - d.first, j));
  }
  return scores;
}

std::multimap<float, int> JetCoreClusterSplitter::secondDistScore(const std::vector<std::vector<float>>& distanceMap) {
  std::multimap<float, int> scores;
  for (unsigned int j = 0; j < distanceMap.size(); j++) {
    std::pair<float, float> d = closestClusters(distanceMap[j]);
    scores.insert(std::pair<float, int>(-d.second, j));
  }
  return scores;
}

std::multimap<float, int> JetCoreClusterSplitter::distScore(const std::vector<std::vector<float>>& distanceMap) {
  std::multimap<float, int> scores;
  for (unsigned int j = 0; j < distanceMap.size(); j++) {
    std::pair<float, float> d = closestClusters(distanceMap[j]);
    scores.insert(std::pair<float, int>(-d.first, j));
  }
  return scores;
}

std::vector<SiPixelCluster> JetCoreClusterSplitter::fittingSplit(const SiPixelCluster& aCluster,
                                                                 float expectedADC,
                                                                 int sizeY,
                                                                 int sizeX,
                                                                 float jetZOverRho,
                                                                 unsigned int nSplitted) {
  std::vector<SiPixelCluster> output;

  unsigned int meanExp = nSplitted;
  if (meanExp <= 1) {
    output.push_back(aCluster);
    return output;
  }

  std::vector<float> clx(meanExp);
  std::vector<float> cly(meanExp);
  std::vector<float> cls(meanExp);
  std::vector<float> oldclx(meanExp);
  std::vector<float> oldcly(meanExp);
  std::vector<SiPixelCluster::Pixel> originalpixels = aCluster.pixels();
  std::vector<std::pair<int, SiPixelCluster::Pixel>> pixels;
  for (unsigned int j = 0; j < originalpixels.size(); j++) {
    int sub = originalpixels[j].adc / chargePerUnit_ * expectedADC / centralMIPCharge_;
    if (sub < 1)
      sub = 1;
    int perDiv = originalpixels[j].adc / sub;
    if (verbose)
      std::cout << "Splitting  " << j << "  in [ " << pixels.size() << " , " << pixels.size() + sub
                << " ], expected numb of clusters: " << meanExp << " original pixel (x,y) " << originalpixels[j].x
                << " " << originalpixels[j].y << " sub " << sub << std::endl;
    for (int k = 0; k < sub; k++) {
      if (k == sub - 1)
        perDiv = originalpixels[j].adc - perDiv * k;
      pixels.push_back(std::make_pair(j, SiPixelCluster::Pixel(originalpixels[j].x, originalpixels[j].y, perDiv)));
    }
  }
  std::vector<int> clusterForPixel(pixels.size());
  // initial values
  for (unsigned int j = 0; j < meanExp; j++) {
    oldclx[j] = -999;
    oldcly[j] = -999;
    clx[j] = originalpixels[0].x + j;
    cly[j] = originalpixels[0].y + j;
    cls[j] = 0;
  }
  bool stop = false;
  int remainingSteps = 100;
  while (!stop && remainingSteps > 0) {
    remainingSteps--;
    // Compute all distances
    std::vector<std::vector<float>> distanceMapX(originalpixels.size(), std::vector<float>(meanExp));
    std::vector<std::vector<float>> distanceMapY(originalpixels.size(), std::vector<float>(meanExp));
    std::vector<std::vector<float>> distanceMap(originalpixels.size(), std::vector<float>(meanExp));
    for (unsigned int j = 0; j < originalpixels.size(); j++) {
      if (verbose)
        std::cout << "Original Pixel pos " << j << " " << pixels[j].second.x << " " << pixels[j].second.y << std::endl;
      for (unsigned int i = 0; i < meanExp; i++) {
        distanceMapX[j][i] = 1.f * originalpixels[j].x - clx[i];
        distanceMapY[j][i] = 1.f * originalpixels[j].y - cly[i];
        float dist = 0;
        //				float sizeX=2;
        if (std::abs(distanceMapX[j][i]) > sizeX / 2.f) {
          dist +=
              (std::abs(distanceMapX[j][i]) - sizeX / 2.f + 1.f) * (std::abs(distanceMapX[j][i]) - sizeX / 2.f + 1.f);
        } else {
          dist += (2.f * distanceMapX[j][i] / sizeX) * (2.f * distanceMapX[j][i] / sizeX);
        }

        if (std::abs(distanceMapY[j][i]) > sizeY / 2.f) {
          dist += 1.f * (std::abs(distanceMapY[j][i]) - sizeY / 2.f + 1.f) *
                  (std::abs(distanceMapY[j][i]) - sizeY / 2.f + 1.f);
        } else {
          dist += 1.f * (2.f * distanceMapY[j][i] / sizeY) * (2.f * distanceMapY[j][i] / sizeY);
        }
        distanceMap[j][i] = sqrt(dist);
        if (verbose)
          std::cout << "Cluster " << i << " Original Pixel " << j << " distances: " << distanceMapX[j][i] << " "
                    << distanceMapY[j][i] << " " << distanceMap[j][i] << std::endl;
      }
    }
    // Compute scores for sequential addition. The first index is the
    // distance, in whatever metrics we use, while the second is the
    // pixel index w.r.t which the distance is computed.
    std::multimap<float, int> scores;

    // Using different rankings to improve convergence (as Giulio proposed)
    scores = secondDistScore(distanceMap);

    // Iterate starting from the ones with furthest second best clusters, i.e.
    // easy choices
    std::vector<float> weightOfPixel(pixels.size());
    for (std::multimap<float, int>::iterator it = scores.begin(); it != scores.end(); it++) {
      int pixel_index = it->second;
      if (verbose)
        std::cout << "Original Pixel " << pixel_index << " with score " << it->first << std::endl;
      // find cluster that is both close and has some charge still to assign
      int subpixel_counter = 0;
      for (auto subpixel = pixels.begin(); subpixel != pixels.end(); ++subpixel, ++subpixel_counter) {
        if (subpixel->first > pixel_index) {
          break;
        } else if (subpixel->first != pixel_index) {
          continue;
        } else {
          float maxEst = 0;
          int cl = -1;
          for (unsigned int subcluster_index = 0; subcluster_index < meanExp; subcluster_index++) {
            float nsig = (cls[subcluster_index] - expectedADC) /
                         (expectedADC * fractionalWidth_);        // 20% uncertainty? realistic from Landau?
            float clQest = 1.f / (1.f + std::exp(nsig)) + 1e-6f;  // 1./(1.+exp(x*x-3*3))
            float clDest = 1.f / (distanceMap[pixel_index][subcluster_index] + 0.05f);

            if (verbose)
              std::cout << " Q: " << clQest << " D: " << clDest << " " << distanceMap[pixel_index][subcluster_index]
                        << std::endl;
            float est = clQest * clDest;
            if (est > maxEst) {
              cl = subcluster_index;
              maxEst = est;
            }
          }
          cls[cl] += subpixel->second.adc;
          clusterForPixel[subpixel_counter] = cl;
          weightOfPixel[subpixel_counter] = maxEst;
          if (verbose)
            std::cout << "Pixel weight j cl " << weightOfPixel[subpixel_counter] << " " << subpixel_counter << " " << cl
                      << std::endl;
        }
      }
    }
    // Recompute cluster centers
    stop = true;
    for (unsigned int subcluster_index = 0; subcluster_index < meanExp; subcluster_index++) {
      if (std::abs(clx[subcluster_index] - oldclx[subcluster_index]) > 0.01f)
        stop = false;  // still moving
      if (std::abs(cly[subcluster_index] - oldcly[subcluster_index]) > 0.01f)
        stop = false;
      oldclx[subcluster_index] = clx[subcluster_index];
      oldcly[subcluster_index] = cly[subcluster_index];
      clx[subcluster_index] = 0;
      cly[subcluster_index] = 0;
      cls[subcluster_index] = 1e-99;
    }
    for (unsigned int pixel_index = 0; pixel_index < pixels.size(); pixel_index++) {
      if (clusterForPixel[pixel_index] < 0)
        continue;
      if (verbose)
        std::cout << "j " << pixel_index << " x " << pixels[pixel_index].second.x << " * y "
                  << pixels[pixel_index].second.y << " * ADC " << pixels[pixel_index].second.adc << " * W "
                  << weightOfPixel[pixel_index] << std::endl;
      clx[clusterForPixel[pixel_index]] += pixels[pixel_index].second.x * pixels[pixel_index].second.adc;
      cly[clusterForPixel[pixel_index]] += pixels[pixel_index].second.y * pixels[pixel_index].second.adc;
      cls[clusterForPixel[pixel_index]] += pixels[pixel_index].second.adc;
    }
    for (unsigned int subcluster_index = 0; subcluster_index < meanExp; subcluster_index++) {
      if (cls[subcluster_index] != 0) {
        clx[subcluster_index] /= cls[subcluster_index];
        cly[subcluster_index] /= cls[subcluster_index];
      }
      if (verbose)
        std::cout << "Center for cluster " << subcluster_index << " x,y " << clx[subcluster_index] << " "
                  << cly[subcluster_index] << std::endl;
      cls[subcluster_index] = 0;
    }
  }
  if (verbose)
    std::cout << "maxstep " << remainingSteps << std::endl;
  // accumulate pixel with same cl
  std::vector<std::vector<SiPixelCluster::Pixel>> pixelsForCl(meanExp);
  for (int cl = 0; cl < (int)meanExp; cl++) {
    for (unsigned int j = 0; j < pixels.size(); j++) {
      if (clusterForPixel[j] == cl and pixels[j].second.adc != 0) {  // for each pixel of cluster
                                                                     // cl find the other pixels
                                                                     // with same x,y and
                                                                     // accumulate+reset their adc
        for (unsigned int k = j + 1; k < pixels.size(); k++) {
          if (pixels[k].second.adc != 0 and pixels[k].second.x == pixels[j].second.x and
              pixels[k].second.y == pixels[j].second.y and clusterForPixel[k] == cl) {
            if (verbose)
              std::cout << "Resetting all sub-pixel for location " << pixels[k].second.x << ", " << pixels[k].second.y
                        << " at index " << k << " associated to cl " << clusterForPixel[k] << std::endl;
            pixels[j].second.adc += pixels[k].second.adc;
            pixels[k].second.adc = 0;
          }
        }
        for (unsigned int p = 0; p < pixels.size(); ++p)
          if (verbose)
            std::cout << "index, x, y, ADC: " << p << ", " << pixels[p].second.x << ", " << pixels[p].second.y << ", "
                      << pixels[p].second.adc << " associated to cl " << clusterForPixel[p] << std::endl
                      << "Adding pixel " << pixels[j].second.x << ", " << pixels[j].second.y << " to cluster " << cl
                      << std::endl;
        pixelsForCl[cl].push_back(pixels[j].second);
      }
    }
  }

  //	std::vector<std::vector<std::vector<SiPixelCluster::PixelPos *> > >
  //pixelMap(meanExp,std::vector<std::vector<SiPixelCluster::PixelPos *>
  //>(512,std::vector<SiPixelCluster::Pixel *>(512,0)));

  for (int cl = 0; cl < (int)meanExp; cl++) {
    if (verbose)
      std::cout << "Pixels of cl " << cl << " ";
    for (unsigned int j = 0; j < pixelsForCl[cl].size(); j++) {
      SiPixelCluster::PixelPos newpix(pixelsForCl[cl][j].x, pixelsForCl[cl][j].y);
      if (verbose)
        std::cout << pixelsForCl[cl][j].x << "," << pixelsForCl[cl][j].y << "|";
      if (j == 0) {
        output.emplace_back(newpix, pixelsForCl[cl][j].adc);
      } else {
        output.back().add(newpix, pixelsForCl[cl][j].adc);
      }
    }
    if (verbose)
      std::cout << std::endl;
    if (!pixelsForCl[cl].empty()) {
      if (forceXError_ > 0)
        output.back().setSplitClusterErrorX(forceXError_);
      if (forceYError_ > 0)
        output.back().setSplitClusterErrorY(forceYError_);
    }
  }
  //	if(verbose)	std::cout << "Weights" << std::endl;
  //	if(verbose)	print(theWeights,aCluster,1);
  //	if(verbose)	std::cout << "Unused charge" << std::endl;
  //	if(verbose)	print(theBufferResidual,aCluster);

  return output;
}

#include "FWCore/PluginManager/interface/ModuleDef.h"
#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(JetCoreClusterSplitter);
