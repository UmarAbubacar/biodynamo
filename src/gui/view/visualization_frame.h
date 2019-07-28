// -----------------------------------------------------------------------------
//
// Copyright (C) The BioDynaMo Project.
// All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#ifndef GUI_VISUALIZATION_FRAME_H_
#define GUI_VISUALIZATION_FRAME_H_

#include <iostream>

#include <TEveBrowser.h>
#include <TEveGeoNode.h>
#include <TEveManager.h>
#include <TGeoManager.h>
#include <TEveViewer.h>
#include <TSystem.h>
#include <TEveGedEditor.h>
#include <TRootEmbeddedCanvas.h>
#include <TCanvas.h>
#include <TGButton.h>
#include <TView.h>
#include <TColor.h>
#include "TGClient.h"

#include "biodynamo.h"
#include "gui/controller/project.h"

namespace gui {

class VisFrame : public TGCompositeFrame {
 public:
  VisFrame(const TGWindow* p);
  ~VisFrame();

  /**
   * Creates TEveManager window, initializes members
   */
  void Init() {
    //TEveManager::Create();

    new TGeoManager("Visualization", "BioDynaMo");

    // Set number of segments for approximating circles in drawing.
    // Keep it low for better performance.
    gGeoManager->SetNsegments(4);

    mat_empty_space_ = new TGeoMaterial("EmptySpace", 0, 0, 0);
    mat_solid_ = new TGeoMaterial("Solid", .938, 1., 10000.);
    med_empty_space_ = new TGeoMedium("Empty", 1, mat_empty_space_);
    med_solid_ = new TGeoMedium("Solid", 1, mat_solid_);

    // Another way to make top volume for world. In this way it will be
    // unbounded.
    top_ = new TGeoVolumeAssembly("WORLD");

    gGeoManager->SetTopVolume(top_);
    // geom_->SetMultiThread(true);

    // connect geom_ to eve
    //TGeoNode *node = gGeoManager->GetTopNode();
    //eve_top_ = new TEveGeoTopNode(geom_, node);
    //gEve->AddGlobalElement(eve_top_);
    //gEve->AddElement(eve_top_);

    // number of visualized nodes inside one volume. If you exceed this number,
    // ROOT will draw nothing.
    gGeoManager->SetMaxVisNodes(max_viz_nodes_);

    //gEve->GetBrowser()->GetMainFrame()->SetWindowName(
    //    "Biodynamo Visualization");
    is_geometry_closed_ = false;
    init_ = true;
    cell_count = 1111;
  }

  /**
   * Updates GLViewer of TEveManager according to current state of ECM.
   */
  void Update() {
    std::cout << "In VisFrame Update()!\n";
    if (!init_)
      throw std::runtime_error("Call GUI::getInstance().Init() first!");

    if (is_geometry_closed_)
      throw std::runtime_error(
          "Geometry is already closed! Don't call GUI::Update() after "
          "GUI::CloseGeometry()!");
    std::cout << "\tClearing nodes!\n";
    top_->ClearNodes();

    auto* sim = Project::GetInstance().GetSimulation();
    auto* rm = sim->GetResourceManager();

    //rm->ApplyOnAllElements([&](auto* cells, uint16_t type_idx) {
    //  auto so_name = cells->GetTypeName();
    //  for (size_t i = 0; i < cells->size(); i++) {
    //    auto container = new TGeoVolumeAssembly("A");
    //    this->AddBranch((*cells)[i], container, so_name);
    //    top_->AddNode(container, top_->GetNdaughters());
    //  }
    //});

    rm->ApplyOnAllElements([&](bdm::SimObject* cells) {
      auto so_name = cells->GetTypeName();
      auto container = new TGeoVolumeAssembly("A");
      this->AddBranch((*cells), container, so_name);
      top_->AddNode(container, top_->GetNdaughters());
      std::cout << "\tUpdating cell!\n";
    });

    fCanvas->cd();
    fCanvas->SetFillColor(1);
    fCanvas->Clear();
    gGeoManager->CloseGeometry();
    gGeoManager->GetTopVolume()->Draw();

    //gEve->FullRedraw3D();
    //gSystem->ProcessEvents();
  
    update_ = true;
  }

  /**
   * Setter for this->MaxVizNodes
   * @param number - number of visualizable nodes per volume
   */
  void SetMaxVizNodes(int number) { this->max_viz_nodes_ = number; }

  /**
   * After building the full geometry tree, geometry must be closed.
   * Closing geometry implies checking the geometry validity, fixing shapes with
   * negative parameters (run-time shapes) building the cache manager,
   * voxelizing
   * all volumes, counting the total number of physical nodes and registering
   * the manager class to the browser.
   */
  void CloseGeometry() {
    if (!update_)
      throw std::runtime_error("Call GUI::getInstance().Update() first!");

    gGeoManager->CloseGeometry();

    is_geometry_closed_ = true;
  }

  virtual Bool_t      ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2);

 private:

 /**
   * Recursively adds sphere and its daughters to the container.
   */
  template <typename SO>
  void AddBranch(SO&& cell, TGeoVolume *container, std::string name) {
    AddCellToVolume(cell, container, name);
    // to be extended for other object
  }

  /**
   * Adds sphere to the volume, its name will be: "S%d", sphereID
   */
  template <typename SO>
  void AddCellToVolume(SO&& cell, TGeoVolume *container, std::string so_name) {
    std::string name = so_name + std::to_string(cell_count++);
    auto radius = cell.GetDiameter() / 2;
    auto massLocation = cell.GetPosition();
    auto x = massLocation[0];
    auto y = massLocation[1];
    auto z = massLocation[2];
    auto position = new TGeoTranslation(x, y, z);

    auto volume = gGeoManager->MakeSphere(name.c_str(), med_solid_, 0., radius);
    volume->SetLineColor(kBlue);

    container->AddNode(volume, container->GetNdaughters(), position);
  }

  ///----Visualization-Canvas----///
  TRootEmbeddedCanvas*                    fEmbeddedCanvas;
  TCanvas*                                fCanvas;

  std::unique_ptr<TGHorizontalFrame>      fHFrame;
  std::unique_ptr<TGTextButton>           fZoomPlusButton;
  std::unique_ptr<TGTextButton>           fZoomMinusButton;
  std::unique_ptr<TGLayoutHints>          fL1;

  /// Top volumes for TGeo and TEve (world)
  TGeoVolume *top_;
  TEveGeoTopNode *eve_top_;

  /// Eve materials and medium
  TGeoMaterial *mat_empty_space_;
  TGeoMaterial *mat_solid_;
  TGeoMedium *med_empty_space_;
  TGeoMedium *med_solid_;

  // just to ensure that methods were called in correct order
  bool init_;                // true if init_ was called
  bool update_;              // true if update was called
  bool is_geometry_closed_;  // true if geometry is already closed

  /// Max visualized shapes per volume.
  int max_viz_nodes_;

  int cell_count;

};

}

#endif // GUI_VISUALIZATION_FRAME_H_