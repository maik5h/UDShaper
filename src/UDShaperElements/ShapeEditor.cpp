#include "ShapeEditor.h"


// Calculate the power such that the function f: [0, 1] -> [0, 1], f(x) = x^power
// satisfies the following equations:
//  f(0.5) = posY           if posY <= 0.5
//  f(0.5) = 1 - posY       if posY > 0.5
//
// In the second case, -power is returned instead of power. It does NOT mean the curve
// is actually f(x) = 1 / (x^|power|) and only indicates the input was > 0.5.
//
// Input must be > 0 and < 1 or else the power will be inf.
static float getPowerFromPosY(float posY)
{
  float power = (posY < 0.5) ? log(posY) / log(0.5) : log(1 - posY) / log(0.5);

  return (posY > 0.5) ? -power : power;
}

ModulatedParameter::ModulatedParameter(float inBase, float inMinValue, float inMaxValue)
  : base(inBase)
  , minValue(inMinValue)
  , maxValue(inMaxValue)
{}

bool ModulatedParameter::addModulator(int idx)
{
  int LFOIdx = (idx - idx % MAX_MODULATION_LINKS) / MAX_MODULATION_LINKS;
  if (!LFOIsConnected[LFOIdx])
  {
    LFOIsConnected[LFOIdx] = true;
    modIndices.insert(idx);
    return true;
  }
  else
  {
    return false;
  }
}

void ModulatedParameter::getModulators(std::set<int>& mods) const
{
  for (int mod : modIndices)
  {
    mods.insert(mod);
  }
}

bool ModulatedParameter::isConnectedToLFO(int LFOIdx) const
{
  return LFOIsConnected[LFOIdx];
}

bool ModulatedParameter::isConnectedToMod(int modIdx) const
{
  return (modIndices.find(modIdx) != modIndices.end());
}

void ModulatedParameter::removeModulator(int idx)
{
  int LFOIdx = (idx - idx % MAX_MODULATION_LINKS) / MAX_MODULATION_LINKS;
  if (modIndices.erase(idx))
  {
    LFOIsConnected[LFOIdx] = false;
  }
}

void ModulatedParameter::set(float newValue)
{
  base = (newValue < minValue) ? minValue : newValue;
  base = (newValue > maxValue) ? maxValue : newValue;
}

float ModulatedParameter::get(double* modulationAmplitudes) const
{
  float currentValue = base;
  if (modulationAmplitudes)
  {
    for (int idx : modIndices)
    {
      currentValue += modulationAmplitudes[idx];
    }
  }

  currentValue = (currentValue > maxValue) ? maxValue : (currentValue < minValue) ? minValue : currentValue;
  return currentValue;
}

bool ModulatedParameter::isModulated() const
{
  return modIndices.size() > 0;
}

void ModulatedParameter::serializeState(IByteChunk& chunk) const
{
  chunk.Put(&base);

  int numLinks = modIndices.size();
  chunk.Put(&numLinks);
  for (int idx : modIndices)
  {
    chunk.Put(&idx);
  }
}

int ModulatedParameter::unserializeState(const IByteChunk& chunk, int startPos, int version)
{
  startPos = chunk.Get(&base, startPos);
  int numLinks = 0;
  startPos = chunk.Get(&numLinks, startPos);

  int linkIdx = 0;
  for (int i = 0; i < numLinks; i++)
  {
    startPos = chunk.Get(&linkIdx, startPos);
    addModulator(linkIdx);
  }
  return startPos;
}

ShapePoint::ShapePoint(float x, float y, IRECT editorRect, float pow, float omega, Shapes initMode)
: posX(x)
, posY(y)
, curveCenterPosY(std::pow(0.5, pow), MIN_CURVE_CENTER, 1 - MIN_CURVE_CENTER)
{
  assert((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));

  rect = editorRect;
  mode = initMode;
  sineOmega = omega;
  sineOmegaPrevious = omega;
}

void ShapePoint::updatePositionAbsolute(float x, float y)
{
  // Move the modulatable relative parameters to the corresponding positions.
  posX.set((x - rect.L) / rect.W());
  posY.set((rect.B - y) / rect.H());
}

float ShapePoint::getPosX(double* modulationAmplitudes, float lowerBound, float upperBound) const
{
  float x = posX.get(modulationAmplitudes);
  x = (x < lowerBound) ? lowerBound : x;
  return (x > upperBound) ? upperBound : x;
}

float ShapePoint::getPosY(double* modulationAmplitudes) const
{
  return posY.get(modulationAmplitudes);
}

float ShapePoint::getAbsPosX(double* modulationAmplitudes, float lowerBound, float upperBound) const
{
  return rect.L + getPosX(modulationAmplitudes, lowerBound, upperBound) * rect.W();
}

