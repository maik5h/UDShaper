
#include "LFOController.h"

SecondsBoxControl::SecondsBoxControl(IRECT rect, int parameterIdx)
  : IVNumberBoxControl(rect, parameterIdx, nullptr, "", DEFAULT_STYLE, false, 1., 0.05, 60., "%.2f s", false)
{
  // TODO Why can I not change the idle background color??

  mSmallIncrement = 0.05;
  mLargeIncrement = 0.1;
}


void SecondsBoxControl::SetDisabled(bool disable)
{
  ForAllChildrenFunc([disable](int childIdx, IControl* pChild) { pChild->SetDisabled(disable); });

  IControl::SetDisabled(disable);

  // Trigger redraw of contents after enabling.
  if (!disable && mTextReadout != nullptr)
  {
    OnValueChanged();
  }
}

void SecondsBoxControl::OnValueChanged(bool preventAction)
{
  mRealValue = Clip(mRealValue, mMinValue, mMaxValue);

  // Snap to .05 before displaying.
  // Casting x to int rounds down to next int. Casting x + 0.5 to int rounds to nearest int.
  // Rounding 20 * x to nearest int is equivalent as snapping to .05.
  double outValue = static_cast<double>(static_cast<int>(mRealValue * 20 + 0.5)) / 20;

  // Make increments smaller for small values.
  if (mRealValue <= 1)
  {
    mLargeIncrement = 0.01;
  }
  else if (mRealValue <= 10)
  {
    mLargeIncrement = 0.05;
  }
  else if (mRealValue <= 30)
  {
    mLargeIncrement = 0.1;
  }
  else
  {
    mLargeIncrement = 0.2;
  }

  if (mRealValue >= 0.95)
  {
    mTextReadout->SetStrFmt(32, fmtSeconds.Get(), outValue);
  }
  else
  {
    mTextReadout->SetStrFmt(32, fmtMilliseconds.Get(), 1000 * outValue);
  }

  if (!preventAction && GetParam())
  {
    SetValue(GetParam()->ToNormalized(outValue));
  }

  SetDirty(!preventAction);
}

void SecondsBoxControl::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  // Mousewheel is always using high precision.
  double inc = (d > 0.f ? 1. : -1.) * mSmallIncrement;
  mRealValue += inc;
  OnValueChanged();
}

void SecondsBoxControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  double gearing = IsFineControl(mod, true) ? mSmallIncrement : mLargeIncrement;
  mRealValue -= (double(dY) * gearing);
  OnValueChanged();
}

BeatsBoxControl::BeatsBoxControl(IRECT rect, int parameterIdx, int initValue)
  : IVNumberBoxControl(rect, parameterIdx, nullptr, "", DEFAULT_STYLE, false, initValue, 0., 12., FREQUENCY_COUNTER_STRINGS[initValue].data(), false)
{
  mSmallIncrement = 0.05;
  mLargeIncrement = 1.;
}

void BeatsBoxControl::SetDisabled(bool disable)
{
  ForAllChildrenFunc([disable](int childIdx, IControl* pChild){ pChild->SetDisabled(disable); });

  IControl::SetDisabled(disable);

  // Trigger redraw of contents after enabling.
  if (!disable && mTextReadout != nullptr)
  {
    OnValueChanged();
  }
}

void BeatsBoxControl::OnValueChanged(bool preventAction)
{
  mRealValue = Clip(mRealValue, mMinValue, mMaxValue);

  // Value is casted to index which defines which of the predefined values to choose from.
  int idx = static_cast<int>(mRealValue);
  mTextReadout->SetStr(FREQUENCY_COUNTER_STRINGS[idx].data());

  if (!preventAction && GetParam())
  {
    SetValue(GetParam()->ToNormalized(idx));
  }

  SetDirty(!preventAction);
}

void BeatsBoxControl::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  // Mousewheel always increases/ decreases by one full step.
  double inc = (d > 0.f ? 1. : -1.) * mLargeIncrement;
  mRealValue += inc;
  OnValueChanged();
}

void BeatsBoxControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  mRealValue -= (double(dY) * mSmallIncrement);
  OnValueChanged();
}

FrequencyPanel::FrequencyPanel(IRECT rect, float GUIWidth, float GUIHeight, IPluginBase* plugin)
  : layout(rect, GUIWidth, GUIHeight)
  , mPlugin(plugin)
{}

