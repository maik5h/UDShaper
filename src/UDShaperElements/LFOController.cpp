
#include "LFOController.h"

SecondsBoxControl::SecondsBoxControl(IRECT rect, int parameterIdx)
  : IVNumberBoxControl(rect, parameterIdx, nullptr, "", DEFAULT_STYLE, false, 1., 0.05, 60., "%.2f s", false)
{
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

void SecondsBoxControl::Draw(IGraphics& g)
{
  if (!IsDisabled())
    g.FillRect(GetColor(kHL), mTextReadout->GetRECT());

  if (mMouseIsDown)
    g.FillRect(GetColor(kFG), mTextReadout->GetRECT());
}

void SecondsBoxControl::OnMouseDblClick(float x, float y, const IMouseMod& mod) {}

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

  if (outValue > 0.95)
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

void BeatsBoxControl::Draw(IGraphics& g)
{
  if (!IsDisabled())
    g.FillRect(GetColor(kHL), mTextReadout->GetRECT());

  if (mMouseIsDown)
    g.FillRect(GetColor(kFG), mTextReadout->GetRECT());
}

void BeatsBoxControl::OnMouseDblClick(float x, float y, const IMouseMod& mod)
{}

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

double FrequencyPanel::getLFOPhase(int LFOIdx, double beatPosition, double secondsPlayed) const
{
  // Convert from beats to bars.
  beatPosition /= 4;

  if (currentMode[LFOIdx] == LFOFrequencyTempo)
  {
    // freqTempo is given in terms of powers of two.
    // The exponent is shifted by 6, such that exponent = 6 corresponds to 2^0 = 1.
    double speed = pow(2, freqTempo[LFOIdx] - 6);
    beatPosition = fmod(beatPosition, 1. / speed);
    return beatPosition * speed;
  }
  else if (currentMode[LFOIdx] == LFOFrequencySeconds)
  {
    return fmod(secondsPlayed, freqSeconds[LFOIdx]) / freqSeconds[LFOIdx];
  }
  return 0.;
}

void FrequencyPanel::refreshInternalState()
{
  // Copy the parameter values concerning the FrequencyPanel into the arrays.
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    double mode = mPlugin->GetParam(EParams::LFOsStart + i * LFOParams::kNumLFOParams + LFOParams::mode)->Value();
    currentMode[i] = static_cast<LFOLoopMode>(mode);

    freqTempo[i] = mPlugin->GetParam(EParams::LFOsStart + i * LFOParams::kNumLFOParams + LFOParams::freqTempo)->Value();
    freqSeconds[i] = mPlugin->GetParam(EParams::LFOsStart + i * LFOParams::kNumLFOParams + LFOParams::freqSeconds)->Value();
  }
}

LFOSelectorControl::LFOSelectorControl(IRECT rect, bool (&linkActive)[MAX_NUMBER_LFOS][MAX_MODULATION_LINKS])
  : IControl(rect, EParams::activeLFOIdx)
  , linkActive(linkActive)
{
  activeRect = rect;
}

void LFOSelectorControl::Draw(IGraphics& g)
{
  // Divide area in a horizontal segment for each LFO.
  float h = mRECT.H() / numberLFOs;

  // Draw a variation of the 3D frame that connects with the frame around the editor.
  float fw = FRAME_WIDTH;

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

    // For the active LFO, draw the number shifted to the right to give the impression
    // it is positioned lower then the other numbers.
    if (i == activeLFOIdx)
    {
      rect.Offset(fw / 2, 0, fw / 2, 0);
    }
    else if (i == activeLFOIdx + 1)
    {
      rect.Offset(-fw / 2, 0, -fw / 2, 0);
    }

    g.DrawText(IText(UDS_TEXT_SIZE * 0.8, UDS_WHITE), std::to_string(i + 1).data(), rect);

    // Draw a rectangle around the text. Make it 20% of the full rect.
    float offsetW = rect.W() * 0.4;
    float offsetH = rect.H() * 0.4;
    IRECT innerRect = rect;
    innerRect.Offset(offsetW, offsetH, -offsetW, -offsetH);
    g.DrawRect(UDS_WHITE, innerRect, nullptr, FRAME_WIDTH_NARROW * 0.4);
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
    mouseOver = true;
  }
}

void LFOSelectorControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  // If the mouse exits the control area while dragging, send a message to ShapeEditors to highlight
  // possible modulation targets.
  if (mouseOver && !mRECT.Contains(x, y))
  {
    GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOConnectInit, GetTag(), sizeof(activeLFOIdx), &activeLFOIdx);
    mouseOver = false;
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
    for (int i = 0; i < MAX_MODULATION_LINKS; i++)
    {
      if (!linkActive[activeLFOIdx][i])
      {
        idx = i + activeLFOIdx * MAX_MODULATION_LINKS;
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
      ui->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOConnectAttempt, GetTag(), sizeof(connectInfo), &connectInfo);
    }
  }
}

LinkKnobVisualLayer::LinkKnobVisualLayer(IRECT rect, int paramIdx)
  : IVKnobControl(rect, paramIdx)
{
  // I do not want to display the label. By default, IVKnobControl
  // reserves space for the label and scales down the widget.
  // Setting mLabelInWidget true preserves the full widget size.
  mLabelInWidget = true;
}

void LinkKnobVisualLayer::Draw(IGraphics& g)
{
  // Same as in IVKnobControl, except DrawLabel(g) has been removed.
  DrawBackground(g, mRECT);
  DrawWidget(g);
  DrawValue(g, mValueMouseOver);
}

LinkKnobInputLayer::LinkKnobInputLayer(IRECT rect, int visualLayerTag)
: IControl(rect, nullptr)
, visTag(visualLayerTag)
, modIdx(-1)
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
    // Find the index of the modulation link currently associated with this knob.
    // The input layer is not actually connected to this parameter, the index has
    // to be obtained from the visual layer.
    modIdx = GetUI()->GetControlWithTag(visTag)->GetParamIdx() - EParams::modStart;
    GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFODisconnect, GetTag(), sizeof(modIdx), &modIdx);
  }
}

void LinkKnobInputLayer::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  // Find the index of the modulation link currently associated with this knob.
  // The input layer is not actually connected to this parameter, the index has
  // to be obtained from the visual layer.
  modIdx = GetUI()->GetControlWithTag(visTag)->GetParamIdx() - EParams::modStart;
  GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::linkKnobMouseOver, GetTag(), sizeof(modIdx), &modIdx);
}

void LinkKnobInputLayer::OnMouseOut()
{
    GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::linkKnobMouseOut, GetTag());
}

void FrequencyPanel::attachUI(IGraphics* pGraphics)
{
  assert(!layout.fullRect.Empty());

  // Find the loop mode and beat value of the current LFO to initialize IControls with correct parameters.
  int activeLFOIdx = mPlugin->GetParam(EParams::activeLFOIdx)->Value();
  LFOLoopMode currentLoopMode = static_cast<LFOLoopMode>(mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, mode))->Value());
  int beatIdx = static_cast<int>(mPlugin->GetParam(getLFOParameterIndex(activeLFOIdx, LFOParams::freqTempo))->Value());

  // Several controls are attached, only two are active at the same time. The currentLoopMode defines which control is needed.
  pGraphics->AttachControl(new ICaptionControl(layout.modeButtonRect, getLFOParameterIndex(activeLFOIdx, mode), IText(UDS_TEXT_SIZE), DEFAULT_FGCOLOR, false), FPModeControlTag, "LFOControls");
  pGraphics->AttachControl(new BeatsBoxControl(layout.counterRect, getLFOParameterIndex(activeLFOIdx, LFOParams::freqTempo), beatIdx), FPBeatsControlTag, "LFOControls");
  pGraphics->AttachControl(new SecondsBoxControl(layout.counterRect, getLFOParameterIndex(activeLFOIdx, LFOParams::freqSeconds)), FPSecondsControlTag, "LFOControls");

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
    ui->GetControlWithTag(FPModeControlTag)->SetParamIdx(getLFOParameterIndex(idx, LFOParams::mode));
    ui->GetControlWithTag(FPBeatsControlTag)->SetParamIdx(getLFOParameterIndex(idx, LFOParams::freqTempo));
    ui->GetControlWithTag(FPSecondsControlTag)->SetParamIdx(getLFOParameterIndex(idx, LFOParams::freqSeconds));
  }
}