float ShapePoint::getAbsPosY(double* modulationAmplitudes) const
{
  // By convention, absolute position is inversely related to posY.
  return rect.B - getPosY(modulationAmplitudes) * rect.H();
}

float ShapePoint::getCurveCenterAbsPosY(float previousY, double* modulationAmplitudes) const
{
  float y = getAbsPosY(modulationAmplitudes);
  float curveCenterY = curveCenterPosY.get(modulationAmplitudes);
  float yExtent = y - previousY;
  return previousY + curveCenterY * yExtent;
}

void ShapePoint::updateCurveCenter(float y, float previousY)
{
  // Nothing to update if curve segment is flat.
  if (previousY == getAbsPosY()) return;

  switch (mode)
  {
  case shapePower: {
    // Find the offset from the last clicked position normalized to the y-range of this segment.
    y = (y - previousY) / (getAbsPosY() - previousY) - centerYClicked;

    // To smooth the transition between 0 and 1, a sigmoid function is applied to the offset.
    // First, reverse transform the current position to provide a starting point for the offset.
    float yCenterTransformed = 2 * std::atanh(2 * centerYClicked - 1);

    // Combine the starting point with the offset.
    // The factor of six amplifies the sensitivity to user inputs. With this value, the curve
    // center is moved slightly faster than the cursor if close to y=0.5.
    float combinedOffset = yCenterTransformed + 6 * y;

    // Apply sigmoid.
    float yCenterNew = 0.5f * (1 + std::tanh(combinedOffset / 2));
    curveCenterPosY.set(yCenterNew);
    break;
  }

  case shapeSine: {
    // For shapeSine the center point stays at the same position, while the sineOmega value is
    // still updated when moving the mouse in y-direction. Sine wave will always go through this
    // point. When omega is smaller 0.5, the sine will smoothly connect the points, if larger 0.5,
    // it will be increased in discrete steps, such that always n + 1/2 cycles lay in the interval
    // between the points.

    // TODO fix this.
    // if (sineOmega <= 0.5){
    //     sineOmega = sineOmegaPrevious * pow(0.5, (float)(y - getCurveCenterAbsPosY()) / 50);
    //     sineOmega = (sineOmega < SHAPE_MIN_OMEGA) ? SHAPE_MIN_OMEGA : sineOmega;
    // } else {
    //     // TODO if shift or strg or smth is pressed, be continuous
    //     sineOmega = sineOmegaPrevious + ((getCurveCenterAbsPosY() - y) / 40);
    //     sineOmega -= (int32_t)sineOmega % 2;
    // }
    break;
  }
  }
}

void ShapePoint::processLeftClick()
{
  centerYClicked = curveCenterPosY.get(nullptr);

  // Save the previous omega state.
  sineOmegaPrevious = sineOmega;
}

// Serializes the ShapePoint state.
void ShapePoint::serializeState(IByteChunk& chunk) const
{
  chunk.Put(&mode);
  posX.serializeState(chunk);
  posY.serializeState(chunk);
  curveCenterPosY.serializeState(chunk);
  chunk.Put(&sineOmega);
}

// Unserialize the ShapePoint state.
// * @return The new chunk position
int ShapePoint::unserializeState(const IByteChunk& chunk, int startPos, int version)
{
  if (version == 0x00000000)
  {
    startPos = chunk.Get(&mode, startPos);
    startPos = posX.unserializeState(chunk, startPos, version);
    startPos = posY.unserializeState(chunk, startPos, version);
    startPos = curveCenterPosY.unserializeState(chunk, startPos, version);
    startPos = chunk.Get(&sineOmega, startPos);
  }
  return startPos;
}

ShapeEditor::ShapeEditor(IRECT rect, float GUIWidth, float GUIHeight, int shapeEditorIndex)
  : index(shapeEditorIndex)
  , layout(rect, GUIWidth, GUIHeight)
{
  // There is a special ShapePoint at x=0, y=0, which can not be moved and is not displayed
  // to assure f(0) = 0. The last ShapePoint at x=1 may be moved, but only in y-direction.
  // None of these points can be removed to assure that the function is always well defined
  // on the interval [0, 1].
  shapePoints.emplace_back(0.f, 0.f, layout.editorRect);
  shapePoints.emplace_back(1.f, 1.f, layout.editorRect);
}