double FrequencyPanel::getLFOPhase(double beatPosition, double secondsPlayed)
{
  // Convert from beats to bars.
  beatPosition /= 4;

  // Find the loop mode of the currently active LFO.
  int activeLFOIdx = mPlugin->GetParam(EParams::activeLFOIdx)->Value();
  LFOLoopMode currentLoopMode = static_cast<LFOLoopMode>(mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, mode))->Value());

  if (currentLoopMode == LFOFrequencyTempo)
  {
    // TODO this needs to be checked once checking host playback position is implemented.
    double exponent = mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, LFOParams::freqTempo))->Value();

    // The exponent is shifted by 6, such that exponent = 6 corresponds to 2^0 = 1.
    double speed = pow(2, exponent - 6);
    beatPosition = fmod(beatPosition, 1. / speed);
    return beatPosition * speed;
  }
  else if (currentLoopMode == LFOFrequencySeconds)
  {
    double loopPeriod = mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, LFOParams::freqSeconds))->Value();
    return fmod(secondsPlayed, loopPeriod) / loopPeriod;
  }
  return 0.;
}

// Saves the FrequencyPanel state:
//  - (LFOLoopMode) currentLoopMode
//  counterValue state:
//      - (LFOLoopMode) mode
//      - (double) value
//      for every possible mode
//bool FrequencyPanel::saveState(const clap_ostream_t* stream)
//{
//  if (stream->write(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode))
//    return false;
//  for (auto& value : counterValue)
//  {
//    if (stream->write(stream, &value.first, sizeof(LFOLoopMode)) != sizeof(LFOLoopMode))
//      return false;
//    if (stream->write(stream, &value.second, sizeof(double)) != sizeof(double))
//      return false;
//  }
//  return true;
//}
//
//// Reads the FrequencyPanel state depending on the version the state was saved in.
//bool FrequencyPanel::loadState(const clap_istream_t* stream, int version[3])
//{
//  if (version[0] == 1 && version[1] == 0 && version[2] == 0)
//  {
//    if (stream->read(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode))
//      return false;
//    // In version 1.0.0 two LFOLoopModes are availbale.
//    for (int i = 0; i < 2; i++)
//    {
//      LFOLoopMode mode;
//      double value;
//
//      if (stream->read(stream, &mode, sizeof(LFOLoopMode)) != sizeof(LFOLoopMode))
//        return false;
//      if (stream->read(stream, &value, sizeof(double)) != sizeof(double))
//        return false;
//
//      counterValue[mode] = value;
//    }
//  }
//  return true;
//}

LFOSelectorControl::LFOSelectorControl(IRECT rect)
  : IControl(rect, EParams::activeLFOIdx)
{
  activeRect = rect;
}

void LFOSelectorControl::setActive(int idx, bool active)
{
  linkActive[idx] = active;
}

const void LFOSelectorControl::getActiveLinks(bool(&isActive)[MAX_MODULATION_LINKS])
{
  for (int i = 0; i < MAX_MODULATION_LINKS; i++)
  {
    isActive[i] = linkActive[activeLFOIdx * MAX_MODULATION_LINKS + i];
  }
}

void LFOSelectorControl::Draw(IGraphics& g)
{
  // Divide area in a horizontal segment for each LFO.
  float h = mRECT.H() / numberLFOs;

  // Draw a variation of the 3D frame that connects with the frame around the editor.
  // TODO replace with actual value.
  float fw = 10;

  // Fill the segment corresponding to the active LFO orange.
  activeRect.T = mRECT.T + activeLFOIdx * h;
  activeRect.B = mRECT.T + (activeLFOIdx + 1) * h;
  g.FillRect(UDS_ORANGE, activeRect.GetOffset(fw, fw, 0, -fw));

  // Define vertices of one trapezoid and two parallelograms that define segments of the frame
  // that will be shaded.
  float topX[4] = {activeRect.L, activeRect.L + fw, activeRect.R, activeRect.R - fw};
  float topY[4] = {activeRect.T, activeRect.T + fw, activeRect.T + fw, activeRect.T};
  float leftX[4] = {activeRect.L, activeRect.L + fw, activeRect.L + fw, activeRect.L};
  float leftY[4] = {activeRect.T, activeRect.T + fw, activeRect.B - fw, activeRect.B};
  float bottomX[4] = {activeRect.L, activeRect.L + fw, activeRect.R, activeRect.R - fw};
  float bottomY[4] = {activeRect.B, activeRect.B - fw, activeRect.B - fw, activeRect.B};

  IColor topColor = IColor::LinearInterpolateBetween(UDS_GREY, UDS_BLACK, ALPHA_3DFRAME_DARK2);
  IColor leftColor = IColor::LinearInterpolateBetween(UDS_GREY, UDS_WHITE, ALPHA_3DFRAME_LIGHT1);
  IColor bottomColor = IColor::LinearInterpolateBetween(UDS_GREY, UDS_WHITE, ALPHA_3DFRAME_LIGHT2);

  g.FillConvexPolygon(topColor, topX, topY, 4);
  g.FillConvexPolygon(leftColor, leftX, leftY, 4);
  g.FillConvexPolygon(bottomColor, bottomX, bottomY, 4);

  // Draw LFO numbers.
  IRECT rect = activeRect;
  for (int i = 0; i < numberLFOs; i++)
  {
    rect.T = mRECT.T + i * h;
    rect.B = mRECT.T + (i + 1) * h;
    g.DrawText(IText(14, UDS_WHITE), std::to_string(i).data(), rect);
  }
}