void FrequencyPanel::setLoopMode(LFOLoopMode mode)
{
  currentMode[activeLFOIdx] = mode;

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

void FrequencyPanel::setFrequencyValue(LFOLoopMode mode, double value)
{
  if (mode == LFOLoopMode::LFOFrequencyTempo)
  {
    freqTempo[activeLFOIdx] = value;
  }
  else if (mode == LFOLoopMode::LFOFrequencySeconds)
  {
    freqSeconds[activeLFOIdx] = value;
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

  pGraphics->AttachControl(new LFOSelectorControl(layout.selectorRect, linkActive), LFOSelectorControlTag);

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
    pGraphics->AttachControl(new LinkKnobInputLayer(IRECT(l, t, r, b), visTag), inTag)->SetDisabled(true);
  }

  // Refresh control members to display the correct state.
  setActiveLFO(mPlugin->GetParam(EParams::activeLFOIdx)->Value());

  // Find the last LFO that modulates a parameter and display one more.
  for (int LFOIdx = MAX_NUMBER_LFOS - 1; LFOIdx >= 0; LFOIdx--)
  {
    for (int linkIdx = 0; linkIdx < MAX_MODULATION_LINKS; linkIdx++)
    {
      if (linkActive[LFOIdx][linkIdx])
      {
        // The index of the last connected LFO plus two, i.e. the number of
        // displayed LFOs.
        LFOIdx = (LFOIdx > MAX_NUMBER_LFOS) ? MAX_NUMBER_LFOS : LFOIdx;
        LFOIdx = (LFOIdx < MIN_NUMBER_LFOS) ? MIN_NUMBER_LFOS : LFOIdx;
        auto selectorControl = static_cast<LFOSelectorControl*>(pGraphics->GetControlWithTag(EControlTags::LFOSelectorControlTag));
        selectorControl->numberLFOs = LFOIdx;
        break;
      }
    }
  }
}

void LFOController::setLoopMode(LFOLoopMode mode)
{
  frequencyPanel.setLoopMode(mode);
}

void LFOController::setFrequencyValue(LFOLoopMode mode, double value)
{
  frequencyPanel.setFrequencyValue(mode, value);
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

    frequencyPanel.activeLFOIdx = idx;

    // Connect the modulation knobs with the new LFO.
    for (int i = 0; i < MAX_MODULATION_LINKS; i++)
    {
      // Update the visual knob layer.
      IControl* knob = ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * i);
      knob->SetParamIdx(EParams::modStart + idx * MAX_MODULATION_LINKS + i);
      knob->SetDisabled(!linkActive[activeLFOIdx][i]);

      // Also enable/disable the input layer.
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * i + 1)->SetDisabled(!linkActive[activeLFOIdx][i]);
    }
  }

  // Connect the frequencyPanel to the parameters of the new active LFO.
  frequencyPanel.setLFOIdx(idx);
}

void LFOController::refreshInternalState()
{
  frequencyPanel.refreshInternalState();
}

void LFOController::getModulationAmplitudes(double beatPosition, double secondsPlayed, double* amplitudes, double* factors) const
{
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    double LFOAmplitude = 0;

    for (int j = 0; j < MAX_MODULATION_LINKS; j++)
    {
      double modAmount = factors[i * MAX_NUMBER_LFOS + j];

      // Evaluate this LFO editor only if
      // - it is connected to at least one ModulatedParameter, i.e. any modAmount != 0
      // - it has not been evaluated yet, i.e. LFOAmplitude = 0
      if (modAmount && !LFOAmplitude)
      {
        double phase = frequencyPanel.getLFOPhase(i, beatPosition, secondsPlayed);
        LFOAmplitude = editors.at(i).forward(phase);
      }
      amplitudes[i * MAX_MODULATION_LINKS + j] = LFOAmplitude * modAmount;
    }
  }
}