int ShapeEditor::getClosestPoint(float x, float y, float minimumDistance)
{
  // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.

  // TODO i do not like the implementation using the internal currentEditMode to communicate if point
  // or curve center was closer at all. Maybe this can be replaced by an optional argument of a pointer to a
  // editMode object.

  // The distance to the closest point, initialized as an arbitrary big number.
  float closestDistance = 10E6;

  // Index of the closest ShapePoint.
  int closestPointIdx = -1;

  // Distance to the closest point.
  float distance;

  for (int i = 1; i < shapePoints.size(); i++)
  {
    ShapePoint point = shapePoints.at(i);
    distance = (point.getAbsPosX() - x) * (point.getAbsPosX() - x) + (point.getAbsPosY() - y) * (point.getAbsPosY() - y);
    if (distance < closestDistance)
    {
      closestDistance = distance;
      closestPointIdx = i;
      currentEditMode = position;
    }

    float curveCenterPosX = (point.getAbsPosX() + shapePoints.at(i - 1).getAbsPosX()) / 2;
    float previousY = shapePoints.at(i - 1).getAbsPosY();
    distance = (curveCenterPosX - x) * (curveCenterPosX - x) + (point.getCurveCenterAbsPosY(previousY) - y) * (point.getCurveCenterAbsPosY(previousY) - y);
    if (distance < closestDistance)
    {
      closestDistance = distance;
      closestPointIdx = i;
      currentEditMode = curveCenter;
    }
  }

  if (closestDistance <= minimumDistance)
  {
    return closestPointIdx;
  }
  {
    currentEditMode = none;
    return -1;
  }
}

void ShapeEditor::deleteSelectedPoint()
{
  // Do not delete point if it is the last point.
  if (rightClickedIdx < shapePoints.size() - 1)
  {
    std::set<int> modIndices = {};
    shapePoints.at(rightClickedIdx).posX.getModulators(modIndices);
    shapePoints.at(rightClickedIdx).posY.getModulators(modIndices);
    shapePoints.at(rightClickedIdx).curveCenterPosY.getModulators(modIndices);

    deletedLinks.clear();
    for (int idx : modIndices)
    {
      deletedLinks.push_back(idx);
    }

    shapePoints.erase(shapePoints.begin() + rightClickedIdx);
    rightClickedIdx = -1;
  }
}

void ShapeEditor::getDeletedLinks(int& numberLinks, int *&pData)
{
  numberLinks = deletedLinks.size();
  pData = deletedLinks.data();
}

void ShapeEditor::processLeftClick(float x, float y)
{
  currentlyDraggingIdx = getClosestPoint(x, y);

  if (currentlyDraggingIdx != -1)
  {
    shapePoints.at(currentlyDraggingIdx).processLeftClick();
  }
}

void ShapeEditor::processDoubleClick(float x, float y)
{
  int closestPointIdx = getClosestPoint(x, y);

  if (closestPointIdx == -1) return;

  // Reset interpolation shape if double click was performed close to curve center.
  if (currentEditMode == curveCenter)
  {
    if (shapePoints.at(closestPointIdx).mode == shapePower)
    {
      shapePoints.at(closestPointIdx).curveCenterPosY.set(0.5);
    }
    else if (shapePoints.at(closestPointIdx).mode == shapeSine)
    {
      shapePoints.at(closestPointIdx).sineOmega = 0.5;
      shapePoints.at(closestPointIdx).sineOmegaPrevious = 0.5;
    }
    return;
  }
}

bool ShapeEditor::processRightClick(float x, float y)
{
  int closestPointIdx = getClosestPoint(x, y);

  // If not rightclicked in proximity of point, add one.
  if (closestPointIdx == -1)
  {
    // Check if point lies inside the editor.
    if (!layout.editorRect.Contains(x, y)) return false;

    float newPointX = (x - layout.editorRect.L) / layout.editorRect.W();
    float newPointY = (layout.editorRect.B - y) / layout.editorRect.H();

    int insertIdx = insertPointAt(newPointX, newPointY);

    // Set the point as currently dragged point so the user can immediately adjust
    // the position after adding the point.
    currentlyDraggingIdx = insertIdx;
    currentEditMode = position;
  }

  else
  {
    // You can not drag using the right mouse button.
    // Only exception is after creating a new ShapePoint (see above).
    currentlyDraggingIdx = -1;

    // If rightclicked on point, show shape menu to change function of curve segment.
    if (currentEditMode == position)
    {
      rightClickedIdx = closestPointIdx;
      return true;
    }

    // If rightclicked on curve center, reset interpolation shape.
    if (currentEditMode == curveCenter)
    {
      if (shapePoints.at(closestPointIdx).mode == shapePower)
      {
        shapePoints.at(closestPointIdx).curveCenterPosY.set(0.5);
      }
      else if (shapePoints.at(closestPointIdx).mode == shapeSine)
      {
        shapePoints.at(closestPointIdx).sineOmega = 0.5;
        shapePoints.at(closestPointIdx).sineOmegaPrevious = 0.5;
      }
    }
  }
  return false;
}