void LFOSelectorControl::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  // I dont think this should be necessary but it is.
  if (!mRECT.Contains(x, y)) return;

  // Find the horizontal segment under the cursor and the corresponding
  // LFO. Set it as active LFO.
  if (mod.L)
  {
    y -= mRECT.T;
    activeLFOIdx = static_cast<int>(y / (mRECT.H() / numberLFOs));
    SetValue(GetParam()->ToNormalized(activeLFOIdx));
    SetDirty(true);
    isDragging = true;
  }
}

void LFOSelectorControl::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  if (isDragging)
  {
    isDragging = false;

    // Send mouse position and target index packaged as LFOConnectInfo to delegate.
    connectInfo.x = x;
    connectInfo.y = y;

    // Find the next unconnected link for this LFO and return if no link left.
    int idx = -1;
    for (int i = activeLFOIdx * MAX_MODULATION_LINKS; i < (activeLFOIdx + 1) * MAX_MODULATION_LINKS; i++)
    {
      if (!linkActive[i])
      {
        idx = i;
        break;
      }
    }

    if (idx == -1)
    {
      return;
    }

    connectInfo.idx = idx;

    // Send a message to the plugin, including details about the attempted connection.
    if (IGraphics* ui = GetUI())
    {
      ui->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOAttemptConnect, GetTag(), sizeof(connectInfo), &connectInfo);
    }
  }
}

LinkKnobVisualLayer::LinkKnobVisualLayer(IRECT rect, int paramIdx)
  : IVKnobControl(rect, paramIdx)
{}

LinkKnobInputLayer::LinkKnobInputLayer(IRECT rect, int visualLayerTag, int knobIdx)
: IControl(rect, nullptr)
, visTag(visualLayerTag)
, kIdx(knobIdx)
{
  menu.AddItem("Remove", 0);
}

// Do not draw anything, the layer below does the graphics.
void LinkKnobInputLayer::Draw(IGraphics& g)
{}

void LinkKnobInputLayer::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  if (mod.R)
  {
    GetUI()->CreatePopupMenu(*this, menu, IRECT(x, y, x, y));
  }
  else if (mod.L)
  {
    GetUI()->GetControlWithTag(visTag)->OnMouseDown(x, y, mod);
  }
}

void LinkKnobInputLayer::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  GetUI()->GetControlWithTag(visTag)->OnMouseUp(x, y, mod);
}

void LinkKnobInputLayer::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  GetUI()->GetControlWithTag(visTag)->OnMouseDrag(x, y, dX, dY, mod);
}

void LinkKnobInputLayer::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  GetUI()->GetControlWithTag(visTag)->OnMouseWheel(x, y, mod, d);
}

void LinkKnobInputLayer::OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx)
{
  if (pSelectedMenu == &menu)
  {
    GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFODisconnect, GetTag(), sizeof(kIdx), &kIdx);
  }
}

