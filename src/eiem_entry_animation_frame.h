#pragma once

namespace RuntimeAnimations {
// RuntimeShapes::Update owns the Unity-frame guard. All target adapters observe
// this one clock, after input and generation publication. Binding attachments
// can create author nodes; sample those nodes in the same frame before render.
void Frame(double elapsed,int frame) {
  {EiemLoadPerformance::Scope animation(EiemLoadPerformance::ShapeAnimation);
    RuntimeControls::AdvanceAnimations(elapsed);}
  RuntimeMaterials::Frame();
  RuntimeShapes::ApplyOutputs(frame);
  RuntimeAttachments::Frame();
  RuntimeAuthorTransforms::Frame();
}
}