void ShapeEditor::setInterpolationMode(Shapes shape)
{
  shapePoints.at(rightClickedIdx).mode = shape;
  rightClickedIdx = -1;
}

void ShapeEditor::processMouseDrag(float x, float y)
{
  if (currentlyDraggingIdx == -1) return;

  if (currentEditMode == position)
  {
    // The rightmost point must not move in x-direction.
    if (currentlyDraggingIdx == shapePoints.size() - 1)
    {
      x = layout.editorRect.R;
      y = (y > layout.editorRect.B) ? layout.editorRect.B : (y < layout.editorRect.T) ? layout.editorRect.T : y;
      shapePoints.at(currentlyDraggingIdx).updatePositionAbsolute(x, y);
      return;
    }

    int xLowerLim = shapePoints.at(currentlyDraggingIdx - 1).getAbsPosX();
    int xUpperLim = shapePoints.at(currentlyDraggingIdx + 1).getAbsPosX();

    x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
    y = (y > layout.editorRect.B) ? layout.editorRect.B : (y < layout.editorRect.T) ? layout.editorRect.T : y;

    shapePoints.at(currentlyDraggingIdx).updatePositionAbsolute(x, y);
  }

  else if (currentEditMode == curveCenter)
  {
    float previousY = shapePoints.at(currentlyDraggingIdx - 1).getAbsPosY();
    shapePoints.at(currentlyDraggingIdx).updateCurveCenter(y, previousY);
  }
}

bool ShapeEditor::isDraggingBeyondBounds() const
{
  if (currentEditMode == editMode::curveCenter)
  {
    ShapePoint point = shapePoints.at(currentlyDraggingIdx);
    bool isMin = (point.curveCenterPosY.get(nullptr) == MIN_CURVE_CENTER);
    bool isMax = (point.curveCenterPosY.get(nullptr) == 1 - MIN_CURVE_CENTER);
    return isMin || isMax;
  }
  return false;
}

void ShapeEditor::getMouseReveal(bool& revealCursor, float& x, float& y) const
{
  if ((currentlyDraggingIdx != -1) && (currentEditMode == editMode::curveCenter))
  {
    revealCursor = true;

    // Find curve center x- and y-position.
    float lX = shapePoints.at(currentlyDraggingIdx - 1).getAbsPosX();
    float uX = shapePoints.at(currentlyDraggingIdx).getAbsPosX();
    x = (lX + uX) / 2;

    float lY = shapePoints.at(currentlyDraggingIdx - 1).getAbsPosY();
    y = shapePoints.at(currentlyDraggingIdx).getCurveCenterAbsPosY(lY);
  }
  else
  {
    revealCursor = false;
  }
}