void FrequencyPanel::attachUI(IGraphics* pGraphics)
{
  assert(!layout.fullRect.Empty());

  // Find the loop mode and beat value of the current LFO to initialize IControls with correct parameters.
  int activeLFOIdx = mPlugin->GetParam(EParams::activeLFOIdx)->Value();
  LFOLoopMode currentLoopMode = static_cast<LFOLoopMode>(mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, mode))->Value());
  int beatIdx = static_cast<int>(mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, LFOParams::freqTempo))->Value());

  // Several controls are attached, only two are active at the same time. The currentLoopMode defines which control is needed.
  pGraphics->AttachControl(new ICaptionControl(layout.modeButtonRect, getLFOParameterIndex(activeLFOIdx, mode), IText(24.f), DEFAULT_FGCOLOR, false), FPModeControlTag, "LFOControls");
  pGraphics->AttachControl(new BeatsBoxControl(layout.counterRect, getLFOParameterIndex(activeLFOIdx, freqTempo), beatIdx), FPBeatsControlTag, "LFOControls");
  pGraphics->AttachControl(new SecondsBoxControl(layout.counterRect, getLFOParameterIndex(activeLFOIdx, freqSeconds)), FPSecondsControlTag, "LFOControls");

  // Disable all controls that are not needed.
  if (currentLoopMode != LFOFrequencySeconds)
  {
    pGraphics->GetControlWithTag(FPSecondsControlTag)->SetDisabled(true);
  }
  if (currentLoopMode != LFOFrequencyTempo)
  {
    pGraphics->GetControlWithTag(FPBeatsControlTag)->SetDisabled(true);
  }
}

void FrequencyPanel::setDisabled(bool disable)
{
  if (IGraphics* ui = mPlugin->GetUI())
  {
    ui->GetControlWithTag(FPModeControlTag)->SetDisabled(disable);
    ui->GetControlWithTag(FPBeatsControlTag)->SetDisabled(disable);
    ui->GetControlWithTag(FPSecondsControlTag)->SetDisabled(disable);
  }
}

void FrequencyPanel::setLFOIdx(int idx)
{
  // Set parameter indices. This affects to which LFO the controls are connected.
  if (IGraphics* ui = mPlugin->GetUI())
  {
    ui->GetControlWithTag(FPModeControlTag)->SetParamIdx(getLFOParameterIndex(idx, mode));
    ui->GetControlWithTag(FPBeatsControlTag)->SetParamIdx(getLFOParameterIndex(idx, freqTempo));
    ui->GetControlWithTag(FPSecondsControlTag)->SetParamIdx(getLFOParameterIndex(idx, freqSeconds));
  }
}

void FrequencyPanel::setLoopMode(LFOLoopMode mode)
{
  // Disable all elements.
  setDisabled(true);

  // Enable needed elements:
  // mode control and the control corresponding to the current modes frequency.
  if (IGraphics* ui = mPlugin->GetUI())
  {
    ui->GetControlWithTag(FPModeControlTag)->SetDisabled(false);

    if (mode == LFOFrequencySeconds)
    {
      ui->GetControlWithTag(FPSecondsControlTag)->SetDisabled(false);
    }
    else if (mode == LFOFrequencyTempo)
    {
      ui->GetControlWithTag(FPBeatsControlTag)->SetDisabled(false);
    }
  }
}

LFOController::LFOController(IRECT rect, float GUIWidth, float GUIHeight, IPluginBase* plugin)
  : layout(rect, GUIWidth, GUIHeight)
  , frequencyPanel(layout.toolsRect, GUIWidth, GUIHeight, plugin)
  , mPlugin(plugin)
{
  // Create editors.
  editors.reserve(MAX_NUMBER_LFOS);
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    // Assign the index -1 to ShapeEditors that act as LFO editors.
    editors.emplace_back(layout.editorFullRect, GUIWidth, GUIHeight, -1);
  }
}