void LFOController::setLinkActive(int idx, bool active)
{
  int linkIdx = idx % MAX_MODULATION_LINKS;
  int LFOIdx = (idx - linkIdx) / MAX_MODULATION_LINKS;
  linkActive[LFOIdx][linkIdx] = active;

  if (IGraphics* ui = mPlugin->GetUI())
  {
    // Enable/ Disable the knob corresponding to the given link.
    // Knobs only control the parameters of the current LFO.
    if (0 <= linkIdx && linkIdx < MAX_MODULATION_LINKS)
    {
      // Enable visual and input layer of this knob.
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * linkIdx)->SetDisabled(!active);
      ui->GetControlWithTag(EControlTags::LFOKnobStart + 2 * linkIdx + 1)->SetDisabled(!active);
    }

    // Update the number of displayed LFOs, such that there is always one
    // unconnected LFO at the end.
    IControl* control = mPlugin->GetUI()->GetControlWithTag(EControlTags::LFOSelectorControlTag);
    LFOSelectorControl* selectorControl = static_cast<LFOSelectorControl*>(control);
    // If the last LFO has been connected, display a new one.
    if (active)
    {
      if ((LFOIdx == selectorControl->numberLFOs - 1) && (selectorControl->numberLFOs < MAX_NUMBER_LFOS))
      {
        selectorControl->numberLFOs++;
      }
    }
    // If the last LFOs are not connected to any parameter, all but one can be hidden.
    else if (selectorControl->numberLFOs > MIN_NUMBER_LFOS)
    {
      int lastConnected = -1;
      for (int i = MAX_NUMBER_LFOS - 1; i >= 0; i--)
      {
        for (int j = 0; j < MAX_MODULATION_LINKS; j++)
        {
          if (linkActive[i][j])
          {
            lastConnected = i;
            break;
          }
        }
        if (lastConnected != -1)
        {
          break;
        }
      }
      // The last displayed LFO indedx is lastConnected + 1, the total number
      // of displayed LFOs is lastConnected + 2.
      int numLFOs = lastConnected + 2;
      numLFOs = (numLFOs <= MIN_NUMBER_LFOS) ? MIN_NUMBER_LFOS : numLFOs;
      selectorControl->numberLFOs = numLFOs;
    }
  }
}

// Serializes the LFOController state.
// All LFO editor states are saved (same method as for base ShapeEditor).
// FrequencyPanels have no internal state, only parameters.
bool LFOController::serializeState(IByteChunk& chunk) const
{
  int numberLFOs = editors.size();
  chunk.Put(&numberLFOs);

  for (auto& editor : editors)
  {
    editor.serializeState(chunk);
  }
  return true;
}

// Unserialize the LFOController state.
//
// The first value to be loaded must be the number of LFOs that have been saved.
// Creates new LFO editors if necessary and calls unserializeState for every
// editor that has to be loaded.
// * @return The new chunk position
int LFOController::unserializeState(const IByteChunk& chunk, int startPos, int version)
{
  if (version == 0x00000000)
  {
    // The number of LFOs in the loaded state.
    int num = 0;
    startPos = chunk.Get(&num, startPos);

    // If more LFOs are loaded than available, add new editors.
    // If less are loaded, just keep the excess editors in the default
    // state.
    while (editors.size() < num)
    {
      editors.emplace_back(layout.editorFullRect, layout.GUIWidth, layout.GUIHeight, -1);
    }

    for (int i = 0; i < num; i++)
    {
      startPos = editors.at(i).unserializeState(chunk, startPos, version);
    }
    return startPos;
  }
}