const float ShapeEditor::forward(float input, double* modulationAmplitudes)
{
  // Catching this case is important because the function might return non-zero values for steep curves
  // due to quantization errors, which would result in an DC offset even when no input audio is given.
  if (input == 0) return 0;

  float out;

  // Absolute value of input is processed, information about sign is saved to flip output after computing.
  bool flipOutput = input < 0;
  input = (input < 0) ? -input : input;
  input = (input > 1) ? 1 : input;

  // Index of the ShapePoint that corresponds to the input.
  int idx = 1;

  // Find the curve segment corresponding to the input.
  // Points can be modulated in x-direction, so the curve segment corresponding to
  // the input can change over time.
  // To find the correct segment, a binary search over the unmodulated positions
  // is performed first. This is very fast.
  // Afterwards it is checked if the selected point and neighboring points are
  // modulated along the x-direction. If they are, their modulated position is
  // determined to calculate the correct shape of the graph. This is slow.

  // If more than one valid point is in the editor, perform a binary search on the
  // unmodulated x-positions.
  if (shapePoints.size() > 2)
  {
    // Lower bound, upper bound and center of the search interval.
    int lowerIdx = 1;
    int upperIdx = shapePoints.size() - 1;
    int center = static_cast<int>((upperIdx + lowerIdx) / 2);

    // x-extent of the curve segment corresponding to the point at center.
    float lowerX = shapePoints.at(center - 1).posX.get(nullptr);
    float upperX = shapePoints.at(center).posX.get(nullptr);

    // Search until the input lies within the curve segment.
    while ((lowerX >= input) || (upperX < input))
    {
      if (lowerX >= input)
      {
        upperIdx = center - 1;
      }
      else if (upperX < input)
      {
        lowerIdx = center + 1;
      }
      center = static_cast<int>((upperIdx + lowerIdx) / 2);
      lowerX = shapePoints.at(center - 1).posX.get(nullptr);
      upperX = shapePoints.at(center).posX.get(nullptr);
    }
    idx = center;
  }

  // Check if the selected point is x-modulated and if yes, find the next point
  // with larger x that is not.
  int modIdx = idx;
  while (shapePoints.at(modIdx).posX.isModulated())
  {
    modIdx++;
  }
  // Index of the next point at a higher x which position is not
  // modulated in x-direction.
  int upperIdx = modIdx;

  modIdx = idx - 1;
  while (shapePoints.at(modIdx).posX.isModulated())
  {
    modIdx--;
  }
  // Index of the next point at a lower x which position is not
  // modulated in x-direction.
  int lowerIdx = modIdx;

  // Find the curve segment concerned by the input inside the interval of
  // modulated points.
  // It is defined by two ShapePoints. To calculate their x-position,
  // the position of the previous point must be known to provide a
  // lower bound.
  float lowerBoundPrevious = shapePoints.at(lowerIdx).getPosX();
  float lowerBound = lowerBoundPrevious;
  float upperBound = shapePoints.at(upperIdx).getPosX();

  if ((upperIdx - lowerIdx) > 1)
  {
    for (int i = lowerIdx; i <= upperIdx; i++)
    {
      lowerBoundPrevious = lowerBound;
      lowerBound = shapePoints.at(i).getPosX(modulationAmplitudes, lowerBound);

      if (lowerBound >= input)
      {
        idx = i;
        break;
      }
    }
  }

  // Evaluate the curve segment at the input.
  switch (shapePoints.at(idx).mode)
  {
  case shapePower: {
    float xL = shapePoints.at(idx - 1).getPosX(modulationAmplitudes, lowerBoundPrevious, upperBound);
    float yL = shapePoints.at(idx - 1).getPosY(modulationAmplitudes);
    float x = shapePoints.at(idx).getPosX(modulationAmplitudes, lowerBound, upperBound);

    // Compute relative x-position inside the curve segment, relative "height" of the curve segment and power
    // corresponding to the current curve cenetr position.
    float relX = (x == xL) ? xL : (input - xL) / (x - xL);
    float segmentYExtent = (shapePoints.at(idx).getPosY(modulationAmplitudes) - yL);
    float power = getPowerFromPosY(shapePoints.at(idx).curveCenterPosY.get(modulationAmplitudes));

    if (power > 0)
    {
      out = yL + pow(relX, power) * segmentYExtent;
    }
    else
    {
      out = shapePoints.at(idx).getPosY(modulationAmplitudes) - pow(1 - relX, -power) * segmentYExtent;
    }
    break;
  }
  case shapeSine:
    out = 1.;
    break;
  }
  return flipOutput ? -out : out;
}

const void ShapeEditor::attachUI(IGraphics* g) {
  assert(!layout.fullRect.Empty());

  g->AttachControl(new FillRectangle(layout.innerRect, UDS_ORANGE));
  g->AttachControl(new DrawFrame(layout.editorRect, UDS_WHITE, FRAME_WIDTH_EDITOR));

  // TODO maybe dont do that, this might easily break and confuse me in the future if index is not 1 or 2.
  int tag = (index == 1) ? EControlTags::ShapeEditorControl1 : EControlTags::ShapeEditorControl2;
  g->AttachControl(new ShapeEditorControl(layout.innerRect, layout.editorRect, this, 256), tag);
}

void ShapeEditor::disconnectLink(int linkIdx)
{
  // Remove the link from the connected ModulatedParameter by removing it from
  // all ModulatedParameters. This could be improved.
  for ( int i = 1; i < shapePoints.size(); i++)
  {
    shapePoints.at(i).posX.removeModulator(linkIdx);
    shapePoints.at(i).posY.removeModulator(linkIdx);
    shapePoints.at(i).curveCenterPosY.removeModulator(linkIdx);
  }
}

void ShapeEditor::getLinks(std::set<int>& links)
{
  for (int i = 1; i < shapePoints.size(); i++)
  {
    shapePoints.at(i).posX.getModulators(links);
    shapePoints.at(i).posY.getModulators(links);
    shapePoints.at(i).curveCenterPosY.getModulators(links);
  }
}

int ShapeEditor::insertPointAt(float x, float y)
{
  int idx = -1;

  for (int i = 1; i < shapePoints.size(); i++)
  {
    if (shapePoints.at(i).posX.get(nullptr) >= x)
    {
      idx = i;
      break;
    }
  }
  shapePoints.emplace(shapePoints.begin() + idx, x, y, layout.editorRect);
  return idx;
}