void LFOController::attachUI(IGraphics* pGraphics)
{
  frequencyPanel.attachUI(pGraphics);

  pGraphics->AttachControl(new LFOSelectorControl(layout.selectorRect), LFOSelectorControlTag);

  // Editor background.
  pGraphics->AttachControl(new FillRectangle(layout.editorRect, UDS_ORANGE));
  pGraphics->AttachControl(new DrawFrame(layout.editorInnerRect, UDS_WHITE, FRAME_WIDTH_EDITOR));

  // Do not call attachUI on the ShapeEditors, but use a single ShapeEditorControl for all editors.
  pGraphics->AttachControl(new ShapeEditorControl(layout.editorRect, layout.editorInnerRect, nullptr, 256), LFOEditorControlTag);

  // Attach knobs that control the modulation amounts.
  for (int i = 0; i < MAX_MODULATION_LINKS; i++)
  {
    // Coordinates of this knobs rect.
    float l = layout.knobsInnerRect.L + i * layout.knobsInnerRect.W() / MAX_MODULATION_LINKS;
    float t = layout.knobsInnerRect.T;
    float r = layout.knobsInnerRect.L + (i + 1) * layout.knobsInnerRect.W() / MAX_MODULATION_LINKS;
    float b = layout.knobsInnerRect.B;

    // Connect the knobs to the parameters concerning the active LFO.
    int LFOIdx = static_cast<int>(mPlugin->GetParam(EParams::activeLFOIdx)->Value());
    int paramIdx = EParams::modStart + LFOIdx * MAX_MODULATION_LINKS + i;

    // Attach the two controls of link knob: visual layer first (does the drawing),
    // input layer second (forwards inputs to draw layer, but handles popup menus).
    int visTag = EControlTags::LFOKnobStart + 2 * i;
    int inTag = visTag + 1;
    pGraphics->AttachControl(new LinkKnobVisualLayer(IRECT(l, t, r, b), paramIdx), visTag)->SetDisabled(true);
    pGraphics->AttachControl(new LinkKnobInputLayer(IRECT(l, t, r, b), visTag, i), inTag)->SetDisabled(true);
  }

  // Refresh control members to display the correct state.
  setActiveLFO(mPlugin->GetParam(EParams::activeLFOIdx)->Value());
}

void LFOController::setLoopMode(LFOLoopMode mode)
{
  frequencyPanel.setLoopMode(mode);
}

void LFOController::setActiveLFO(int idx)
{
  activeLFOIdx = idx;

  if (IGraphics* ui = mPlugin->GetUI())
  {
    // Connect the editorControl to the new active LFO editor.
    ShapeEditorControl* editorControl = static_cast<ShapeEditorControl*>(ui->GetControlWithTag(LFOEditorControlTag));
    editorControl->setEditor(editors.data() + idx);

    // Inform the selectorControl about the new index.
    LFOSelectorControl* selectorControl = static_cast<LFOSelectorControl*>(ui->GetControlWithTag(LFOSelectorControlTag));
    selectorControl->activeLFOIdx = idx;

    // Connect the modulation knobs with the new LFO.
    bool linkActive[MAX_MODULATION_LINKS];
    selectorControl->getActiveLinks(linkActive);
    for (int i = 0; i < MAX_MODULATION_LINKS; i++)
    {
      int test = EParams::modStart + idx * MAX_MODULATION_LINKS + i;

      // Update the visual knob layer.
      IControl* knob = ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * i);
      knob->SetParamIdx(EParams::modStart + idx * MAX_MODULATION_LINKS + i);
      knob->SetDisabled(!linkActive[i]);

      // Also enable/disable the input layer.
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * i + 1)->SetDisabled(!linkActive[i]);
    }
  }

  // Connect the frequencyPanel to the parameters of the new active LFO.
  frequencyPanel.setLFOIdx(idx);
}

const void LFOController::getModulationAmplitudes(double beatPosition, double secondsPlayed, double* amplitudes)
{
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    double LFOAmplitude = 0;

    for (int j = 0; j < MAX_MODULATION_LINKS; j++)
    {
      double modAmount = mPlugin->GetParam(EParams::modStart + i * MAX_NUMBER_LFOS + j)->Value();

      // Evaluate this LFO editor only if
      // - it is connected to at least one ModulatedParameter, i.e. any modAmount != 0
      // - it has not been evaluated yet, i.e. LFOAmplitude = 0
      if (modAmount && !LFOAmplitude)
      {
        double phase = frequencyPanel.getLFOPhase(beatPosition, secondsPlayed);
        LFOAmplitude = editors.at(i).forward(phase);
      }
      amplitudes[i * MAX_MODULATION_LINKS + j] = LFOAmplitude * modAmount;
    }
  }
}

void LFOController::setLinkActive(int idx, bool active)
{
  if (IGraphics* ui = mPlugin->GetUI())
  {
    LFOSelectorControl* selector = static_cast<LFOSelectorControl*>(ui->GetControlWithTag(EControlTags::LFOSelectorControlTag));
    selector->setActive(idx, active);

    // Enable/ Disable the knob corresponding to the given link.
    // Knobs only control the parameters of the current LFO.
    int shiftedIdx = idx - activeLFOIdx * MAX_MODULATION_LINKS;
    if (0 <= shiftedIdx && shiftedIdx < MAX_MODULATION_LINKS)
    {
      // Enable visual and input layer of this knob.
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * shiftedIdx)->SetDisabled(!active);
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * shiftedIdx + 1)->SetDisabled(!active);
    }
  }
}