// Saves the ShapeEditor state to the IByteChunk object.
// First saves the number of ShapePoints as an int and then saves:
//  - (Shapes)  point interpolation mode
//  - (float)   point x-position and modulation links
//  - (float)   point y-position and modulation links
//  - (float)   point curve center y-position and modulation links
//  - (float)   point omega value
//
// for every ShapePoint. These values describe the ShapeEditor state entirely.
bool ShapeEditor::serializeState(IByteChunk& chunk) const
{
  // The number of points to save. The first point in the vector is always at (0, 0) and does not have to be saved.
  int numberPoints = shapePoints.size() - 1;

  chunk.Put(&numberPoints);

  //for (ShapePoint point : shapePoints)
  for (int i = 0; i < numberPoints; i++)
  {
    shapePoints.at(i + 1).serializeState(chunk);
  }
  return true;
}

// Loads a ShapeEditor state from an IByteChunk.
//
// The first element in the stream must be the number of ShapePoints to be loaded.
// Creates a new ShapePoint and calls unserializeState for every point that has to
// be loaded.
// * @return The new chunk position
int ShapeEditor::unserializeState(const IByteChunk& chunk, int startPos, int version)
{
  if (version == 0x00000000)
  {
    int numberPoints;
    startPos = chunk.Get(&numberPoints, startPos);

    shapePoints.reserve(numberPoints + 1);

    for (int i = 0; i < numberPoints; i++)
    {
      if (i != numberPoints - 1)
      {
        // Add new points unless the last point is reached, which was already created at instantiation
        // of this ShapeEditor.
        shapePoints.emplace(shapePoints.begin() + i + 1, 0.f, 0.f, layout.editorRect);
      }
      startPos = shapePoints.at(i + 1).unserializeState(chunk, startPos, version);
    }
    return startPos;
  }
  else
  {
    // TODO if version is not known one of these things should happen:
    //  - If a preset was loaded, it means probably that the preset was saved from a more
    //    recent version of UDShaper and can therefore not be loaded correctly.
    //    There should be a message to warn the user.
    //  - If it happens from copying/ moving by the host, throw an exception probably?
    return startPos;
  }
}

ShapeEditorControl::ShapeEditorControl(const IRECT& bounds, const IRECT& editorBounds, ShapeEditor* shapeEditor, int numPoints, bool useLayer)
  : IControl(bounds)
  , IVectorBase(DEFAULT_STYLE)
  , mUseLayer(useLayer)
  , editorRect(editorBounds)
{
  editor = shapeEditor;
  mPoints.resize(numPoints);

  AttachIControl(this, "");

  if (mLayer)
    mLayer->Invalidate();
  SetDirty(false);

  menu.AddItem("Delete", 0);
  menu.AddSeparator(1);
  menu.AddItem("Power", 2);
  // TODO add this once sinusoidal interpolation is implemented.
  //menu.AddItem("Sine", 3);
}

void ShapeEditorControl::setEditor(ShapeEditor* newEditor)
{
  editor = newEditor;
  SetDirty(true);
}

void ShapeEditorControl::Draw(IGraphics& g)
{
  if (editor == nullptr) return;

  float hdiv = editorRect.W() / static_cast<float>(mHorizontalDivisions);
  float vdiv = editorRect.H() / static_cast<float>(mVerticalDivisions);

  auto drawFunc = [&]() {
    // TODO Segments between very close points are not rendered
    // + The curve can overshoot points on very steep peaks.
    // Set an additional samplepoint at every ShapePoint to prevent this?

    g.DrawGrid(UDS_WHITE, editorRect, hdiv, vdiv, &gridBlend);

    // Draw the graph of the shaping function.
    for (int i = 0; i < mPoints.size(); i++)
    {
      float v = editor->forward(static_cast<float>(i) / static_cast<float>(mPoints.size() - 1), modulationAmplitudes);
      v = (v - mMin) / (mMax - mMin);
      mPoints.at(i) = v;
    }

    g.DrawData(UDS_WHITE, editorRect, mPoints.data(), static_cast<int>(mPoints.size()), nullptr, &mBlend, mTrackSize);

    // Draw the ShapePoints on top of the graph.
    // The rules for points with modulated x-position are:
    // - modulated points push other modulated points to the right.
    // - modulated points can not move past unmodulated points regardless of direction.
    float lowerBound = 0;
    float lowerBoundPrevious = 0;
    float upperBound = 0;
    for (int i = 1; i < editor->shapePoints.size(); i++)
    {
      ShapePoint point = editor->shapePoints.at(i);

      lowerBoundPrevious = lowerBound;

      // To find the upper bound of this point, check for the next point not modulated in x-direction.
      // This upper bound can be reused until the point that marks the upper bound has been drawn.
      if (point.getPosX() >= upperBound)
      {
        for (int j = i; j < editor->shapePoints.size(); j++)
        {
          if (!editor->shapePoints.at(j).posX.isModulated())
          {
            upperBound = editor->shapePoints.at(j).getPosX();
            break;
          }
        }
      }

      // The position of this point marks the lower bound of the next.
      float lowerBoundNext = point.getPosX(modulationAmplitudes, lowerBound, upperBound);
      float posX = point.getAbsPosX(modulationAmplitudes, lowerBound, upperBound);
      float posY = point.getAbsPosY(modulationAmplitudes);
      float curveCenterPosX = (posX + editor->shapePoints.at(i - 1).getAbsPosX(modulationAmplitudes, lowerBound, upperBound)) / 2;
      float curveCenterPosY = point.getCurveCenterAbsPosY(editor->shapePoints.at(i - 1).getAbsPosY(modulationAmplitudes), modulationAmplitudes);
      g.FillCircle(UDS_WHITE, posX, posY, POINT_SIZE);
      g.FillCircle(UDS_WHITE, curveCenterPosX, curveCenterPosY, POINT_SIZE_SMALL);

      // Add a ring around points if the user tries to connect a modulation link.
      if (LFOConnectIdx != -1)
      {
        // Check if this LFO has been connected to both the x- and y-position of this point.
        // If yes, dont highlight it.
        bool connectedX = point.posX.isConnectedToLFO(LFOConnectIdx);
        bool connectedY = point.posY.isConnectedToLFO(LFOConnectIdx);

        // The last point can never be modulated in x-direction. Always treat it as connected.
        connectedX = connectedX || (i == editor->shapePoints.size() - 1);

        if (!(connectedX && connectedY))
        {
          g.DrawCircle(UDS_WHITE, posX, posY, CIRCLE_SIZE, nullptr, CIRCLE_THICKNESS);
        }

        if (!point.curveCenterPosY.isConnectedToLFO(LFOConnectIdx))
        {
          g.DrawCircle(UDS_WHITE, curveCenterPosX, curveCenterPosY, CIRCLE_SIZE_SMALL, nullptr, CIRCLE_THICKNESS_SMALL);
        }
      }

      // If the user hovers over a LinkKnob, highlight the corresponding parameter.
      else if (highlightKnobIdx != -1)
      {
        bool connectedX = point.posX.isConnectedToMod(highlightKnobIdx);
        bool connectedY = point.posY.isConnectedToMod(highlightKnobIdx);
        if (connectedX || connectedY)
        {
          g.DrawCircle(UDS_WHITE, posX, posY, CIRCLE_SIZE, nullptr, CIRCLE_THICKNESS);
        }
        else if (point.curveCenterPosY.isConnectedToMod(highlightKnobIdx))
        {
          g.DrawCircle(UDS_WHITE, curveCenterPosX, curveCenterPosY, CIRCLE_SIZE_SMALL, nullptr, CIRCLE_THICKNESS_SMALL);
        }
      }

      lowerBound = lowerBoundNext;
    }
  };

  if (mUseLayer)
  {
    if (!g.CheckLayer(mLayer))
    {
      g.StartLayer(this, mRECT);
      drawFunc();
      mLayer = g.EndLayer();
    }

    g.DrawLayer(mLayer, &mBlend);
  }
  else
    drawFunc();
}

void ShapeEditorControl::OnResize()
{
  SetTargetRECT(MakeRects(mRECT));
  SetDirty(false);
}

void ShapeEditorControl::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  if (mod.L)
  {
    editor->processLeftClick(x, y);

    // Hide the mouse cursor if the user starts dragging the curve center.
    if (editor->currentEditMode == editMode::curveCenter)
    {
      GetUI()->HideMouseCursor(true, true);
    }
  }
  if (mod.R)
  {
    if (editor->processRightClick(x, y))
    {
      GetUI()->CreatePopupMenu(*this, menu, IRECT(x, y, x, y));
    }
  }
  SetDirty(true);
}

void ShapeEditorControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  // If the curve center is edited, the mouse is locked in place by HideMouseCursor.
  // Collect the dragging offset to forward to the ShapeEditor.
  // Do not do that if the maximum curve center position has been reached, else
  // the user will have to drag all the way back without effect.
  bool editingCurveCenter = (editor->currentEditMode == editMode::curveCenter);
  bool maxReached = editor->isDraggingBeyondBounds();
  if (editingCurveCenter && !maxReached)
  {
    cumulativeDragOffset += dY;
  }

  editor->processMouseDrag(x, y + cumulativeDragOffset);
  SetDirty(true);
}

void ShapeEditorControl::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  bool revealCursor = false;
  float targetX = x;
  float targetY = y;

  // Check if the editor has hidden the cursor and must show it again.
  editor->getMouseReveal(revealCursor, targetX, targetY);

  if (revealCursor)
  {
    GetUI()->HideMouseCursor(false, false);
    GetUI()->MoveMouseCursor(targetX, targetY);
  }

  cumulativeDragOffset = 0.f;
}

void ShapeEditorControl::OnMouseDblClick(float x, float y, const IMouseMod& mod)
{
  if (mod.L)
  {
    editor->processDoubleClick(x, y);
    SetDirty(true);
  }
}

void ShapeEditorControl::OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx)
{
  if (pSelectedMenu == &menu)
  {
    int item = pSelectedMenu->GetChosenItemIdx();
    if (item == 0)
    {
      editor->deleteSelectedPoint();

      int numberLinks;
      int* pData;
      editor->getDeletedLinks(numberLinks, pData);

      GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::editorPointDeleted, -1, numberLinks, pData);
    }
    else if (item == 2)
    {
      editor->setInterpolationMode(shapePower);
    }
    else if (item == 3)
    {
      editor->setInterpolationMode(shapeSine);
    }
  }
  else if (pSelectedMenu == &menuMod)
  {
    // modPoint and modIdx are always set before menuMod is opened, so it can be assume
    // that they have sensible values in this scope, see ShapeEditorControl::OnMsgFromDelegate.

    int item = pSelectedMenu->GetChosenItemIdx();
    if (item == 0)
    {
      if (editor->shapePoints.at(modPointIdx).posX.addModulator(modIdx))
      {
        GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOConnectSuccess, GetTag(), sizeof(modIdx), &modIdx);
      }
    }
    else if (item == 1)
    {
      if (editor->shapePoints.at(modPointIdx).posY.addModulator(modIdx))
      {
        GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOConnectSuccess, GetTag(), sizeof(modIdx), &modIdx);
      }
    }
  }
  else if (!pSelectedMenu)
  {
    // Reset temporary variables concerning menuMod.
    modPointIdx = -1;
    modIdx = -1;
  }
}

void ShapeEditorControl::OnMsgFromDelegate(int msgTag, int dataSize, const void* pData)
{
  if (msgTag == EControlMsg::LFOConnectInit)
  {
    LFOConnectIdx = *static_cast<const int*>(pData);
    SetDirty(false);
  }

  // An LFO tries to connect to a ModulatedParameter.
  else if (msgTag == EControlMsg::LFOConnectAttempt)
  {
    LFOConnectInfo info = *static_cast<const LFOConnectInfo*>(pData);
    int closestPointIdx = editor->getClosestPoint(info.x, info.y);

    if (closestPointIdx != -1)
    {
      if (editor->currentEditMode == curveCenter)
      {
        // If the connect was successfull send a response message to update the LFO UI.
        if (editor->shapePoints.at(closestPointIdx).curveCenterPosY.addModulator(info.idx))
        {
          GetUI()->GetDelegate()->SendArbitraryMsgFromUI(EControlMsg::LFOConnectSuccess, GetTag(), sizeof(info.idx), &info.idx);
        }
      }
      else
      {
        // Store point and mod index to later access it when the menu selection is
        // processed.
        modPointIdx = closestPointIdx;
        modIdx = info.idx;

        // Clear the menu and add only the entries that are allowed in the current state.
        menuMod.Clear(false);

        // Check if x- and y-position are already connected to the LFO. Disable menu
        // option if they are.
        int xFlags = IPopupMenu::Item::Flags::kNoFlags;
        int yFlags = IPopupMenu::Item::Flags::kNoFlags;

        int LFOIdx = (info.idx - info.idx % MAX_MODULATION_LINKS) / MAX_MODULATION_LINKS;
        bool xConnected = editor->shapePoints.at(closestPointIdx).posX.isConnectedToLFO(LFOIdx);
        bool yConnected = editor->shapePoints.at(closestPointIdx).posY.isConnectedToLFO(LFOIdx);

        // For x-position additionally check if the corresponding point is the last one,
        // which can not be modulated in x-direction.
        if (xConnected || (closestPointIdx == editor->shapePoints.size() - 1))
        {
          xFlags = IPopupMenu::Item::Flags::kDisabled;
        }
        if (yConnected)
        {
          yFlags = IPopupMenu::Item::Flags::kDisabled;
        }

        menuMod.AddItem("X", -1, xFlags);
        menuMod.AddItem("Y", -1, yFlags);

        GetUI()->CreatePopupMenu(*this, menuMod, IRECT(info.x, info.y, info.x, info.y));
      }
    }

    LFOConnectIdx = -1;
    SetDirty(false);
  }

  // If the cursor entered a LinkKnob, highlight the point corresponding to it.
  else if (msgTag == EControlMsg::linkKnobMouseOver)
  {
    highlightKnobIdx = *static_cast<const int*>(pData);
    SetDirty(false);
  }

  else if (msgTag == EControlMsg::linkKnobMouseOut)
  {
    highlightKnobIdx = -1;
    SetDirty(false);
  }
}